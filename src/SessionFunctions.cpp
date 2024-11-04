#include "SessionFunctions.h"
#include "Session.h"
#include "Data.h"
#include "SessionData.h"
#include "Test.h"
#include "LuaEngine.h"
#include "DerivationTree.h"
#include "Processes.h"

#include <mutex>
#include <boost/circular_buffer.hpp>
#include <time.h>
#include <algorithm>
#include <numeric>

std::shared_ptr<Input> SessionFunctions::GenerateInput(std::shared_ptr<SessionData>& sessiondata)
{
	loginfo("[GenerateInput] create form");
	// generate a new input
	std::shared_ptr<Input> input = sessiondata->data->CreateForm<Input>();
	input->derive = sessiondata->data->CreateForm<DerivationTree>();
	loginfo("[GenerateInput] generate");
	if (sessiondata->_generator->Generate(input) == false)
	{
		sessiondata->data->DeleteForm(input->derive);
		input->derive.reset();
		sessiondata->data->DeleteForm(input);
		return {};
	}
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

void SessionFunctions::GenerateInput(std::shared_ptr<Input>& input, std::shared_ptr<SessionData>& sessiondata)
{
	// if we already generated the input: skip
	if (input->GetGenerated() || !sessiondata->_generator)
		return;
	if (input) {
		// generate a new input
		if (!input->derive)
			input->derive = sessiondata->data->CreateForm<DerivationTree>();
		loginfo("[GenerateInput] generate");
		if (sessiondata->_generator->GetGrammar() && sessiondata->_generator->GetGrammar()->GetFormID() == input->derive->grammarID) {
			if (sessiondata->_generator->Generate(input) == false) {
				sessiondata->data->DeleteForm(input->derive);
				input->derive.reset();
				sessiondata->data->DeleteForm(input);
			}
		} else {
			auto grammar = sessiondata->data->LookupFormID<Grammar>(input->derive->grammarID);
			if (grammar) {
				sessiondata->_generator->Generate(input, grammar);
				return;
			}
		}
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
	} else
		input = GenerateInput(sessiondata);
}

void SessionFunctions::SaveCheck(std::shared_ptr<SessionData>& sessiondata)
{
	// check whether to save at all
	if (!sessiondata->_settings->saves.enablesaves)
		return;
	static auto autosavetime = std::chrono::steady_clock::now();
	static uint64_t testnum = SessionStatistics::TestsExecuted(sessiondata);
	static std::mutex savelock;
	bool save = false;
	{
		std::unique_lock<std::mutex> guard(savelock);
		// check whether we should save now
		if (sessiondata->_settings->saves.autosave_every_seconds > 0 && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - autosavetime).count() >= sessiondata->_settings->saves.autosave_every_seconds) {
			// set new last save-time
			autosavetime = std::chrono::steady_clock::now();
			save = true;
		}
		if (!save) {
			auto tn = SessionStatistics::TestsExecuted(sessiondata);
			if (sessiondata->_settings->saves.autosave_every_tests > 0 && (int64_t)(tn - testnum) > sessiondata->_settings->saves.autosave_every_tests) {  // set neew last test count
				testnum = tn;
				save = true;
			}
		}
	}

	if (!save)
		return;

	// perform the save
	std::thread(SaveSession_Async, sessiondata).detach();
}

void SessionFunctions::SaveSession_Async(std::shared_ptr<SessionData> sessiondata)
{
	loginfo("Master Save Session");
	// async function that simply initiates the save
	// we are using this to free us from a potential deadlock in the TaskController
	// if the save is orchestrated by a callback within the TaskController
	// if we would allow it, the saving function that stops the taskcontroller temporarily
	// would deadlock itself
	sessiondata->data->Save();
}

