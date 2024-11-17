#include "SessionFunctions.h"
#include "Session.h"
#include "Data.h"
#include "SessionData.h"
#include "Test.h"
#include "LuaEngine.h"
#include "DerivationTree.h"
#include "Processes.h"
#include "DeltaDebugging.h"
#include "Generation.h"

#include <mutex>
#include <boost/circular_buffer.hpp>
#include <time.h>
#include <algorithm>
#include <numeric>
#include <random>

std::shared_ptr<Input> SessionFunctions::GenerateInput(std::shared_ptr<SessionData>& sessiondata)
{
	loginfo("[GenerateInput] create form");
	// generate a new _input
	std::shared_ptr<Input> input = sessiondata->data->CreateForm<Input>();
	input->SetFlag(Form::FormFlags::DoNotFree);
	input->derive = sessiondata->data->CreateForm<DerivationTree>();
	input->derive->_inputID = input->GetFormID();
	loginfo("[GenerateInput] generate");
	if (sessiondata->_generator->Generate(input, {}, sessiondata) == false)
	{
		sessiondata->data->DeleteForm(input->derive);
		input->derive.reset();
		sessiondata->data->DeleteForm(input);
		return {};
	}
	loginfo("[GenerateInput] setup");
	// setup _input class
	input->_hasfinished = false;
	input->_trimmed = false;
	input->_executiontime = std::chrono::nanoseconds(0);
	input->_exitcode = 0;
	input->_pythonstring = "";
	input->_pythonconverted = false;
	input->_stringrep = "";
	input->_oracleResult = OracleResult::None;

	return input;
}

std::shared_ptr<Input> SessionFunctions::ExtendInput(std::shared_ptr<SessionData>& sessiondata, std::shared_ptr<Input> parent)
{
	loginfo("[GenerateInput] create form");
	// generate a new _input
	std::shared_ptr<Input> input = sessiondata->data->CreateForm<Input>();
	input->SetFlag(Form::FormFlags::DoNotFree);
	input->derive = sessiondata->data->CreateForm<DerivationTree>();
	input->derive->_inputID = input->GetFormID();
	// flag makes the generator extend the input
	input->SetFlag(Input::Flags::GeneratedGrammarParent);
	if (parent->GetOracleResult() == OracleResult::Failing)
		input->SetFlag(Input::Flags::GeneratedGrammarParentBacktrack);
	loginfo("[GenerateInput] generate");
	if (sessiondata->_generator->Generate(input, {}, sessiondata, parent) == false) {
		sessiondata->data->DeleteForm(input->derive);
		input->derive.reset();
		sessiondata->data->DeleteForm(input);
		return {};
	}
	loginfo("[GenerateInput] setup");
	// setup _input class
	input->_hasfinished = false;
	input->_trimmed = false;
	input->_executiontime = std::chrono::nanoseconds(0);
	input->_exitcode = 0;
	input->_pythonstring = "";
	input->_pythonconverted = false;
	input->_stringrep = "";
	input->_oracleResult = OracleResult::None;

	return input;
}

void SessionFunctions::ExtendInput(std::shared_ptr<Input>& input, std::shared_ptr<SessionData>& sessiondata, std::shared_ptr<Input> parent)
{
	// if we already generated the _input: skip
	if (input->GetGenerated() || !sessiondata->_generator)
		return;
	if (input) {
		// generate a new _input
		if (!input->derive) {
			input->derive = sessiondata->data->CreateForm<DerivationTree>();
			input->derive->_inputID = input->GetFormID();
		}
		// flag makes the generator extend the input
		input->SetFlag(Input::Flags::GeneratedGrammarParent);
		if (input->GetOracleResult() == OracleResult::Failing)
			input->SetFlag(Input::Flags::GeneratedGrammarParentBacktrack);
		loginfo("[GenerateInput] generate");
		if (sessiondata->_generator->GetGrammar() && sessiondata->_generator->GetGrammar()->GetFormID() == input->derive->_grammarID) {
			if (sessiondata->_generator->Generate(input, {}, sessiondata, parent) == false) {
				//sessiondata->data->DeleteForm(input->derive);
				//input->derive.reset();
				//sessiondata->data->DeleteForm(input);
				//input.reset();
			}
		} else {
			auto grammar = sessiondata->data->LookupFormID<Grammar>(input->derive->_grammarID);
			if (grammar) {
				sessiondata->_generator->Generate(input, grammar, sessiondata, parent);
				return;
			}
		}
		loginfo("[GenerateInput] setup");
	} else
		input = ExtendInput(sessiondata, parent);
}

