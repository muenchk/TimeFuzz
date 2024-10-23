#include "SessionFunctions.h"
#include "Session.h"
#include "Data.h"
#include "SessionData.h"
#include "Test.h"
#include "LuaEngine.h"

#include <mutex>
#include <boost/circular_buffer.hpp>
#include <time.h>
#include <algorithm>
#include <numeric>

std::shared_ptr<Input> SessionFunctions::GenerateInput(std::shared_ptr<Session>& session)
{
	loginfo("[GenerateInput] create form");
	// generate a new input
	std::shared_ptr<Input> input = session->data->CreateForm<Input>();
	loginfo("[GenerateInput] generate");
	session->_generator->Generate(input);
	loginfo("[GenerateInput] setup");
	// setup input class
	input->hasfinished = false;
	input->trimmed = false;
	input->executiontime = std::chrono::nanoseconds(0);
	input->exitcode = 0;
	input->pythonstring = "";
	input->pythonconverted = false;
	input->stringrep = "";
	input->oracleResult = Oracle::OracleResult::None;

	return input;
}

void SessionFunctions::SaveCheck(std::shared_ptr<Session>& session)
{
	// check whether to save at all
	if (!session->_settings->general.enablesaves)
		return;
	static auto autosavetime = std::chrono::steady_clock::now();
	static uint64_t testnum = SessionStatistics::TestsExecuted(session);
	static std::mutex savelock;
	bool save = false;
	{
		std::unique_lock<std::mutex> guard(savelock);
		// check whether we should save now
		if (session->_settings->general.autosave_every_seconds > 0 && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - autosavetime).count() >= session->_settings->general.autosave_every_seconds) {
			// set new last save-time
			autosavetime = std::chrono::steady_clock::now();
			save = true;
		} 
		if (!save) {
			auto tn = SessionStatistics::TestsExecuted(session);
			if (session->_settings->general.autosave_every_tests > 0 && (int64_t)(tn - testnum) > session->_settings->general.autosave_every_tests) {  // set neew last test count
				testnum = tn;
				save = true;
			}
		}
	}

	if (!save)
		return;

	// perform the save
	std::thread(SaveSession_Async, session).detach();
}

void SessionFunctions::SaveSession_Async(std::shared_ptr<Session> session)
{
	loginfo("Master Save Session");
	// async function that simply initiates the save
	// we are using this to free us from a potential deadlock in the TaskController
	// if the save is orchestrated by a callback within the TaskController
	// if we would allow it, the saving function that stops the taskcontroller temporarily
	// would deadlock itself
	session->data->Save();
}

bool SessionFunctions::EndCheck(std::shared_ptr<Session>& session)
{
	// check whether end conditions are met
	bool end = false;
	if (session->_settings->conditions.use_foundnegatives && SessionStatistics::NegativeTestsGenerated(session) >= session->_settings->conditions.foundnegatives)
		end = true;
	else if (session->_settings->conditions.use_foundpositives && SessionStatistics::PositiveTestsGenerated(session) >= session->_settings->conditions.foundpositives)
		end = true;
	else if (session->_settings->conditions.use_timeout && std::chrono::duration_cast<std::chrono::seconds>(SessionStatistics::Runtime(session)).count() >= session->_settings->conditions.timeout)
		end = true;
	else if (session->_settings->conditions.use_overalltests && SessionStatistics::TestsExecuted(session) >= session->_settings->conditions.overalltests)
		end = true;

	// check whether the recent input generation is converging and we should abort the program 
	if (session->sessiondata._generatedinputs < (int64_t)session->sessiondata.GENERATION_WEIGHT_BUFFER_SIZE) {
		if ((double)std::accumulate(session->sessiondata._recentfailes.begin(), session->sessiondata._recentfailes.end(), 0) / (double)session->sessiondata._generatedinputs >= session->sessiondata.GENERATION_WEIGHT_LIMIT)
			end = true;
	} else if ((double)std::accumulate(session->sessiondata._recentfailes.begin(), session->sessiondata._recentfailes.end(), 0) / (double)session->sessiondata.GENERATION_WEIGHT_BUFFER_SIZE >= session->sessiondata.GENERATION_WEIGHT_LIMIT)
		end = true;
	
	// if we don't end the session, do nothing
	if (end == false)
		return false;

	// start thread that clears the session
	std::thread(EndSession_Async, session).detach();
	return true;
}