bool SessionFunctions::EndCheck(std::shared_ptr<SessionData>& sessiondata)
{
	// check whether end conditions are met
	bool end = false;
	if (sessiondata->_settings->conditions.use_foundnegatives && SessionStatistics::NegativeTestsGenerated(sessiondata) >= sessiondata->_settings->conditions.foundnegatives)
		end = true;
	else if (sessiondata->_settings->conditions.use_foundpositives && SessionStatistics::PositiveTestsGenerated(sessiondata) >= sessiondata->_settings->conditions.foundpositives)
		end = true;
	else if (sessiondata->_settings->conditions.use_timeout && std::chrono::duration_cast<std::chrono::seconds>(SessionStatistics::Runtime(sessiondata)).count() >= sessiondata->_settings->conditions.timeout)
		end = true;
	else if (sessiondata->_settings->conditions.use_overalltests && SessionStatistics::TestsExecuted(sessiondata) >= sessiondata->_settings->conditions.overalltests)
		end = true;

	double failureRate = 0.0f;

	// check whether the recent input generation is converging and we should abort the program 
	if (sessiondata->_generatedinputs < (int64_t)sessiondata->GENERATION_WEIGHT_BUFFER_SIZE) {
		failureRate = (double)std::accumulate(sessiondata->_recentfailes.begin(), sessiondata->_recentfailes.end(), 0) / (double)sessiondata->_generatedinputs;
		if (failureRate >= sessiondata->GENERATION_WEIGHT_LIMIT)
			end = true;
	}
	else {
		failureRate = (double)std::accumulate(sessiondata->_recentfailes.begin(), sessiondata->_recentfailes.end(), 0) / (double)sessiondata->GENERATION_WEIGHT_BUFFER_SIZE;
		if (failureRate >= sessiondata->GENERATION_WEIGHT_LIMIT)
			end = true;
	}

	sessiondata->_failureRate = failureRate;

	// if we don't end the session, do nothing
	if (end == false)
		return false;

	// start thread that clears the session
	std::thread(EndSession_Async, sessiondata).detach();
	return true;
}

void SessionFunctions::EndSession_Async(std::shared_ptr<SessionData> sessiondata)
{
	loginfo("Master End Session");
	auto session = sessiondata->data->CreateForm<Session>();
	if (session->abort)
		return;
	// async function that ends the session itself

	// force save
	session->data->Save();

	// end the session

	// set abort flag to catch all threads that try to escape
	session->abort = true;
	session->StopSession(false);
}

Data::VisitAction HandleInput(std::shared_ptr<Form> form)
{
	if (auto input = form->As<Input>(); input != nullptr) {
		input->FreeMemory();
	}
	return Data::VisitAction::None;
}

void SessionFunctions::ReclaimMemory(std::shared_ptr<SessionData>& sessiondata)
{
	StartProfiling;
	sessiondata->lastmemorysweep = std::chrono::steady_clock::now();
	sessiondata->data->Visit(HandleInput);

#if defined(unix) || defined(__unix__) || defined(__unix)
	uint64_t mem = Processes::GetProcessMemory(getpid());
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	uint64_t mem = Processes::GetProcessMemory(GetCurrentProcess());
#endif
	mem = mem / 1048576;
	profile(TimeProfiling, "Freed {} MB", sessiondata->memory_mem - mem);
	sessiondata->memory_mem = mem;
}

void SessionFunctions::MasterControl(std::shared_ptr<SessionData>& sessiondata, bool forceexecute)
{
	loginfo("[Mastercontrol] begin");
	// try to get lock for master
	if (forceexecute) // forcefully execute function
		sessiondata->_sessionFunctionLock.acquire();
	else if (!sessiondata->_sessionFunctionLock.try_acquire())
	{
		// if we cannot get the lock, just skip it and do own stuff
		// this may lead to new tests not being generated
		return;
	}

	StartProfiling;

	std::chrono::nanoseconds timediff = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - sessiondata->lastchecks);

	loginfo("[MasterControl] EndCheck");

	// check whether we fulfilled our goals and can end the session
	if (timediff >= sessiondata->_checkperiod) {
		if (EndCheck(sessiondata) == true) {
			sessiondata->_sessionFunctionLock.release();
			profile(TimeProfiling, "Time taken for MasterControl");
			return;
		}
	}

	loginfo("[MasterControl] SaveCheck");

	// check whether we should save
	SaveCheck(sessiondata);

	/*  Data management?
	* check how much memory we are using, if too much:
	* delete some inputs from our memory of failed inputs
	* trim the exclusion tree for stray inputs (those that are very deep pathes and aren't often reached
	* ...
	*/