void SessionFunctions::GenerateInput(std::shared_ptr<Input>& input, std::shared_ptr<SessionData>& sessiondata)
{
	// if we already generated the _input: skip
	if (input->GetGenerated() || !sessiondata->_generator)
		return;
	if (input) {
		// generate a new _input
		if (!input->derive) {
			input->derive = sessiondata->data->CreateForm<DerivationTree>();
			input->derive->_inputID = input->GetFormID();
		}
		loginfo("[GenerateInput] generate");
		if (sessiondata->_generator->GetGrammar() && sessiondata->_generator->GetGrammar()->GetFormID() == input->derive->_grammarID) {
			if (sessiondata->_generator->Generate(input, {}, sessiondata) == false) {
				//sessiondata->data->DeleteForm(input->derive);
				//input->derive.reset();
				//sessiondata->data->DeleteForm(input);
				//input.reset();
			}
		} else {
			auto grammar = sessiondata->data->LookupFormID<Grammar>(input->derive->_grammarID);
			if (grammar) {
				sessiondata->_generator->Generate(input, grammar, sessiondata);
				return;
			}
		}
		loginfo("[GenerateInput] setup");
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
	// if the save is orchestrated by a _callback within the TaskController
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

	// check whether the recent _input generation is converging and we should abort the program 
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
	if (session->_abort)
		return;
	// async function that ends the session itself

	// force save
	session->data->Save();

	// end the session

	// set abort flag to catch all threads that try to escape
	session->_abort = true;
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
	sessiondata->_lastMemorySweep = std::chrono::steady_clock::now();
	sessiondata->data->Visit(HandleInput);

#if defined(unix) || defined(__unix__) || defined(__unix)
	uint64_t mem = Processes::GetProcessMemory(getpid());
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	uint64_t mem = Processes::GetProcessMemory(GetCurrentProcess());
#endif
	mem = mem / 1048576;
	profile(TimeProfiling, "Freed {} MB", sessiondata->_memory_mem - mem);
	sessiondata->_memory_mem = mem;
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

	auto now = std::chrono::steady_clock::now();

	std::chrono::nanoseconds timediff = std::chrono::duration_cast<std::chrono::nanoseconds>(now - sessiondata->_lastchecks);

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
	sessiondata->_memory_mem = Processes::GetProcessMemory(getpid());
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	sessiondata->_memory_mem = Processes::GetProcessMemory(GetCurrentProcess());
#endif
	sessiondata->_memory_mem = sessiondata->_memory_mem / 1048576;

	if (sessiondata->_memory_outofmemory == true)
	{
		if (sessiondata->_memory_ending == false) {
			// we are still not ending so check the timer
			if (sessiondata->_memory_outofmemory_timer < now)
			{
				// we have exceeded the timer
				// if we have more memory consumption than the limit, exit
				if ((int64_t)sessiondata->_memory_mem > sessiondata->_settings->general.memory_limit) {
					sessiondata->_memory_ending = true;
					std::thread(EndSession_Async, sessiondata).detach();
				} else
					sessiondata->_memory_outofmemory = false;
			}
		}
	} else {
		if ((int64_t)sessiondata->_memory_mem > sessiondata->_settings->general.memory_softlimit) {
			if ((int64_t)sessiondata->_memory_mem > sessiondata->_settings->general.memory_limit || std::chrono::duration_cast<std::chrono::nanoseconds>(now - sessiondata->_lastMemorySweep) > sessiondata->_settings->general.memory_sweep_period) {
				// free memory
				ReclaimMemory(sessiondata);
				// update now since some time may have passed
				now = std::chrono::steady_clock::now();
				// if we were above the memory limit set the outofmemory flag, and a timer
				// if we are still above the limit when the timer ends, save and end the session
				// (even though sving might consume a bit more memory)
				if ((int64_t)sessiondata->_memory_mem > sessiondata->_settings->general.memory_limit) {
					sessiondata->_memory_outofmemory = true;
					sessiondata->_memory_outofmemory_timer = now + std::chrono::milliseconds(100);
				}
			}
		}
	}

	// cleanup session data periodically
	if (std::chrono::duration_cast<std::chrono::seconds>(now - sessiondata->_cleanup) > sessiondata->_cleanup_period) {
		sessiondata->_cleanup = now;
		{
			std::unique_lock<std::shared_mutex> guard(sessiondata->_negativeInputsLock);
			for (auto node : sessiondata->_negativeInputs) {
				if (auto ptr = node->input.lock(); ptr) {
					if (ptr->HasFlag(Input::Flags::Duplicate)) {
						logmessage("Duplicate Input");
						sessiondata->_negativeInputs.erase(node);
					}
					if (ptr->HasFlag(Form::FormFlags::Deleted)) {
						logmessage("Deleted Input");
						sessiondata->_negativeInputs.erase(node);
					}
				} else {
					logmessage("Invalid pointer");
					sessiondata->_negativeInputs.erase(node);
				}
			}
			sessiondata->_negativeInputNumbers = (int64_t)sessiondata->_negativeInputs.size();
		}
		{
			std::unique_lock<std::shared_mutex> guard(sessiondata->_unfinishedInputsLock);
			for (auto node : sessiondata->_unfinishedInputs) {
				if (auto ptr = node->input.lock(); ptr) {
					if (ptr->HasFlag(Input::Flags::Duplicate)) {
						logmessage("Duplicate Input");
						sessiondata->_unfinishedInputs.erase(node);
					}
					if (ptr->HasFlag(Form::FormFlags::Deleted)) {
						logmessage("Deleted Input");
						sessiondata->_unfinishedInputs.erase(node);
					}
				} else {
					logmessage("Invalid pointer");
					sessiondata->_unfinishedInputs.erase(node);
				}
			}
			sessiondata->_unfinishedInputNumbers = (int64_t)sessiondata->_unfinishedInputs.size();
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
		if (!sessiondata)
			std::cout << "No sessiondata\n";
		if (sessiondata->data == nullptr)
			std::cout << "No sessiondata->data\n";
		if (!input)
			std::cout << "No input\n";
		if (!input->test)
			std::cout << "No input->test\n";

		std::cout << "Delete Test\n";
		sessiondata->data->DeleteForm(input->test);
		std::cout << "Delete Input\n";
		sessiondata->data->DeleteForm(input);
		std::cout << "Delete Done\n";

		input->UnsetFlag(Form::FormFlags::DoNotFree);
		return;
	}
	if (input->test->_skipOracle) {
		input->UnsetFlag(Form::FormFlags::DoNotFree);
		return;
	}

	// add this input to the current generation
	if (input->HasFlag(Input::Flags::GeneratedDeltaDebugging))
		sessiondata->GetGeneration(input->GetGenerationID())->AddDDInput(input);
	else
		sessiondata->GetGeneration(input->GetGenerationID())->AddGeneratedInput(input);

	// if the inputs generation matches the current generation, initiate the end of the current iteration
	if (input->GetGenerationID() == sessiondata->GetCurrentGenerationID())
		sessiondata->CheckGenerationEnd();

	// calculate oracle result
	bool stateerror = false;
	input->_oracleResult = Lua::EvaluateOracle(std::bind(&Oracle::Evaluate, sessiondata->_oracle, std::placeholders::_1, std::placeholders::_2), input->test, stateerror);
	if (stateerror) {
		logcritical("Test End functions cannot be completed, as the calling thread lacks a lua context");
		// remove input from the current generation
		// we do this as removing counts down the targeted generation values,
		if (input->HasFlag(Input::Flags::GeneratedDeltaDebugging))
			sessiondata->GetGeneration(input->GetGenerationID())->RemoveDDInput(input);
		else
			sessiondata->GetGeneration(input->GetGenerationID())->RemoveGeneratedInput(input);
		input->UnsetFlag(Form::FormFlags::DoNotFree);
		return;
	}
	// check whether _output should be stored
	input->test->_storeoutput = sessiondata->_settings->tests.storePUToutput || (sessiondata->_settings->tests.storePUToutputSuccessful && input->GetOracleResult() == OracleResult::Passing);

	if (input->_hasfinished == false)
		logwarn("Somehow its finished even though its not");

	// check _input result
	switch (input->GetOracleResult()) {
	case OracleResult::Failing:
		{
		// -----We have a failing _input-----
			sessiondata->_negativeInputNumbers++;

			// -----Calculate initial weight for backtracking-----
			double weight = 0.0f;

			// -----Add _input to its list-----
			sessiondata->AddInput(input, OracleResult::Failing, weight);
			
			// -----Add _input to exclusion tree as result is fixed-----
			sessiondata->_excltree->AddInput(input, OracleResult::Failing);



		}
		break;
	case OracleResult::Passing:
		{
			// -----We have a passing _input-----
			sessiondata->_positiveInputNumbers++;

			// -----Add _input to its list-----
			sessiondata->AddInput(input, OracleResult::Passing);

			// -----Add _input to exclusion tree as result is fixed-----
			sessiondata->_excltree->AddInput(input, OracleResult::Passing);



		}
		break;
	case OracleResult::Prefix:
		{
			// -----The _input has a prefix that has already been decided-----
			sessiondata->AddInput(input, OracleResult::Prefix);
			// inputs which's value is determined by a prefix cannot get here, as they 
			// are caught on generation, so everything here is just for show
		}
		break;
	case OracleResult::Unfinished:
		{
			// -----We have an unfinished _input-----
			sessiondata->_unfinishedInputNumbers++;

			// -----Calculate initial weight for expansion-----
			double weight = 0.0f;



			// -----Add _input to its list-----
			sessiondata->AddInput(input, OracleResult::Unfinished, weight);

			// -----Add _input to exclusion tree to avoid duplicate execution-----
			sessiondata->_excltree->AddInput(input, OracleResult::Unfinished);



		}
		break;
	case OracleResult::Undefined:
		{
			// -----We have an undefined result-----
			sessiondata->_undefinedInputNumbers++;
			sessiondata->AddInput(input, OracleResult::Undefined);
			// we don't really have anything else to do, since we cannot really use
			// tests with an undefined result as we simply do not know what it is
		}
		break;
	}

	input->UnsetFlag(Form::FormFlags::DoNotFree);
	return;
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

std::shared_ptr<DeltaDebugging::DeltaController> SessionFunctions::BeginDeltaDebugging(std::shared_ptr<SessionData>& sessiondata, std::shared_ptr<Input> input, std::shared_ptr<Functions::BaseFunction> callback, bool bypassTests)
{
	DeltaDebugging::DDGoal goal = DeltaDebugging::DDGoal::MaximizePrimaryScore;
	DeltaDebugging::DDMode mode = sessiondata->_settings->dd.mode;
	DeltaDebugging::DDParameters* params = nullptr;
	switch (goal)
	{
	case DeltaDebugging::DDGoal::MaximizePrimaryScore:
		{
			DeltaDebugging::MaximizePrimaryScore* par = new DeltaDebugging::MaximizePrimaryScore;
			par->acceptableLoss = (float)sessiondata->_settings->dd.optimizationLossThreshold;
			params = par;
		}
		break;
	case DeltaDebugging::DDGoal::MaximizeSecondaryScore:
		{
			DeltaDebugging::MaximizeSecondaryScore* par = new DeltaDebugging::MaximizeSecondaryScore;
			par->acceptableLossPrimary = (float)sessiondata->_settings->dd.optimizationLossThreshold;
			par->acceptableLossSecondary = (float)sessiondata->_settings->dd.optimizationLossThreshold;
			params = par;
		}
		break;
	case DeltaDebugging::DDGoal::ReproduceResult:
		{
			DeltaDebugging::ReproduceResult* par = new DeltaDebugging::ReproduceResult;
			params = par;
		}
		break;
	}
	params->bypassTests = bypassTests;
	params->minimalSubsetSize = (int32_t)sessiondata->_settings->dd.executeAboveLength + 1;
	auto control = sessiondata->data->CreateForm<DeltaDebugging::DeltaController>();
	if (control->Start(params, sessiondata, input, callback))
	{
		// add ddcontroler to current generation
		sessiondata->GetCurrentGeneration()->AddDDController(control);
		return control;
	}
	else
	{
		sessiondata->data->DeleteForm(control);
		return {};
	}
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
			//int64_t generate = _sessiondata->_settings->generation.generationsize - (int64_t)_sessiondata->_exechandler->WaitingTasks();
			int64_t generate = _sessiondata->GetNumberInputsToGenerate();
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
			//_sessiondata->_settings->generation.generationsize > (int64_t)_sessiondata->_exechandler->WaitingTasks() && failcount < (int64_t)_sessiondata->GENERATION_RETRIES && gencount < _sessiondata->_settings->generation.generationstep
			while (_sessiondata->CheckNumberInputsToGenerate(generated, failcount, generate) > 0) {
				loginfo("[MasterGenerationCallback] inner loop");
				std::mt19937 randan((uint32_t)std::chrono::steady_clock::now().time_since_epoch().count());
				std::uniform_int_distribution<signed> dist(0, 1);
				if (dist(randan) == 0 && _sessiondata->GetTopK_Unfinished(1).size() > 0) {
					auto parent = _sessiondata->GetTopK_Unfinished(1)[0];
					parent->SetFlag(Form::FormFlags::DoNotFree);
					input = SessionFunctions::ExtendInput(_sessiondata, parent);
					parent->UnsetFlag(Form::FormFlags::DoNotFree);
				}
				else
					input = SessionFunctions::GenerateInput(_sessiondata);
				if (input) {
					logdebug("[MasterGenerationCallback] prefix tree");
					if (_sessiondata->_excltree->HasPrefix(input) == false) {
						gencount++;
						// we found one that isn't a prefix
						_sessiondata->IncGeneratedInputs();
						//inputs.push_back(_input);
						logdebug("[MasterGenerationCallback] add test");
						// add tests to execution handler
						auto callback = dynamic_pointer_cast<Functions::TestCallback>(Functions::TestCallback::Create());
						callback->_sessiondata = _sessiondata;
						callback->_input = input;
						if (_sessiondata->_exechandler->AddTest(input, callback) == false) {
							_sessiondata->IncAddTestFails();
							_sessiondata->data->DeleteForm(input);
							callback->Dispose();
						} else
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

			if (generated != generate)
				_sessiondata->FailNumberInputsToGenerate(generate - generated, generated);

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
					auto _callback = dynamic_pointer_cast<Functions::TestCallback>(Functions::TestCallback::Create());
					_callback->_sessiondata = _sessiondata;
					_callback->_input = inp;
					if (_sessiondata->_exechandler->AddTest(inp, _callback) == false) {
						_sessiondata->IncAddTestFails();
						_sessiondata->data->DeleteForm(inp);
						_callback->Dispose();
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



	void GenerationEndCallback::Run()
	{
		// this callback calculates the top k inputs that will be kept for future generation of new inputs
		// it will start k delta controllers for the top k inputs
		// the callback for the delta controllers are instance of GenerationFinishedCallback
		// if all delta controllers are finished the transtion will begin

		// get top 5 percent generated inputs //_sessiondata->_settings->generation.generationsize * 0.05
		auto topk = _sessiondata->GetTopK((int32_t)(10));

		for (auto ptr : topk)
		{
			auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
			callback->_sessiondata = _sessiondata;
			auto ddcontroller = SessionFunctions::BeginDeltaDebugging(_sessiondata, ptr, callback, false);
			if (ddcontroller)
				_sessiondata->GetCurrentGeneration()->AddDDController(ddcontroller);
		}
	}

	bool GenerationEndCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
		});
		return true;
	}

	bool GenerationEndCallback::WriteData(unsigned char* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		return true;
	}

	size_t GenerationEndCallback::GetLength()
	{
		return BaseFunction::GetLength() + 8;
	}

	void GenerationEndCallback::Dispose()
	{
		_sessiondata.reset();
	}


	void GenerationFinishedCallback::Run()
	{
		if (_sessiondata->GetCurrentGeneration()->IsDeltaDebuggingActive() == false)
		{
			_sessiondata->SetNewGeneration();

			// copy sources to new generation
			// ------------------------------
			// ------------------------------
			// ------------------------------
			// ------------------------------
			// ------------------------------
			// ------------------------------
			// ------------------------------
			// ------------------------------
			// ------------------------------
			// ------------------------------
			// ------------------------------

			SessionFunctions::MasterControl(_sessiondata);
			SessionFunctions::GenerateTests(_sessiondata);
		}
	}

	bool GenerationFinishedCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
		});
		return true;
	}

	bool GenerationFinishedCallback::WriteData(unsigned char* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		return true;
	}

	size_t GenerationFinishedCallback::GetLength()
	{
		return BaseFunction::GetLength() + 8;
	}

	void GenerationFinishedCallback::Dispose()
	{
		_sessiondata.reset();
	}
}