void SessionFunctions::EndSession_Async(std::shared_ptr<Session> session)
{
	loginfo("Master End Session");
	// async function that ends the session itself

	// force save
	session->data->Save();

	// end the session

	// set abort flag to catch all threads that try to escape
	session->abort = true;
	session->StopSession(false);
}


void SessionFunctions::MasterControl(std::shared_ptr<Session>& session, bool forceexecute)
{
	loginfo("[Mastercontrol] begin");
	// try to get lock for master
	if (forceexecute) // forcefully execute function
		session->sessiondata._sessionFunctionLock.acquire();
	else if (!session->sessiondata._sessionFunctionLock.try_acquire())
	{
		// if we cannot get the lock, just skip it and do own stuff
		// this may lead to new tests not being generated
		return;
	}

	bool forceendcheck = false;
	std::chrono::nanoseconds timediff = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - session->sessiondata.lastchecks);

MasterControlBegin:

	loginfo("[MasterControl] EndCheck");

	// check whether we fulfilled our goals and can end the session
	if (forceendcheck || timediff >= session->sessiondata._checkperiod)
		if (EndCheck(session) == true) {
			session->sessiondata._sessionFunctionLock.release();
			return;
		}

	loginfo("[MasterControl] generation");

	// check whether we need to generate new inputs
	// the goal is to always a certain number of tests prepared waiting for execution. 
	// Normally this can simply be achieved by adding a new test for each one that finishes,
	// but since stuff can happen and it doesn't cost much, we will check the number of
	// waiting tests against the parameters

	// number of new tests we need to generate
	size_t generate = session->_settings->generation.generationsize - session->_exechandler->WaitingTasks();

	int32_t generated = 0;

	std::shared_ptr<Input> input;
	std::vector<std::shared_ptr<Input>> inputs;
	for (int64_t i = 0; i < (int64_t)generate; i++) {
		loginfo("[MasterControl] outer loop");
		int64_t count = 0;
		// try to generate new inputs for a maximum of 1000 times
		// this should be high enough so that we can generate enough inputs to find some
		// that aren't excluded yet, while still making sure we don't take forever
		while (count < (int64_t)session->sessiondata.GENERATION_RETRIES) {
			loginfo("[MasterControl] inner loop");
			count++;
			input = GenerateInput(session);
			loginfo("[MasterControl] prefix tree");
			if (session->_excltree->HasPrefix(input) == false) {
				// we found one that isn't a prefix
				session->sessiondata._generatedinputs++;
				inputs.push_back(input);
				generated++;
				break;
			}
			// else continue with loop
			else
				session->sessiondata._generatedWithPrefix++;
			loginfo("[MasterControl] inner loop end");
		}
		// if count is GENERATION_RETRIES update fails
		if (count >= (int64_t)session->sessiondata.GENERATION_RETRIES)
		{
			session->sessiondata._generationfails++;
			session->sessiondata._recentfailes.push_back(1);
		} else
			session->sessiondata._recentfailes.push_back(0);
	}

	if (generated == 0 && generate == (size_t)session->_settings->generation.generationsize)
	{
		// if we could not generate any new inputs and the number we needed to generate is equal to the maximum generation size
		// we may be in a state where we cannot progress without a new test if there are no tests left currently executed
		// or waiting to finish
		// we will go back to the beginning of the function and retry, we will do so with forced ending checks until we either generate
		// new inputs or the weight of failed generations becomes too large and the session is forcibly ended.

		loginfo("[MasterControl] repeat");
		forceendcheck = true;
		goto MasterControlBegin;
	}
	else {
		loginfo("[MasterControl] add test");
		// add tests to execution handler
		for (auto inp : inputs) {
			auto callback = new Functions::TestCallback();
			callback->_session = session;
			callback->_input = inp;
			session->_exechandler->AddTest(inp, callback);
		}
	}

	loginfo("[MasterControl] SaveCheck");

	// check whether we should save
	SaveCheck(session);

	/*  Data management?
	* check how much memory we are using, if too much:
	* delete some inputs from our memory of failed inputs
	* trim the exclusion tree for stray inputs (those that are very deep pathes and aren't often reached
	* ...
	*/



	loginfo("[Mastercontrol] end");
	session->sessiondata._sessionFunctionLock.release();
	//throw new std::runtime_error("not implemented");
}