#if defined(unix) || defined(__unix__) || defined(__unix)
	sessiondata->memory_mem = Processes::GetProcessMemory(getpid());
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	sessiondata->memory_mem = Processes::GetProcessMemory(GetCurrentProcess());
#endif
	sessiondata->memory_mem = sessiondata->memory_mem / 1048576;

	if (sessiondata->memory_outofmemory == true)
	{
		if (sessiondata->memory_ending == false) {
			// we are still not ending so check the timer
			if (sessiondata->memory_outofmemory_timer < std::chrono::steady_clock::now())
			{
				// we have exceeded the timer
				// if we have more memory consumption than the limit, exit
				if ((int64_t)sessiondata->memory_mem > sessiondata->_settings->general.memory_limit) {
					sessiondata->memory_ending = true;
					std::thread(EndSession_Async, sessiondata).detach();
				} else
					sessiondata->memory_outofmemory = false;
			}
		}
	} else {
		if ((int64_t)sessiondata->memory_mem > sessiondata->_settings->general.memory_softlimit) {
			if ((int64_t)sessiondata->memory_mem > sessiondata->_settings->general.memory_limit || std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - sessiondata->lastmemorysweep) > sessiondata->_settings->general.memory_sweep_period) {
				// free memory
				ReclaimMemory(sessiondata);
				// if we were above the memory limit set the outofmemory flag, and a timer
				// if we are still above the limit when the timer ends, save and end the session
				// (even though sving might consume a bit more memory)
				if ((int64_t)sessiondata->memory_mem > sessiondata->_settings->general.memory_limit) {
					sessiondata->memory_outofmemory = true;
					sessiondata->memory_outofmemory_timer = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
				}
			}
		}
	}

	loginfo("[Mastercontrol] end");
	sessiondata->_sessionFunctionLock.release();

	profile(TimeProfiling, "Time taken for MasterControl");
	return;
}

void SessionFunctions::TestEnd(std::shared_ptr<SessionData>& sessiondata, std::shared_ptr<Input> input, bool replay)
{
	if (replay == true)
	{
		// if this was only to replay, we just delete it
		sessiondata->data->DeleteForm(input->test);
		sessiondata->data->DeleteForm(input);
	}
	// calculate oracle result
	bool stateerror = false;
	input->oracleResult = Lua::EvaluateOracle(std::bind(&Oracle::Evaluate, sessiondata->_oracle, std::placeholders::_1, std::placeholders::_2), input->test, stateerror);
	if (stateerror) {
		logcritical("Test End functions cannot be completed, as the calling thread lacks a lua context");
		return;
	}
	// check whether output should be stored
	input->test->storeoutput = sessiondata->_settings->tests.storePUToutput || (sessiondata->_settings->tests.storePUToutputSuccessful && input->GetOracleResult() == Oracle::OracleResult::Passing);

	// check input result
	switch (input->GetOracleResult()) {
	case Oracle::OracleResult::Failing:
		{
		// -----We have a failing input-----
			sessiondata->_negativeInputNumbers++;

			// -----Calculate initial weight for backtracking-----
			double weight = 0.0f;

			// -----Add input to its list-----
			sessiondata->AddInput(input, Oracle::OracleResult::Failing, weight);
			
			// -----Add input to exclusion tree as result is fixed-----
			sessiondata->_excltree->AddInput(input);



		}
		break;
	case Oracle::OracleResult::Passing:
		{
			// -----We have a passing input-----
			sessiondata->_positiveInputNumbers++;

			// -----Add input to its list-----
			sessiondata->AddInput(input, Oracle::OracleResult::Passing);

			// -----Add input to exclusion tree as result is fixed-----
			sessiondata->_excltree->AddInput(input);



		}
		break;
	case Oracle::OracleResult::Prefix:
		{
			// -----The input has a prefix that has already been decided-----
			sessiondata->AddInput(input, Oracle::OracleResult::Prefix);
			// inputs which's value is determined by a prefix cannot get here, as they 
			// are caught on generation, so everything here is just for show
		}
		break;
	case Oracle::OracleResult::Unfinished:
		{
			// -----We have an unfinished input-----
			sessiondata->_unfinishedInputNumbers++;

			// -----Calculate initial weight for expansion-----
			double weight = 0.0f;



			// -----Add input to its list-----
			sessiondata->AddInput(input, Oracle::OracleResult::Unfinished, weight);



		}
		break;
	case Oracle::OracleResult::Undefined:
		{
			// -----We have an undefined result-----
			sessiondata->_undefinedInputNumbers++;
			sessiondata->AddInput(input, Oracle::OracleResult::Undefined);
			// we don't really have anything else to do, since we cannot really use
			// tests with an undefined result as we simply do not know what it is
		}
		break;
	}


	// ----- DO SOME NICE STUFF -----
}

