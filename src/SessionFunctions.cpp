#include "SessionFunctions.h"
#include "Session.h"
#include "Data.h"
#include "FilterFunctions.h"
#include "Test.h"
#include "LuaEngine.h"
#include "DerivationTree.h"
#include "Processes.h"
#include "DeltaDebugging.h"
#include "Form.h"
#include "Evaluation.h"

#include <mutex>
#include <boost/circular_buffer.hpp>
#include <time.h>
#include <algorithm>
#include <forward_list>
#include <numeric>
#include <random>
#include <set>

std::shared_ptr<Input> SessionFunctions::GenerateInput(std::shared_ptr<SessionData> sessiondata)
{
	loginfo("[GenerateInput] create form");
	logdebug("generating input");
	// generate a new _input
	std::shared_ptr<Input> input = sessiondata->data->CreateForm<Input>();
	input->SetFlag(Form::FormFlags::DoNotFree);
	input->derive = sessiondata->data->CreateForm<DerivationTree>();
	input->derive->SetInputID(input->GetFormID());
	loginfo("[GenerateInput] generate");
	if (sessiondata->_generator->Generate(input, {}, {}, sessiondata) == false)
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

std::shared_ptr<Input> SessionFunctions::ExtendInput(std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Input> parent)
{
	loginfo("[GenerateInput] create form");
	logdebug("extending input {}", Utility::GetHex(parent->GetFormID()));
	// generate a new _input
	std::shared_ptr<Input> input = sessiondata->data->CreateForm<Input>();
	input->SetFlag(Form::FormFlags::DoNotFree);
	input->derive = sessiondata->data->CreateForm<DerivationTree>();
	input->derive->SetInputID(input->GetFormID());
	// flag makes the generator extend the input
	input->SetFlag(Input::Flags::GeneratedGrammarParent);
	if (parent->GetOracleResult() == OracleResult::Failing)
		input->SetFlag(Input::Flags::GeneratedGrammarParentBacktrack);
	loginfo("[GenerateInput] generate");
	if (sessiondata->_generator->Generate(input, parent, {}, sessiondata) == false) {
		sessiondata->data->DeleteForm(input->derive);
		input->derive.reset();
		sessiondata->data->DeleteForm(input);
		return {};
	}
	if (input->derive->GetRegenerate() == false)
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

void SessionFunctions::ExtendInput(std::shared_ptr<Input> input, std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Input> parent)
{
	// if we already generated the _input: skip
	if (input->GetGenerated() || !sessiondata->_generator)
		return;
	if (input) {
		logdebug("re-extending input {}", Utility::GetHex(input->GetFormID()));
		// generate a new _input
		if (!input->derive) {
			input->derive = sessiondata->data->CreateForm<DerivationTree>();
			input->derive->SetInputID(input->GetFormID());
		}
		// flag makes the generator extend the input
		input->SetFlag(Input::Flags::GeneratedGrammarParent);
		if (input->GetOracleResult() == OracleResult::Failing)
			input->SetFlag(Input::Flags::GeneratedGrammarParentBacktrack);
		loginfo("[GenerateInput] generate");
		if (sessiondata->_generator->GetGrammar() && sessiondata->_generator->GetGrammar()->GetFormID() == input->derive->GetGrammarID()) {
			if (sessiondata->_generator->Generate(input, parent, {}, sessiondata) == false) {
				//sessiondata->data->DeleteForm(input->derive);
				//input->derive.reset();
				//sessiondata->data->DeleteForm(input);
				//input.reset();
				logwarn("failed to generate input");
			}
		} else {
			auto grammar = sessiondata->data->LookupFormID<Grammar>(input->derive->GetGrammarID());
			if (grammar) {
				sessiondata->_generator->Generate(input, parent, grammar, sessiondata);
				return;
			}
		}
		loginfo("[GenerateInput] setup");
	} else
		input = ExtendInput(sessiondata, parent);
}

void SessionFunctions::GenerateInput(std::shared_ptr<Input> input, std::shared_ptr<SessionData> sessiondata)
{
	// if we already generated the _input: skip
	if (input->GetGenerated() || !sessiondata->_generator)
		return;
	if (input) {
		logdebug("extending input {}", Utility::GetHex(input->GetFormID()));
		// generate a new _input
		if (!input->derive) {
			input->derive = sessiondata->data->CreateForm<DerivationTree>();
			input->derive->SetInputID(input->GetFormID());
		}
		loginfo("[GenerateInput] generate");
		if (sessiondata->_generator->GetGrammar() && sessiondata->_generator->GetGrammar()->GetFormID() == input->derive->GetGrammarID()) {
			if (sessiondata->_generator->Generate(input, {}, {}, sessiondata) == false) {
				//sessiondata->data->DeleteForm(input->derive);
				//input->derive.reset();
				//sessiondata->data->DeleteForm(input);
				//input.reset();
			}
		} else {
			auto grammar = sessiondata->data->LookupFormID<Grammar>(input->derive->GetGrammarID());
			if (grammar) {
				sessiondata->_generator->Generate(input, {}, grammar, sessiondata);
				return;
			}
		}
		loginfo("{}: [GenerateInput] setup", Utility::PrintForm(input));
	} else
		input = GenerateInput(sessiondata);
}

void SessionFunctions::SaveCheck(std::shared_ptr<SessionData> sessiondata, bool generationEnd)
{
	loginfo("whether to save or not to save");
	// check whether to save at all
	if (!sessiondata->_settings->saves.enablesaves) {
		if (generationEnd) {
			auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
			callback->_sessiondata = sessiondata;
			callback->_generation = sessiondata->GetCurrentGeneration();
			callback->_afterSave = true;
			loginfo("Adding session end task");
			sessiondata->_controller->AddTask(callback);
		}
		return;
	}
	static auto autosavetime = std::chrono::steady_clock::now();
	static uint64_t testnum = SessionStatistics::TestsExecuted(sessiondata);
	static std::mutex savelock;
	bool save = false;
	{
		std::unique_lock<std::mutex> guard(savelock);
		if (generationEnd)
		{
			// if we are performing a generation end check, just do it and skip the rest
			auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
			callback->_sessiondata = sessiondata;
			callback->_generation = sessiondata->GetCurrentGeneration();
			callback->_afterSave = true;
			if (sessiondata->_settings->saves.saveAfterEachGeneration) {
				logmessage("Save session async");
				std::thread(SaveSession_Async, sessiondata, callback, generationEnd).detach();
			} else {
				loginfo("Adding session end task");
				sessiondata->_controller->AddTask(callback);
			}
			return;
		}
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
	std::thread(SaveSession_Async, sessiondata, std::shared_ptr<Functions::BaseFunction>{}, generationEnd).detach();
}

void SessionFunctions::SaveSession_Async(std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Functions::BaseFunction> callback, bool generationEnded)
{
	loginfo("Master Save Session");
	// async function that simply initiates the save
	// we are using this to free us from a potential deadlock in the TaskController
	// if the save is orchestrated by a _callback within the TaskController
	// if we would allow it, the saving function that stops the taskcontroller temporarily
	// would deadlock itself
	sessiondata->data->Save(callback);
	checkendconditionsskipsaving = true;

	// check whether we should end right now, if so skip saving and just exit
	EndCheck(sessiondata, generationEnded);
}

bool SessionFunctions::EndCheck(std::shared_ptr<SessionData> sessiondata, bool generationEnded)
{
	auto sessdat = sessiondata;
	loginfo("whether to end or not to end");
	// check whether end conditions are met
	bool end = false;
	if (sessiondata->_settings->generation.generationalMode && generationEnded) {
		// only check for test end conditions on the end of each generation in generational mode
		// to give the program time to complete all outstanding delta debuggings
		if (sessiondata->_settings->conditions.use_foundnegatives && SessionStatistics::NegativeTestsGenerated(sessiondata) >= sessiondata->_settings->conditions.foundnegatives) {
			end = true;
			loginfo("Condition fulfilled: Negative Tests");
		} else if (sessiondata->_settings->conditions.use_foundpositives && SessionStatistics::PositiveTestsGenerated(sessiondata) >= sessiondata->_settings->conditions.foundpositives) {
			end = true;
			loginfo("Condition fulfilled: Positive Tests");
		}

		else if (sessiondata->_settings->conditions.use_overalltests && SessionStatistics::TestsExecuted(sessiondata) >= sessiondata->_settings->conditions.overalltests) {
			end = true;
			loginfo("Condition fulfilled: Overall Tests");
		}

		else if (sessiondata->_settings->conditions.use_generations && sessiondata->GetCurrentGeneration()->GetGenerationNumber() == sessiondata->_settings->conditions.generations) {
			end = true;
			loginfo("Condition fulfilled: Generations");
		}

		if (end == true && sessiondata->GetCurrentGeneration()->IsDeltaDebuggingActive() == true) {
			// block input generation, but finish delta debugging
			bool prior = sessiondata->BlockInputGeneration();
			end = false;
			loginfo("Condition overwritte: Finish delta debugging");

			if (!prior) {
				// add session finish callback to all active dd controllers
				std::vector<std::shared_ptr<DeltaDebugging::DeltaController>> controllers;
				sessiondata->GetCurrentGeneration()->GetDDControllers(controllers);
				for (auto ptr : controllers) {
					if (ptr->Finished() == false) {
						auto callback = dynamic_pointer_cast<Functions::FinishSessionCallback>(Functions::FinishSessionCallback::Create());
						callback->_sessiondata = sessdat;
						ptr->AddCallback(callback);
					}
				}
			}
		}
	}
	if (sessiondata->_settings->conditions.use_timeout && std::chrono::duration_cast<std::chrono::seconds>(SessionStatistics::Runtime(sessiondata)).count() >= sessiondata->_settings->conditions.timeout)
		end = true;
	double failureRate = 0.0f;

	// check whether the recent _input generation is converging and we should abort the program
	if (sessiondata->_generatedinputs < (int64_t)sessiondata->GENERATION_WEIGHT_BUFFER_SIZE) {
		failureRate = (double)std::accumulate(sessiondata->_recentfailes.begin(), sessiondata->_recentfailes.end(), 0) / (double)sessiondata->_generatedinputs;
		//if (failureRate >= sessiondata->GENERATION_WEIGHT_LIMIT)
		//	end = true;
	} else {
		failureRate = (double)std::accumulate(sessiondata->_recentfailes.begin(), sessiondata->_recentfailes.end(), 0) / (double)sessiondata->GENERATION_WEIGHT_BUFFER_SIZE;
		//if (failureRate >= sessiondata->GENERATION_WEIGHT_LIMIT)
		//	end = true;
	}

	sessiondata->_failureRate = failureRate;

	// if we don't end the session, do nothing
	if (end == false) {
		checkendconditionsskipsaving = false;
		return false;
	}
	loginfo("ending");
	if (generationEnded) {
		// if we are performing a generation end check, just do it and skip the rest
		auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
		callback->_sessiondata = sessiondata;
		callback->_generation = sessiondata->GetCurrentGeneration();
		callback->_afterSave = true;
		std::thread(EndSession_Async, sessiondata, callback).detach();
	} else {
		// start thread that clears the session
		std::thread(EndSession_Async, sessiondata, std::shared_ptr<Functions::BaseFunction>{}).detach();
	}
	return true;
}

void SessionFunctions::EndSession_Async(std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Functions::BaseFunction> callback)
{
	if (sessiondata->TryLockEnd()) {
		loginfo("Master End Session");
		auto session = sessiondata->data->CreateForm<Session>();
		if (session->_abort)
			return;
		// async function that ends the session itself

		// force save
		if (checkendconditionsskipsaving == false)
			session->data->Save(callback);
		checkendconditionsskipsaving = false;

		// end the session

		// set abort flag to catch all threads that try to escape
		session->StopSession(false, false);
		sessiondata->UnlockEnd();
	}
}

bool HandleForms(std::shared_ptr<Form> form)
{
	if (form->GetType() == FormType::DevTree)
		return false;
	if (form->Freed())
		return false;
	else
		return true;
}

void SessionFunctions::ReclaimMemory(std::shared_ptr<SessionData> sessiondata)
{
	loginfo("trying to free memory");
	StartProfiling;
	sessiondata->_lastMemorySweep = std::chrono::steady_clock::now();
	std::vector<std::shared_ptr<Form>> free;
	int32_t size = 0;
	sessiondata->data->Visit([&free,&size](std::shared_ptr<Form> form) {
		if (HandleForms(form)) {
			free.push_back(form);
			size++;
		}
		return Data::VisitAction::None;
		});

	profile(TimeProfiling, "Visiting {}", size);
	ResetProfiling;

	for (auto& form : free) {
		form->FreeMemory();
		if (form->GetType() == FormType::Input) {
			auto _input = form->As<Input>();
			if (_input->GetGenerated() == false && _input->test && _input->test->IsValid() == false) {
				if (_input->derive)
					_input->derive->FreeMemory();
			}
		}
	}
	free.clear();

#if defined(unix) || defined(__unix__) || defined(__unix)
	uint64_t mem = Processes::GetProcessMemory(getpid());
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	uint64_t mem = Processes::GetProcessMemory(GetCurrentProcess());
#endif
	mem = mem / 1048576;
	profile(TimeProfiling, "Freed {} MB", (int64_t)sessiondata->_memory_mem - (int64_t)mem);
	sessiondata->_memory_mem = mem;
}

void SessionFunctions::MasterControl(std::shared_ptr<SessionData> sessiondata, bool forceexecute)
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
	if (timediff >= sessiondata->_checkperiod || forceexecute) {
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
					std::thread(EndSession_Async, sessiondata, std::shared_ptr<Functions::BaseFunction>{}).detach();
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

bool SessionFunctions::TestEnd(std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Input> input, bool replay, std::shared_ptr<DeltaDebugging::DeltaController> producer)
{
	logdebug("calculating test end");
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

		return false;
	}
	if (input->test->_skipOracle) {
		return false;
	}
	StartProfiling

	// calculate oracle result
	bool stateerror = false;
	input->_oracleResult = Lua::EvaluateOracle(std::bind(&Oracle::Evaluate, sessiondata->_oracle, std::placeholders::_1, std::placeholders::_2), input->test, stateerror);
	profile(TimeProfiling, "{}: calcualted oracle", Utility::PrintForm(input));
	ResetProfiling;
	if (stateerror) {
		logcritical("Test End functions cannot be completed, as the calling thread lacks a lua context");
		// remove input from the current generation
		// we do this as removing counts down the targeted generation values,
		if (input->HasFlag(Input::Flags::GeneratedDeltaDebugging))
			sessiondata->GetGeneration(input->GetGenerationID())->RemoveDDInput(input);
		else
			sessiondata->GetGeneration(input->GetGenerationID())->RemoveGeneratedInput(input);
		return false;
	}
	if (input->GetOracleResult() == OracleResult::Repeat || sessiondata->_settings->fixes.repeatTimeoutedTests && input->test && input->test->_exitreason == Test::ExitReason::Timeout) {
		if (input->GetRetries() < 5) {
			// the oracle decided there was a problem with the test, so go and repeat it
			loginfo("Repeating test");
			input->IncRetries();
			input->test->_exitreason = Test::ExitReason::Repeat;
			input->Debug_ClearSequence();
			input->SetGenerated(false);
			auto test = input->test;
			input->test.reset();
			auto callback = test->_callback->DeepCopy();
			sessiondata->data->DeleteForm(test);
			sessiondata->_exechandler->AddTest(input, callback, true, false);
			AddTestExitReason(sessiondata, Test::ExitReason::Repeat);
			return true;
		} else {
			input->_oracleResult = OracleResult::Undefined;
			loginfo("Not repeating test");
		}
	}

	// add this input to the current generation
	if (input->HasFlag(Input::Flags::GeneratedDeltaDebugging))
		sessiondata->GetGeneration(input->GetGenerationID())->AddDDInput(input);
	else {
		sessiondata->GetGeneration(input->GetGenerationID())->AddGeneratedInput(input);

		// if the inputs generation matches the current generation, initiate the end of the current iteration
		if (input->GetGenerationID() == sessiondata->GetCurrentGenerationID())
			sessiondata->CheckGenerationEnd();
	}
	profile(TimeProfiling, "{}: added input to generation", Utility::PrintForm(input));
	ResetProfiling;

	// check whether _output should be stored
	input->test->_storeoutput = sessiondata->_settings->tests.storePUToutput || (sessiondata->_settings->tests.storePUToutputSuccessful && input->GetOracleResult() == OracleResult::Passing);

	if (input->_hasfinished == false)
		logwarn("Somehow its finished even though its not");

	if (!input->test || !input->derive) {
		logwarn("Input has neither test nor derivationtree");
		return false;
	}
	if (input->derive->GetRegenerate() == false)
	{
		logwarn("Regeneration of Dev Tree is not possible.");
		return false;
	}

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

			auto parent = sessiondata->data->LookupFormID<Input>(input->_parent.parentInput);
			if (parent)
				parent->IncDerivedFails();
		}
		break;
	case OracleResult::Passing:
		{
			// only begin delta debugging if the input has been generated ligitimately and not as a result of delta debugging
			if (!input->HasFlag(Input::Flags::GeneratedDeltaDebugging) || producer->GetGoal() != DeltaDebugging::DDGoal::ReproduceResult) {
				// -----Begin Delta Debugging the test with ReproduceResults-----
				BeginDeltaDebugging(sessiondata, input, {}, true, DeltaDebugging::DDGoal::ReproduceResult);
			}

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
	profile(TimeProfiling, "{}: added input to lists", Utility::PrintForm(input));
	return false;
}

void SessionFunctions::AddTestExitReason(std::shared_ptr<SessionData> sessiondata, int32_t reason)
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
	case Test::ExitReason::Pipe:
		sessiondata->exitstats.pipe++;
		break;
	case Test::ExitReason::InitError:
		sessiondata->exitstats.initerror++;
		break;
	case Test::ExitReason::Repeat:
		sessiondata->exitstats.repeat++;
		break;
	}
}

void SessionFunctions::GenerateTests(std::shared_ptr<SessionData> sessiondata)
{
	if (sessiondata->_active_generation_callbacks.load() < sessiondata->_max_generation_callbacks) {
		sessiondata->_active_generation_callbacks++;
		auto callback = dynamic_pointer_cast<Functions::MasterGenerationCallback>(Functions::MasterGenerationCallback::Create());
		callback->_sessiondata = sessiondata;
		sessiondata->_controller->AddTask(callback);
	}
}

std::shared_ptr<DeltaDebugging::DeltaController> SessionFunctions::BeginDeltaDebugging(std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Input> input, std::shared_ptr<Functions::BaseFunction> callback, bool bypassTests, DeltaDebugging::DDGoal goal, DeltaDebugging::DDGoal secondarygoal, int32_t budget)
{
	loginfo("enter");
	if (goal == DeltaDebugging::DDGoal::None)
		goal = DeltaDebugging::DDGoal::MaximizePrimaryScore;
	//if (secondarygoal == DeltaDebugging::DDGoal::None)
	//	secondarygoal = DeltaDebugging::DDGoal::MaximizeBothScores;
	DeltaDebugging::DDMode mode = sessiondata->_settings->dd.mode;
	if (input->IsIndividualPrimaryScoresEnabled() && sessiondata->_settings->dd.allowScoreOptimization)
		mode = DeltaDebugging::DDMode::ScoreProgress;
	DeltaDebugging::DDParameters* params = nullptr;
	switch (goal)
	{
	case DeltaDebugging::DDGoal::MaximizePrimaryScore:
		{
			DeltaDebugging::MaximizePrimaryScore* par = new DeltaDebugging::MaximizePrimaryScore;
			par->acceptableLoss = (float)sessiondata->_settings->dd.optimizationLossThreshold;
			par->acceptableLossAbsolute = (float)sessiondata->_settings->dd.optimizationLossAbsolute;
			params = par;
		}
		break;
	case DeltaDebugging::DDGoal::MaximizeSecondaryScore:
		{
			DeltaDebugging::MaximizeSecondaryScore* par = new DeltaDebugging::MaximizeSecondaryScore;
			par->acceptableLossSecondary = (float)sessiondata->_settings->dd.optimizationLossThreshold;
			par->acceptableLossSecondaryAbsolute = (float)sessiondata->_settings->dd.optimizationLossAbsolute;
			params = par;
		}
		break;
	case DeltaDebugging::DDGoal::MaximizeBothScores:
		{
			DeltaDebugging::MaximizeBothScores* par = new DeltaDebugging::MaximizeBothScores;
			par->acceptableLossPrimary = (float)sessiondata->_settings->dd.optimizationLossThreshold;
			par->acceptableLossPrimaryAbsolute = (float)sessiondata->_settings->dd.optimizationLossAbsolute;
			par->acceptableLossSecondary = (float)sessiondata->_settings->dd.optimizationLossThreshold;
			par->acceptableLossSecondaryAbsolute = (float)sessiondata->_settings->dd.optimizationLossAbsolute;
			params = par;
		}
		break;
	case DeltaDebugging::DDGoal::ReproduceResult:
		{
			DeltaDebugging::ReproduceResult* par = new DeltaDebugging::ReproduceResult;
			par->secondarygoal = secondarygoal;
			params = par;
		}
		break;
	case DeltaDebugging::DDGoal::None:
		break;
	}
	params->mode = mode;
	params->bypassTests = bypassTests;
	params->minimalSubsetSize = (int32_t)sessiondata->_settings->dd.executeAboveLength + 1;
	params->budget = budget;
	auto control = sessiondata->data->CreateForm<DeltaDebugging::DeltaController>();
	if (control->Start(params, sessiondata, input, callback))
	{
		// add ddcontroler to current generation
		sessiondata->GetCurrentGeneration()->AddDDController(control);
		loginfo("started delta debugging");
		return control;
	}
	else
	{
		sessiondata->data->DeleteForm(control);
		loginfo("couldn't start delta debugging");
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

uint64_t SessionStatistics::UndefinedTestsGenerated(std::shared_ptr<SessionData>& sessiondata)
{
	return (uint64_t)sessiondata->_undefinedInputNumbers;
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

		auto randan = std::mt19937((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
		auto gendistr = std::uniform_int_distribution<signed>(1, 100);

		// check whether we are allowed to generate new inputs
		if (_sessiondata->CanGenerate() == false)
			return;

		while (true) {

			// get readers lock for input generation
			_sessiondata->Acquire_InputGenerationReadersLock();

			// check whether we need to generate new inputs
			// the goal is to always a certain number of tests prepared waiting for execution.
			// Normally this can simply be achieved by adding a new test for each one that finishes,
			// but since stuff can happen and it doesn't cost much, we will check the number of
			// waiting tests against the parameters

			// number of new tests we need to generate
			//int64_t generate = _sessiondata->_settings->generation.generationsize - (int64_t)_sessiondata->_exechandler->WaitingTasks();
			int64_t generate = _sessiondata->GetNumberInputsToGenerate();
			if (generate == 0) {
				if (auto gen = _sessiondata->GetCurrentGeneration(); gen->NeedsGeneration() && _sessiondata->_exechandler->WaitingTasks() == 0) {
					gen->ResetActiveInputs();
					generate = _sessiondata->GetNumberInputsToGenerate();
				}
				if (generate == 0) {
					// get readers lock for input generation
					_sessiondata->Release_InputGenerationReadersLock();
					return;
				}
			}
			
			int32_t generated = 0;

			std::shared_ptr<Input> input;
			//std::vector<std::shared_ptr<Input>> inputs;

			int64_t failcount = 0;
			int64_t gencount = 0;

			auto gen = _sessiondata->GetCurrentGeneration();

			// generate new inputs while we have too few inputs waiting for execution
			// break the loop if we have accumulated too many failures
			// try to generate new inputs for a maximum of 1000 times
			// this should be high enough so that we can generate enough inputs to find some
			// that aren't excluded yet, while still making sure we don't take forever
			// also stop generation is we have generated [generationsize] passing inputs, as new tests
			// might have finished in the meantime and we don't want this to become an endless loop
			// blocking our task executions
			//_sessiondata->_settings->generation.generationsize > (int64_t)_sessiondata->_exechandler->WaitingTasks() && failcount < (int64_t)_sessiondata->GENERATION_RETRIES && gencount < _sessiondata->_settings->generation.generationstep
			while (_sessiondata->CheckNumberInputsToGenerate(generated, failcount, generate) > 0 && failcount < (int64_t)_sessiondata->GENERATION_RETRIES && _sessiondata->GetGenerationEnding() == false) {
				loginfo("[MasterGenerationCallback] inner loop");
				/*std::mt19937 randan((uint32_t)std::chrono::steady_clock::now().time_since_epoch().count());
				std::uniform_int_distribution<signed> dist(0, 1);
				if (dist(randan) == 0 && _sessiondata->GetTopK_Unfinished(1).size() > 0) {
					auto parent = _sessiondata->GetTopK_Unfinished(1)[0];
					parent->SetFlag(Form::FormFlags::DoNotFree);
					input = SessionFunctions::ExtendInput(_sessiondata, parent);
					parent->UnsetFlag(Form::FormFlags::DoNotFree);
				}
				else
					input = SessionFunctions::GenerateInput(_sessiondata);*/
				if (gen->HasSources() && (_sessiondata->_settings->generation.chance_new_generations == 0 || _sessiondata->_settings->generation.chance_new_generations < gendistr(randan)))
				{
					auto parent = gen->GetRandomSource();
					if (parent)
						input = SessionFunctions::ExtendInput(_sessiondata, parent);
					else {
						// check whether the sources of the generation are still valid
						if (_sessiondata->CheckGenerationEnd(true)) {
							// release readers lock for input generation
							_sessiondata->Release_InputGenerationReadersLock();
							return;
						}
					}
				} else
					input = SessionFunctions::GenerateInput(_sessiondata);
				if (input && input->derive) {
					if (input->derive->GetRegenerate() == false) {
						logcritical("Generated non-regeneratable input");
					}
					// set input generation time
					input->SetGenerationTime(_sessiondata->data->GetRuntime());
					input->SetGenerationID(_sessiondata->GetCurrentGenerationID());
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
						if (input->test)
							_sessiondata->data->DeleteForm(input->test);
						if (input->derive)
							_sessiondata->data->DeleteForm(input->derive);
						auto parent = _sessiondata->data->LookupFormID<Input>(input->GetParentID());
						if (parent)
							parent->IncDerivedFails();
						_sessiondata->data->DeleteForm(input);
						_sessiondata->IncGeneratedWithPrefix();
						failcount++;
					}
				} else {
					if (input) {
						if (input->test)
							_sessiondata->data->DeleteForm(input->test);
						if (input->derive)
							_sessiondata->data->DeleteForm(input->derive);
					}
					_sessiondata->data->DeleteForm(input);
					failcount++;
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

			if (_sessiondata->GetGenerationEnding()) {
				profile(TimeProfiling, "Time taken for MasterGenerationCallback");
				// release readers lock for input generation1
				_sessiondata->Release_InputGenerationReadersLock();
				return;
			}

			if (generated == 0 && failcount >= (int64_t)_sessiondata->GENERATION_RETRIES /*generate == _sessiondata->_settings->generation.generationsize */) {
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
					// release readers lock for input generation
					_sessiondata->Release_InputGenerationReadersLock();
					return;
				}
				// check whether the sources of the generation are still valid
				if (_sessiondata->CheckGenerationEnd(true)) {
					// release readers lock for input generation
					_sessiondata->Release_InputGenerationReadersLock();
					return;
				}
			} else {
				// release readers lock for input generation
				_sessiondata->Release_InputGenerationReadersLock();
				return;
			} 
			// release readers lock for input generation
			_sessiondata->Release_InputGenerationReadersLock();
		}
		profile(TimeProfiling, "Time taken for MasterGenerationCallback");
	}

	std::shared_ptr<BaseFunction> MasterGenerationCallback::DeepCopy()
	{
		auto ptr = std::make_shared<MasterGenerationCallback>();
		ptr->_sessiondata = _sessiondata;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	bool MasterGenerationCallback::ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		if (!CmdArgs::_clearTasks)
			resolver->AddTask([this, sessid, resolver]() {
				this->_sessiondata = resolver->_data->CreateForm<SessionData>();
			});
		return true;
	}

	bool MasterGenerationCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		if (!CmdArgs::_clearTasks)
			resolver->AddTask([this, sessid, resolver]() {
				this->_sessiondata = resolver->_data->CreateForm<SessionData>();
			});
		return true;
	}

	bool MasterGenerationCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		return true;
	}

	unsigned char* MasterGenerationCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		size = GetLength();
		return buffer;
	}

	size_t MasterGenerationCallback::GetLength()
	{
		return BaseFunction::GetLength() + 8;
	}

	void MasterGenerationCallback::Dispose()
	{
		if (_sessiondata)
			_sessiondata->_active_generation_callbacks--;
		_sessiondata.reset();
	}

	void WaitForGen(std::shared_ptr<Generation>& generation, std::shared_ptr<SessionData>& sessiondata, std::chrono::milliseconds timeout)
	{
		loginfo("wait for generation to end");
		int64_t lastactive = 0, active = generation->GetActiveInputs();
	StartWait:
		while (sessiondata->_exechandler->WaitingTasks() > 0 || sessiondata->_controller->GetWaitingLightJobs() > 0 || sessiondata->_controller->GetWaitingHeavyJobs() > 0 || sessiondata->_controller->GetWaitingMediumJobs() > 0 || sessiondata->_controller->Working() > 1) {
				loginfo("WaitingTests: {}, LightTasks: {}", sessiondata->_exechandler->WaitingTasks(), sessiondata->_controller->GetWaitingLightJobs());
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		auto time = std::chrono::steady_clock::now();
		while (generation->GetActiveInputs() > 0 && time + timeout < std::chrono::steady_clock::now())
		{
			if (lastactive != generation->GetActiveInputs())
				time = std::chrono::steady_clock::now();
			loginfo("Active Tasks: {}", generation->GetActiveInputs());
			if (sessiondata->_exechandler->WaitingTasks() > 0 || sessiondata->_controller->GetWaitingLightJobs() > 0 || sessiondata->_controller->GetWaitingHeavyJobs() > 0 || sessiondata->_controller->GetWaitingMediumJobs() > 0 || sessiondata->_controller->Working() > 1)
				goto StartWait;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	void GenerationEndCallback::Run()
	{
		loginfo("Generation ended.");

		auto generation = _sessiondata->GetCurrentGeneration();
		if (_sessiondata->_controller->GetNumThreads() == 1 && (_sessiondata->_exechandler->WaitingTasks() > 0 || _sessiondata->_controller->GetWaitingLightJobs() > 0 || _sessiondata->_controller->GetWaitingHeavyJobs() > 0 || _sessiondata->_controller->GetWaitingMediumJobs() > 0 || _sessiondata->_controller->Working() > 1)) {
			// if we only have one thread running, put ourselves back into waiting queue and
			// give the taskhandler a chance to execute the waiting light tasks first
			auto callback = dynamic_pointer_cast<Functions::GenerationEndCallback>(Functions::GenerationEndCallback::Create());
			callback->_sessiondata = _sessiondata;
			_sessiondata->_controller->AddTask(callback, false);
			return;
		}
		loginfo("wait for gen to end");
		WaitForGen(generation, _sessiondata, std::chrono::milliseconds(10000));

		loginfo("begin delta debugging selection");
		// if there is are delta debugging instances active, get them and assign generation finished callbacks to it,so we make sure they are finished before we start the next generation
		std::vector<std::shared_ptr<DeltaDebugging::DeltaController>> controllers;
		_sessiondata->GetCurrentGeneration()->GetDDControllers(controllers);


		if (_sessiondata->_settings->dd.deltadebugging && _sessiondata->_settings->dd.allowScoreOptimization) {
			loginfo("score optimization enabled, select inputs for delta debugging");
			// this callback calculates the top k inputs that will be kept for future generation of new inputs
			// it will start k delta controllers for the top k inputs
			// the callback for the delta controllers are instance of GenerationFinishedCallback
			// if all delta controllers are finished the transtion will begin

			std::set<std::shared_ptr<Input>, FormIDLess<Input>> targets;
			// frac is the fraction of the max score that we check for in each iteration
			double frac = _sessiondata->_settings->dd.optimizationLossThreshold;
			double max = 0.0f;

			switch (_sessiondata->_settings->generation.sourcesType) {
			case Settings::GenerationSourcesType::FilterLength:
				{
				}
				break;
			case Settings::GenerationSourcesType::FilterPrimaryScoreRelative:
			case Settings::GenerationSourcesType::FilterPrimaryScore:
				{
					auto top = _sessiondata->GetTopK(1);

					if (top.size() > 0)
						max = top[0]->GetPrimaryScore();

					FilterFunctions::FilterSet<InputGreaterPrimary>(_sessiondata, generation, targets, max, frac, true, true, _sessiondata->_settings->generation.numberOfInputsToDeltaDebugPerGeneration);

					if ((int32_t)targets.size() < _sessiondata->_settings->generation.numberOfInputsToDeltaDebugPerGeneration) {
						auto vec = _sessiondata->FindKSources(_sessiondata->_settings->generation.numberOfInputsToDeltaDebugPerGeneration - (int32_t)targets.size(), std::set<std::shared_ptr<Input>>{ targets.begin(), targets.end() }, _sessiondata->_settings->generation.allowBacktrackFailedInputs, 1, 2 /*cannot backtrack on length 1*/);
						for (auto input : vec) {
							targets.insert(input);
						}
					}
			}
			break;
			case Settings::GenerationSourcesType::FilterSecondaryScoreRelative:
			case Settings::GenerationSourcesType::FilterSecondaryScore:
				{
					auto top = _sessiondata->GetTopK_Secondary(1);

					if (top.size() > 0)
						max = top[0]->GetSecondaryScore();

					FilterFunctions::FilterSet<InputGreaterSecondary>(_sessiondata, generation, targets, max, frac, true, false, _sessiondata->_settings->generation.numberOfInputsToDeltaDebugPerGeneration);

					if ((int32_t)targets.size() < _sessiondata->_settings->generation.numberOfInputsToDeltaDebugPerGeneration) {
						auto vec = _sessiondata->FindKSources(_sessiondata->_settings->generation.numberOfInputsToDeltaDebugPerGeneration - (int32_t)targets.size(), std::set<std::shared_ptr<Input>>{ targets.begin(), targets.end() }, _sessiondata->_settings->generation.allowBacktrackFailedInputs, 1, 2 /*cannot backtrack on length 1*/);
						for (auto input : vec) {
							targets.insert(input);
						}
					}
				}
				break;
			}

			loginfo("Found {} targets to delta debug", targets.size());

			// indicates whether we were able to start delta debugging
			bool begun = false;
			int32_t count = 0;
			for (auto ptr : targets) {
				loginfo("1");
				if (count >= (int32_t)(_sessiondata->_settings->generation.numberOfInputsToDeltaDebugPerGeneration))
					break;
				loginfo("2");
				auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
				callback->_generation = _sessiondata->GetCurrentGeneration();
				callback->_sessiondata = _sessiondata;
				loginfo("3");
				auto ddcontroller = SessionFunctions::BeginDeltaDebugging(_sessiondata, ptr, callback, false, DeltaDebugging::DDGoal::None, DeltaDebugging::DDGoal::None, _sessiondata->_settings->dd.budgetGenEnd);
				loginfo("4");
				if (ddcontroller) {
					_sessiondata->GetCurrentGeneration()->AddDDController(ddcontroller);
					count++;
					begun = true;
					loginfo("Startet Delta Debugging for Input: {}", Input::PrintForm(ptr));
				}
			}
			// check if there was an active delta debugging beforehand
			for (auto ptr : controllers) {
				loginfo("5");
				if (ptr->Finished() == false) {
					loginfo("6");
					begun = true;
					auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
					callback->_sessiondata = _sessiondata;
					callback->_generation = _sessiondata->GetCurrentGeneration();
					if (ptr->AddCallback(callback))
						begun = true;
				}
			}

			if (begun == false) {
				loginfo("no delta debugging was started, end generation");
				// we haven't been able to start delta debugging, so add Generation Finished callback ourselves

				auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
				callback->_generation = _sessiondata->GetCurrentGeneration();
				callback->_sessiondata = _sessiondata;
				_sessiondata->_controller->AddTask(callback);
			}
		}
		else
		{
			loginfo("no delta debugging started, wait for existing delta debugging to end");
			bool begun = false;
			// check if there was an active delta debugging beforehand
			for (auto ptr : controllers) {
				loginfo("7");
				if (ptr->Finished() == false) {
					loginfo("8");
					auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
					callback->_sessiondata = _sessiondata;
					callback->_generation = _sessiondata->GetCurrentGeneration();
					if (ptr->AddCallback(callback))
						begun = true;
				}
			}
			if (begun == false) {
				loginfo("no existing dleta debugging to wait for, end generation");
				// there is no delta debugging active in the current generation, so add Generation Finished callback ourselves
				auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
				callback->_sessiondata = _sessiondata;
				callback->_generation = _sessiondata->GetCurrentGeneration();
				_sessiondata->_controller->AddTask(callback);
			}
		}
	}

	std::shared_ptr<BaseFunction> GenerationEndCallback::DeepCopy()
	{
		auto ptr = std::make_shared<GenerationEndCallback>();
		ptr->_sessiondata = _sessiondata;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	bool GenerationEndCallback::ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
		});
		return true;
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

	bool GenerationEndCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		return true;
	}

	unsigned char* GenerationEndCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		size = GetLength();
		return buffer;
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
		bool exp = false;
		if (_afterSave == false) {
			loginfo("Stats:\tGenerated: {}\tDD Controllers: {}\tDDSize: {}\tDDActive: {}",
				_sessiondata->GetCurrentGeneration()->GetGeneratedSize(),
				_sessiondata->GetCurrentGeneration()->GetNumberOfDDControllers(),
				_sessiondata->GetCurrentGeneration()->GetDDSize(), _sessiondata->GetCurrentGeneration()->IsDeltaDebuggingActive());
			auto generation = _sessiondata->GetCurrentGeneration();
			if (_sessiondata->_generationFinishing.compare_exchange_strong(exp, true)) {
				if (_sessiondata->_controller->GetNumThreads() == 1 && (_sessiondata->_exechandler->WaitingTasks() > 0 || _sessiondata->_controller->GetWaitingLightJobs() > 0 || _sessiondata->_controller->GetWaitingHeavyJobs() > 0 || _sessiondata->_controller->GetWaitingMediumJobs() > 0 || _sessiondata->_controller->Working() > 1)) {
					// if we only have one thread running, put ourselves back into waiting queue and
					// give the taskhandler a chance to execute the waiting light tasks first
					auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
					callback->_sessiondata = _sessiondata;
					callback->_afterSave = false;
					_sessiondata->_controller->AddTask(callback, false);
					return;
				}
				WaitForGen(generation, _sessiondata, std::chrono::milliseconds(10000));
				if (_sessiondata->GetCurrentGeneration()->IsDeltaDebuggingActive() == false /*if there are multiple callbacks for the same generation, make sure that only one gets executed, i.e. the fastest*/) {
					WaitForGen(generation, _sessiondata, std::chrono::milliseconds(10000));
					generation->SetEndTime(_sessiondata->data->GetRuntime());
					if (CmdArgs::_results) {
						StartProfiling;
						_sessiondata->data->StopClock();
						auto eval = Evaluation::GetSingleton();
						eval->EvaluateGeneral();
						profile(TimeProfiling, "Time taking for Evaluation General");
						ResetProfiling;
						eval->Evaluate(generation);
						profile(TimeProfiling, "Time taking for Evaluation Generation");
						ResetProfiling;
						std::vector<std::shared_ptr<DeltaDebugging::DeltaController>> controllers;
						generation->GetDDControllers(controllers);
						for (auto dd : controllers) {
							eval->Evaluate(dd);
						}
						profile(TimeProfiling, "Time taking for Evaluation DD");
						ResetProfiling;
						eval->EvaluateTopK();
						profile(TimeProfiling, "Time taking for Evaluation TopK");
						ResetProfiling;
						eval->EvaluatePositive();
						profile(TimeProfiling, "Time taking for Evaluation Positive");
						_sessiondata->data->StartClock();
					}
					loginfo("HoHoHo");
					if (SessionFunctions::EndCheck(_sessiondata, true)) {
						loginfo("End Check ending");
						// we are ending the session, so the callback is gonna be saved and we don't need to do anything else
						return;
					}
					SessionFunctions::SaveCheck(_sessiondata, true);
					SessionFunctions::MasterControl(_sessiondata);
				} else {
					loginfo("HaHaHa");
					// delta debugging is active
					std::vector<std::shared_ptr<DeltaDebugging::DeltaController>> controllers;
					_sessiondata->GetCurrentGeneration()->GetDDControllers(controllers);
					for (auto ptr : controllers) {
						if (ptr->Finished() == false && ptr->HasCallback(Functions::GenerationFinishedCallback::GetTypeStatic()) == false) {
							auto callback = dynamic_pointer_cast<Functions::GenerationFinishedCallback>(Functions::GenerationFinishedCallback::Create());
							callback->_sessiondata = _sessiondata;
							callback->_generation = _sessiondata->GetCurrentGeneration();
							ptr->AddCallback(callback);
						}
					}
				}
			}
		} else {
			loginfo("Begin transition to new generation");

			StartProfiling;
			auto generation = _sessiondata->GetCurrentGeneration();

			WaitForGen(generation, _sessiondata, std::chrono::milliseconds(10000));

			// get writers lock for input generation
			_sessiondata->Acquire_InputGenerationWritersLock();

			loginfo("Initializing New Generation");
			_sessiondata->SetNewGeneration();
			auto lastgen = _sessiondata->GetLastGeneration();
			loginfo("Gather new sources");
			// determine new sources and copy them

			std::set<std::shared_ptr<Input>, FormIDLess<Input>> sources;
			// frac is the fraction of the max score that we are check for in this iteration
			double frac = _sessiondata->_settings->dd.optimizationLossThreshold;
			double max = 0.0f;

			switch (_sessiondata->_settings->generation.sourcesType) {
			case Settings::GenerationSourcesType::FilterLength:
				{
					std::vector<std::shared_ptr<Input>> topk;
					if (_sessiondata->_settings->generation.allowBacktrackFailedInputs)
						topk = _sessiondata->GetTopK_Length(_sessiondata->_settings->generation.numberOfSourcesPerGeneration * 4, 1, 2 /*cannot backtrack on length 1*/);
					else
						topk = _sessiondata->GetTopK_Length_Unfinished(_sessiondata->_settings->generation.numberOfSourcesPerGeneration * 4);
					for (auto input : topk) {
						if (_sessiondata->_settings->generation.maxNumberOfFailsPerSource > input->GetDerivedFails() || _sessiondata->_settings->generation.maxNumberOfFailsPerSource == 0) {
							// check that the length of the source is longer than min backtrack, such that it can be extended in the first place
							if (input->GetOracleResult() == OracleResult::Unfinished && input->EffectiveLength() > _sessiondata->_settings->methods.IterativeConstruction_Extension_Backtrack_min ||
								input->GetOracleResult() == OracleResult::Failing && input->EffectiveLength() > _sessiondata->_settings->methods.IterativeConstruction_Backtrack_Backtrack_min) {
								sources.insert(input);
								loginfo("Found New Source: {}", Input::PrintForm(input));
							}
						}
						if ((int32_t)sources.size() >= _sessiondata->_settings->generation.numberOfSourcesPerGeneration)
							break;
					}
					if ((int32_t)sources.size() < _sessiondata->_settings->generation.numberOfSourcesPerGeneration) {
						auto vec = _sessiondata->FindKSources(_sessiondata->_settings->generation.numberOfSourcesPerGeneration - (int32_t)sources.size(), std::set<std::shared_ptr<Input>>{ sources.begin(), sources.end() }, _sessiondata->_settings->generation.allowBacktrackFailedInputs, 1, 2 /*cannot backtrack on length 1*/);
						for (auto input : vec) {
							sources.insert(input);
						}
					}
				}
				break;
			case Settings::GenerationSourcesType::FilterPrimaryScoreRelative:
				{
					auto topk = _sessiondata->GetTopK(1);

					if (topk.size() > 0)
						max = topk[0]->GetPrimaryScore();
					// if the max is 0 just run loop a single time since 0 is the minimal value for primaryScore
					if (max == 0.0f)
						frac = 1.0f;

					FilterFunctions::FilterSet<InputGainGreaterPrimary>(_sessiondata, lastgen, sources, max, frac, false, true, _sessiondata->_settings->generation.numberOfSourcesPerGeneration);

					if ((int32_t)sources.size() < _sessiondata->_settings->generation.numberOfSourcesPerGeneration) {
						auto vec = _sessiondata->FindKSources(_sessiondata->_settings->generation.numberOfSourcesPerGeneration - (int32_t)sources.size(), std::set<std::shared_ptr<Input>>{ sources.begin(), sources.end() }, _sessiondata->_settings->generation.allowBacktrackFailedInputs, 1, 2 /*cannot backtrack on length 1*/);
						for (auto input : vec) {
							sources.insert(input);
						}
					}
				}
				break;
			case Settings::GenerationSourcesType::FilterPrimaryScore:
				{
					auto topk = _sessiondata->GetTopK(1);

					if (topk.size() > 0)
						max = topk[0]->GetPrimaryScore();
					// if the max is 0 just run loop a single time since 0 is the minimal value for primaryScore
					if (max == 0.0f)
						frac = 1.0f;

					FilterFunctions::FilterSet<InputGreaterPrimary>(_sessiondata, lastgen, sources, max, frac, false, true, _sessiondata->_settings->generation.numberOfSourcesPerGeneration);

					if ((int32_t)sources.size() < _sessiondata->_settings->generation.numberOfSourcesPerGeneration) {
						auto vec = _sessiondata->FindKSources(_sessiondata->_settings->generation.numberOfSourcesPerGeneration - (int32_t)sources.size(), std::set<std::shared_ptr<Input>>{ sources.begin(), sources.end() }, _sessiondata->_settings->generation.allowBacktrackFailedInputs, 1, 2 /*cannot backtrack on length 1*/);
						for (auto input : vec) {
							sources.insert(input);
						}
					}
				}
				break;
			case Settings::GenerationSourcesType::FilterSecondaryScoreRelative:
				{
					auto topk = _sessiondata->GetTopK_Secondary(1);
					if (topk.size() > 0)
						max = topk[0]->GetSecondaryScore();
					// if the max is 0 just run loop a single time since 0 is the minimal value for primaryScore
					if (max == 0.0f)
						frac = 1.0f;

					FilterFunctions::FilterSet<InputGainGreaterSecondary>(_sessiondata, lastgen, sources, max, frac, false, false, _sessiondata->_settings->generation.numberOfSourcesPerGeneration);

					if ((int32_t)sources.size() < _sessiondata->_settings->generation.numberOfSourcesPerGeneration) {
						auto vec = _sessiondata->FindKSources(_sessiondata->_settings->generation.numberOfSourcesPerGeneration - (int32_t)sources.size(), std::set<std::shared_ptr<Input>>{ sources.begin(), sources.end() }, _sessiondata->_settings->generation.allowBacktrackFailedInputs, 1, 2 /*cannot backtrack on length 1*/);
						for (auto input : vec) {
							sources.insert(input);
						}
					}
				}
				break;
			case Settings::GenerationSourcesType::FilterSecondaryScore:
				{
					auto topk = _sessiondata->GetTopK_Secondary(1);
					if (topk.size() > 0)
						max = topk[0]->GetSecondaryScore();
					// if the max is 0 just run loop a single time since 0 is the minimal value for primaryScore
					if (max == 0.0f)
						frac = 1.0f;

					FilterFunctions::FilterSet<InputGreaterSecondary>(_sessiondata, lastgen, sources, max, frac, false, false, _sessiondata->_settings->generation.numberOfSourcesPerGeneration);

					if ((int32_t)sources.size() < _sessiondata->_settings->generation.numberOfSourcesPerGeneration) {
						auto vec = _sessiondata->FindKSources(_sessiondata->_settings->generation.numberOfSourcesPerGeneration - (int32_t)sources.size(), std::set<std::shared_ptr<Input>>{ sources.begin(), sources.end() }, _sessiondata->_settings->generation.allowBacktrackFailedInputs, 1, 2 /*cannot backtrack on length 1*/);
						for (auto input : vec) {
							sources.insert(input);
						}
					}
				}
				break;
			}

			// we now have the set of new sources
			auto gen = _sessiondata->GetCurrentGeneration();
			for (auto input : sources)
				gen->AddSource(input);
			gen->SetActive();
			generation->SetInactive();
			_sessiondata->_generationFinishing = false;

			// now we can continue with our generation as normal
			int32_t maxtasks = std::min((int32_t)(_sessiondata->_controller->GetHeavyThreadCount() * 0.8), _sessiondata->_settings->generation.activeGeneratedInputs / _sessiondata->_settings->generation.generationstep);
			if (maxtasks < 1)
				maxtasks = 1;
			if (maxtasks == _sessiondata->_controller->GetHeavyThreadCount() && maxtasks != 1)
				maxtasks = _sessiondata->_controller->GetHeavyThreadCount() - 1;
			for (int32_t i = 0; i < maxtasks; i++)
				SessionFunctions::GenerateTests(_sessiondata);
			loginfo("New generation begun.");

			// release writers lock for input generation
			_sessiondata->Release_InputGenerationWritersLock();

			profile(TimeProfiling, "Time taken for sources selection.");
		}
	}

	std::shared_ptr<BaseFunction> GenerationFinishedCallback::DeepCopy()
	{
		auto ptr = std::make_shared<GenerationFinishedCallback>();
		ptr->_sessiondata = _sessiondata;
		ptr->_generation = _generation;
		ptr->_afterSave = _afterSave;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	bool GenerationFinishedCallback::ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		uint64_t genid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, genid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
			this->_generation = resolver->ResolveFormID<Generation>(genid);
		});
		_afterSave = Buffer::ReadBool(buffer, offset);
		return true;
	}

	bool GenerationFinishedCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		uint64_t genid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, genid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
			this->_generation = resolver->ResolveFormID<Generation>(genid);
		});
		_afterSave = Buffer::ReadBool(buffer, offset);
		return true;
	}

	bool GenerationFinishedCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_generation->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_afterSave, buffer, offset);                 // +1
		return true;
	}

	unsigned char* GenerationFinishedCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_generation->GetFormID(), buffer, offset);   // +8
		Buffer::Write(_afterSave, buffer, offset);                 // +1
		size = GetLength();
		return buffer;
	}

	size_t GenerationFinishedCallback::GetLength()
	{
		return BaseFunction::GetLength() + 8 + 8+ 1;
	}

	void GenerationFinishedCallback::Dispose()
	{
		_sessiondata.reset();
		_generation.reset();
	}

	
	void FinishSessionCallback::Run()
	{
		SessionFunctions::MasterControl(_sessiondata, true);
	}
	
	std::shared_ptr<BaseFunction> FinishSessionCallback::DeepCopy()
	{
		auto ptr = std::make_shared<FinishSessionCallback>();
		ptr->_sessiondata = _sessiondata;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	bool FinishSessionCallback::ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
		});
		return true;
	}

	bool FinishSessionCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of sessiondata and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
		});
		return true;
	}

	bool FinishSessionCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		return true;
	}

	unsigned char* FinishSessionCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		size = GetLength();
		return buffer;
	}

	size_t FinishSessionCallback::GetLength()
	{
		return BaseFunction::GetLength() + 8;
	}

	void FinishSessionCallback::Dispose()
	{
		_sessiondata.reset();
	}

}