void SessionFunctions::TestEnd(std::shared_ptr<Session>& session, std::shared_ptr<Input>& input)
{
	// calculate oracle result
	bool stateerror = false;
	input->oracleResult = Lua::EvaluateOracle(std::bind(&Oracle::Evaluate, session->_oracle, std::placeholders::_1, std::placeholders::_2), input->test, stateerror);
	if (stateerror) {
		logcritical("Test End functions cannot be completed, as the calling thread lacks a lua context");
		return;
	}
	// check whether output should be stored
	input->test->storeoutput = session->_settings->tests.storePUToutput || (session->_settings->tests.storePUToutputSuccessful && input->GetOracleResult() == Oracle::OracleResult::Passing);

	// check input result
	switch (input->GetOracleResult()) {
	case Oracle::OracleResult::Failing:
		{
		// -----We have a failing input-----
			session->sessiondata._negativeInputNumbers++;



		}
		break;
	case Oracle::OracleResult::Passing:
		{
			// -----We have a passing input-----
			session->sessiondata._positiveInputNumbers++;

			// -----Calculate initial weight for backtracking-----
			double weight = 0.0f;




			// -----Add input to its list-----
			session->sessiondata.AddInput(input, Oracle::OracleResult::Passing, weight);



		}
		break;
	case Oracle::OracleResult::Prefix:
		{
			// -----The input has a prefix that has already been decided-----
			session->sessiondata.AddInput(input, Oracle::OracleResult::Prefix);
			// inputs which's value is determined by a prefix cannot get here, as they 
			// are caught on generation, so everything here is just for show
		}
		break;
	case Oracle::OracleResult::Unfinished:
		{
			// -----We have an unfinished input-----
			session->sessiondata._unfinishedInputNumbers++;

			// -----Calculate initial weight for expansion-----
			double weight = 0.0f;



			// -----Add input to its list-----
			session->sessiondata.AddInput(input, Oracle::OracleResult::Passing, weight);



		}
		break;
	case Oracle::OracleResult::Undefined:
		{
			// -----We have an undefined result-----
			session->sessiondata._undefinedInputNumbers++;
			session->sessiondata.AddInput(input, Oracle::OracleResult::Undefined);
			// we don't really have anything else to do, since we cannot really use
			// tests with an undefined result as we simply do not know what it is
		}
		break;
	}


	// ----- DO SOME NICE STUFF -----
}

uint64_t SessionStatistics::TestsExecuted(std::shared_ptr<Session>& session)
{
	return (uint64_t)(session->sessiondata._negativeInputNumbers + session->sessiondata._positiveInputNumbers + session->sessiondata._undefinedInputNumbers + session->sessiondata._unfinishedInputNumbers);
}

uint64_t SessionStatistics::PositiveTestsGenerated(std::shared_ptr<Session>& session)
{
	return (uint64_t)session->sessiondata._positiveInputNumbers;
}

uint64_t SessionStatistics::NegativeTestsGenerated(std::shared_ptr<Session>& session)
{
	return (uint64_t)session->sessiondata._negativeInputNumbers;
}

uint64_t SessionStatistics::TestsPruned(std::shared_ptr<Session>&)
{
	return 0;
}

std::chrono::nanoseconds SessionStatistics::Runtime(std::shared_ptr<Session>& session)
{
	return session->data->GetRuntime();
}