void SessionFunctions::AddTestExitReason(std::shared_ptr<SessionData>& sessiondata, int32_t reason)
{
	switch (reason) {
	case Test::ExitReason::Natural:
		sessiondata->exitstats.natural++;
		break;
	case Test::ExitReason::LastInput:
		sessiondata->exitstats.lastinput++;
		break;
	case Test::ExitReason::Terminated:
		sessiondata->exitstats.terminated++;
		break;
	case Test::ExitReason::Timeout:
		sessiondata->exitstats.timeout++;
		break;
	case Test::ExitReason::FragmentTimeout:
		sessiondata->exitstats.fragmenttimeout++;
		break;
	case Test::ExitReason::Memory:
		sessiondata->exitstats.memory++;
		break;
	case Test::ExitReason::InitError:
		sessiondata->exitstats.initerror++;
		break;
	}
}

void SessionFunctions::GenerateTests(std::shared_ptr<SessionData>& sessiondata)
{
	auto callback = dynamic_pointer_cast<Functions::MasterGenerationCallback>(Functions::MasterGenerationCallback::Create());
	callback->_sessiondata = sessiondata;
	sessiondata->_controller->AddTask(callback);
}

uint64_t SessionStatistics::TestsExecuted(std::shared_ptr<SessionData>& sessiondata)
{
	return (uint64_t)(sessiondata->_negativeInputNumbers + sessiondata->_positiveInputNumbers + /*sessiondata->_undefinedInputNumbers +*/ sessiondata->_unfinishedInputNumbers);
}

uint64_t SessionStatistics::PositiveTestsGenerated(std::shared_ptr<SessionData>& sessiondata)
{
	return (uint64_t)sessiondata->_positiveInputNumbers;
}

uint64_t SessionStatistics::NegativeTestsGenerated(std::shared_ptr<SessionData>& sessiondata)
{
	return (uint64_t)sessiondata->_negativeInputNumbers;
}

uint64_t SessionStatistics::UnfinishedTestsGenerated(std::shared_ptr<SessionData>& sessiondata)
{
	return (uint64_t)sessiondata->_unfinishedInputNumbers;
}

uint64_t SessionStatistics::TestsPruned(std::shared_ptr<SessionData>&)
{
	return 0;
}

std::chrono::nanoseconds SessionStatistics::Runtime(std::shared_ptr<SessionData>& sessiondata)
{
	return sessiondata->data->GetRuntime();
}



namespace Functions
{

	void MasterGenerationCallback::Run()
	{
		StartProfiling;
		logdebug("[MasterGenerationCallback] generation");

		while (true) {
			// check whether we need to generate new inputs
			// the goal is to always a certain number of tests prepared waiting for execution.
			// Normally this can simply be achieved by adding a new test for each one that finishes,
			// but since stuff can happen and it doesn't cost much, we will check the number of
			// waiting tests against the parameters

			// number of new tests we need to generate
			int64_t generate = _sessiondata->_settings->generation.generationsize - (int64_t)_sessiondata->_exechandler->WaitingTasks();
			if (generate == 0)
				return;

			int32_t generated = 0;

			std::shared_ptr<Input> input;
			//std::vector<std::shared_ptr<Input>> inputs;

			int64_t failcount = 0;
			int64_t gencount = 0;

			// generate new inputs while we have too few inputs waiting for execution
			// break the loop if we have accumulated too many failures
			// try to generate new inputs for a maximum of 1000 times
			// this should be high enough so that we can generate enough inputs to find some
			// that aren't excluded yet, while still making sure we don't take forever
			// also stop generation is we have generated [generationsize] passing inputs, as new tests
			// might have finished in the meantime and we don't want this to become an endless loop
			// blocking our task executions
			while (_sessiondata->_settings->generation.generationsize > (int64_t)_sessiondata->_exechandler->WaitingTasks() && failcount < (int64_t)_sessiondata->GENERATION_RETRIES && gencount < _sessiondata->_settings->generation.generationstep) {
				loginfo("[MasterGenerationCallback] inner loop");
				input = SessionFunctions::GenerateInput(_sessiondata);
				if (input) {
					logdebug("[MasterGenerationCallback] prefix tree");
					if (_sessiondata->_excltree->HasPrefix(input) == false) {
						gencount++;
						// we found one that isn't a prefix
						_sessiondata->IncGeneratedInputs();
						//inputs.push_back(input);
						logdebug("[MasterGenerationCallback] add test");
						// add tests to execution handler
						auto callback = dynamic_pointer_cast<Functions::TestCallback>(Functions::TestCallback::Create());
						callback->_sessiondata = _sessiondata;
						callback->_input = input;
						if (_sessiondata->_exechandler->AddTest(input, callback) == false) {
							_sessiondata->IncAddTestFails();
							_sessiondata->data->DeleteForm(input);
							callback->Dispose();
						}
						generated++;
					}
					// else continue with loop
					else {
						_sessiondata->IncGeneratedWithPrefix();
						failcount++;
					}
				}
				logdebug("[MasterGenerationCallback] inner loop end");
			}

			// if count is GENERATION_RETRIES update fails
			if (failcount >= (int64_t)_sessiondata->GENERATION_RETRIES) {
				_sessiondata->IncGenerationFails();
				_sessiondata->PushRecenFails(1);
			} else
				_sessiondata->PushRecenFails(0);

			if (generated == 0 && (int64_t)_sessiondata->_exechandler->WaitingTasks() == 0 /*generate == _sessiondata->_settings->generation.generationsize */) {
				// if we could not generate any new inputs and the number we needed to generate is equal to the maximum generation size (i.e. we don't have any tasks waiting)
				// we may be in a state where we cannot progress without a new test if there are no tests left currently executed
				// or waiting to finish
				// we will go back to the beginning of the function and retry, we will do so with forced ending checks until we either generate
				// new inputs or the weight of failed generations becomes too large and the session is forcibly ended.

				// perform endcheck to see if we should end, otherwise repeat the generation
				logdebug("[MasterGenerationCallback] EndCheck");

				// check whether we fulfilled our goals and can end the session
				if (SessionFunctions::EndCheck(_sessiondata) == true) {
					profile(TimeProfiling, "Time taken for MasterGenerationCallback");
					return;
				}
			} else {
				return;
			} /* else {
				logdebug("[MasterGenerationCallback] add test");
				// add tests to execution handler
				for (auto inp : inputs) {
					auto callback = dynamic_pointer_cast<Functions::TestCallback>(Functions::TestCallback::Create());
					callback->_sessiondata = _sessiondata;
					callback->_input = inp;
					if (_sessiondata->_exechandler->AddTest(inp, callback) == false) {
						_sessiondata->IncAddTestFails();
						_sessiondata->data->DeleteForm(inp);
						callback->Dispose();
					}
				}
				return;
			}*/
		}
		profile(TimeProfiling, "");
	}

	

	bool MasterGenerationCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
		});
		return true;
	}

	bool MasterGenerationCallback::WriteData(unsigned char* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		return true;
	}

	size_t MasterGenerationCallback::GetLength()
	{
		return BaseFunction::GetLength() + 8;
	}

	void MasterGenerationCallback::Dispose()
	{
		_sessiondata.reset();
	}
}
