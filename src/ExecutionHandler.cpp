#include "ExecutionHandler.h"
#include "Input.h"
#include "Oracle.h"
#include "Settings.h"
#include "Processes.h"
#include "Logging.h"
#include "Session.h"
#include "Data.h"
#include "BufferOperations.h"
#include "LuaEngine.h"
#include "SessionFunctions.h"
#include "Generation.h"
//#include <io.h>

ExecutionHandler::ExecutionHandler()
{
}

void ExecutionHandler::Init(std::shared_ptr<Session> session, std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Settings> settings, std::shared_ptr<TaskController> threadpool, int32_t maxConcurrentTests, std::shared_ptr<Oracle> oracle)
{
	loginfo("Init execution handler");
	_maxConcurrentTests = maxConcurrentTests > 0 ? maxConcurrentTests : 1;
	_populationSize = _maxConcurrentTests * 2;
	_sessiondata = sessiondata;
	_threadpool = threadpool;
	_settings = settings;
	_session = session;
	_oracle = oracle;
}

ExecutionHandler* ExecutionHandler::GetSingleton()
{
	static ExecutionHandler session;
	return std::addressof(session);
}

#if defined(unix) || defined(__unix__) || defined(__unix)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	pragma warning(push)
#	pragma warning(disable: 4100)
#endif
void ExecutionHandler::SetEnableFragments(bool enable)
{
#if defined(unix) || defined(__unix__) || defined(__unix)
	_enableFragments = enable;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	_enableFragments = false;
	logcritical("Fragments are not available under windows");
#endif
}
#if defined(unix) || defined(__unix__) || defined(__unix)
#	pragma GCC diagnostic pop
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	pragma warning(pop)
#endif

void ExecutionHandler::Clear()
{
	_cleared = true;
	while (!_waitingTests.empty())
	{
		std::weak_ptr<Test> test = _waitingTests.front();
		_waitingTests.pop_front();
		if (auto ptr = test.lock(); ptr && _session->data)
			_session->data->DeleteForm(ptr);
	}
	while (!_waitingTestsExec.empty()) {
		std::weak_ptr<Test> test = _waitingTestsExec.front();
		_waitingTestsExec.pop_front();
		if (auto ptr = test.lock(); ptr && _session->data)
			_session->data->DeleteForm(ptr);
	}
	for (auto test : _runningTests)
	{
		if (auto ptr = test.lock(); ptr) {
			ptr->KillProcess();
			StopTest(ptr);
		}
	}
	_runningTests.clear();
	for (auto test : _stoppingTests)
	{
		StopTest(test);
	}
	_session.reset();
	_settings.reset();
	_threadpool.reset();
	_oracle.reset();
	_stoppingTests.clear();
	_currentTests = 0;
	_active = false;
	_nextid = 1;
	_enableFragments = false;
	_stopHandler = false;
	_finishtests = false;
	_thread = {};
}

void ExecutionHandler::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
		Functions::RegisterFactory(Functions::ExecInitTestsCallback::GetTypeStatic(), Functions::ExecInitTestsCallback::Create);
	}
}


void ExecutionHandler::SetMaxConcurrentTests(int32_t maxConcurrenttests)
{
	loginfo("Set max concurrent tests to {}", maxConcurrenttests);
	_maxConcurrentTests = maxConcurrenttests > 0 ? maxConcurrenttests : 1;
	_populationSize = _maxConcurrentTests * 2;
}

void ExecutionHandler::StartHandlerAsIs()
{
	if (_cleared) {
		logcritical("Cannot Start Handler on a cleared class.");
		return;
	}
	loginfo("Start execution handler");
	// check that the handler isn't active already
	{
		std::unique_lock<std::mutex> guard(_toplevelsync);
		if (_active) {
			logcritical("ExecutionHandler is already running");
			return;
		} else {
			_active = true;
			loginfo("Started Execution Handler");
		}
	}
	// start thread
	_thread = std::thread(&ExecutionHandler::InternalLoop, this);
	_thread.detach();
}

void ExecutionHandler::StartHandler()
{
	if (_cleared) {
		logcritical("Cannot Start Handler on a cleared class.");
		return;
	}
	loginfo("Start execution handler");
	// check that the handler isn't active already
	{
		std::unique_lock<std::mutex> guard(_toplevelsync);
		if (_active) {
			logcritical("ExecutionHandler is already running");
			return;
		} else {
			_active = true;
			loginfo("Started Execution Handler");
		}
	}
	// reset internal class values
	{
		std::unique_lock<std::mutex> guard(_lockqueue);
		_currentTests = 0;
		// delete all waiting tests
		while (_waitingTests.empty() == false) {
			auto weak = _waitingTests.front();
			_waitingTests.pop_front();
			if (auto test = weak.lock(); test) {
				if (auto ptr = test->_input.lock(); ptr) {
					ptr->_hasfinished = true;
					ptr->test = test;
				} else {
					logwarn("ptr invalid");
				}
				test->_exitreason = Test::ExitReason::InitError;
				SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::InitError);
				test->_input.reset();
				// call _callback if test has finished
				_threadpool->AddTask(test->_callback);
			}
		}
		// delete all waiting initialized tests
		while (_waitingTestsExec.empty() == false) {
			auto weak = _waitingTestsExec.front();
			_waitingTestsExec.pop_front();
			if (auto test = weak.lock(); test) {
				if (auto ptr = test->_input.lock(); ptr) {
					ptr->_hasfinished = true;
					ptr->test = test;
				} else {
					logwarn("ptr invalid");
				}
				test->_exitreason = Test::ExitReason::InitError;
				SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::InitError);
				test->InValidate();
				test->_input.reset();
				// call _callback if test has finished
				_threadpool->AddTask(test->_callback);
			}
		}
		// delete all running tests
		for (std::weak_ptr<Test> weak : _runningTests) {
			if (auto test = weak.lock(); test) {
				test->KillProcess();
				if (auto ptr = test->_input.lock(); ptr) {
					ptr->_hasfinished = true;
					ptr->test = test;
				} else {
					logwarn("ptr invalid");
				}
				test->_exitreason = Test::ExitReason::InitError;
				SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::InitError);
				test->InValidate();
				test->_input.reset();
				// call _callback if test has finished
				_threadpool->AddTask(test->_callback);
			}
		}
		_runningTests.clear();
		// wake anything that may sleep here
		_waitforhandler.notify_all();
		_waitforjob.notify_all();
		// don't reset nextid, as it should be unique as long as the class exists
		// _nextid = 0;
		_stopHandler = false;
		_finishtests = false;
		_thread = {};
	}
	// start thread
	_thread = std::thread(&ExecutionHandler::InternalLoop, this);
	_thread.detach();
}

void ExecutionHandler::StopHandler()
{
	_stopHandler = true;
	_waitforjob.notify_all();
}

void ExecutionHandler::WaitOnHandler()
{
	std::unique_lock<std::mutex> guard(_toplevelsync);
	if (!_active)
		return;
	else {
		_waitforhandler.wait(guard);
		return;
	}
}

void ExecutionHandler::StopHandlerAfterTestsFinishAndWait()
{
	_finishtests = true;
	_stopHandler = true;
	std::unique_lock<std::mutex> guard(_toplevelsync);
	if (!_active)
		return;
	else {
		_waitforhandler.wait(guard);
		return;
	}
}

bool ExecutionHandler::AddTest(std::shared_ptr<Input> input, std::shared_ptr<Functions::BaseFunction> callback, bool bypass, bool replay)
{
	StartProfilingDebug;
	loginfo("Adding new test");
	if (input->GetGenerated() == false)
	{
		// we are trying to add an _input that hasn't been generated or regenerated
		// try the generate it and if it succeeds add the test
		SessionFunctions::GenerateInput(input, _sessiondata);
		if (input->GetGenerated() == false)
			return false;
	}
	if (input->Length() == 0)
		return false;
	uint64_t id = 0;
	{
		std::unique_lock<std::mutex> guard(_lockqueue);
		id = _nextid++;
	}
	std::shared_ptr<Test> test = _sessiondata->data->CreateForm<Test>();
	test->Init(callback, id);
	if (test->_exitreason & Test::ExitReason::InitError) {
		_sessiondata->data->DeleteForm(test);
		return false;
	}
	test->_running = false;
	test->_input = input;
	test->_itr = test->_input.lock()->begin();
	test->_itrend = test->_input.lock()->end();
	test->_reactiontime = {};
	test->_output = "";
	if (replay)
		test->_skipExclusionCheck = true;

	bool stateerror = false;
	test->_cmdArgs = Lua::GetCmdArgs(std::bind(&Oracle::GetCmdArgs, _oracle, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), test, stateerror, replay);
	if (_oracle->GetOracletype() == Oracle::PUTType::Script)
		test->_scriptArgs = Lua::GetScriptArgs(std::bind(&Oracle::GetScriptArgs, _oracle, std::placeholders::_1, std::placeholders::_2), test, stateerror);
	if (stateerror) {
		logcritical("Add Test cannot be completed, as the calling thread lacks a lua context.");
		_sessiondata->data->DeleteForm(test);
		return false;
	}
	{
		std::unique_lock<std::mutex> guard(_lockqueue);
		if (bypass)
			_waitingTests.push_front(test);
		else
			_waitingTests.push_back(test);
	}
	_waitforjob.notify_one();
	profileDebug(TimeProfilingDebug, "");
	return true;
}

void ExecutionHandler::InitTests()
{
	while (_waitingTestsExec.size() < _populationSize && _waitingTests.size() > 0)
	{
		std::weak_ptr<Test> ptest;
		{
			std::unique_lock<std::mutex> guard(_lockqueue);
			if (_waitingTests.size() > 0) {
				ptest = _waitingTests.front();
				_waitingTests.pop_front();
			} else
				return;
		}
		if (auto test = ptest.lock(); test) {
			test->PrepareForExecution();
			// if we cannot initialize pipes and status just call callback and be done with it
			if (test->_exitreason & Test::ExitReason::InitError) {
				if (auto ptr = test->_input.lock(); ptr) {
					ptr->_hasfinished = true;
					ptr->test = test;
				} else {
					logwarn("ptr invalid");
				}
				_threadpool->AddTask(test->_callback);
				logdebug("test cannot be initialized");
				return;
			}
			{
				std::unique_lock<std::mutex> guard(_lockqueue);
				_waitingTestsExec.push_back(test);
			}
		}
	}
}

void ExecutionHandler::InitTestsLockFree()
{
	while (_waitingTestsExec.size() < _populationSize && _waitingTests.size() > 0) {
		std::weak_ptr<Test> ptest = _waitingTests.front();
		_waitingTests.pop_front();
		if (auto test = ptest.lock(); test) {
			test->PrepareForExecution();
			// if we cannot initialize pipes and status just call callback and be done with it
			if (test->_exitreason & Test::ExitReason::InitError) {
				if (auto ptr = test->_input.lock(); ptr) {
					ptr->_hasfinished = true;
					ptr->test = test;
				} else {
					logwarn("ptr invalid");
				}
				_threadpool->AddTask(test->_callback);
				logdebug("test cannot be initialized");
				return;
			}
			_waitingTestsExec.push_back(test);
		}
	}
}

bool ExecutionHandler::StartTest(std::shared_ptr<Test> test)
{
	StartProfilingDebug;
	// everything that has less than 50 list entries is very likely to be already run when for 
	// for instance using delta debugging, so just check again to be sure we haven't run it already
	if (auto ptr = test->_input.lock(); test->_skipExclusionCheck == false && ptr && ptr->Length() <= 50)
	{
		FormID prefixID = 0;
		if (_sessiondata->_excltree->HasPrefix(ptr, prefixID) && prefixID != 0)
		{
			auto prefix = _sessiondata->data->LookupFormID<Input>(prefixID);
			if (prefix)
			{
				prefix->DeepCopy(ptr);
				auto callback = test->_callback;
				test->InValidatePreExec();
				prefix->test->DeepCopy(test);
				test->_callback = callback;
				test->_skipOracle = true;
				ptr->test = test;
				ptr->_hasfinished = true;
				_threadpool->AddTask(test->_callback);
				logdebug("test is duplicate");
				ptr->SetFlag(Input::Flags::Duplicate);
				profileDebug(TimeProfilingDebug, "");
				_sessiondata->IncGeneratedWithPrefix();
				// inform generation that an inut has finished if applicable
				if (ptr->HasFlag(Input::Flags::GeneratedGrammar))
					_sessiondata->GetCurrentGeneration()->FailGeneration(1);
				return false;
			}
		}
	}

	logdebug("start test {}", uintptr_t(test.get()));
	// start test
	// if successful: add test to list of active tests and update _currentTests
	// give test its first _input on startup
	// if unsuccessful: report error and complete test-case as failed
	test->_starttime = std::chrono::steady_clock::now();
	if (!Processes::StartPUTProcess(test, _oracle->path().string(), test->_cmdArgs))
	{
		test->_exitreason = Test::ExitReason::InitError;
		SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::InitError);
		if (auto ptr = test->_input.lock(); ptr) {
			ptr->_hasfinished = true;
			// invalidate so no more functions can be called on the test
			test->InValidate();
			// give _input access to test information
			ptr->test = test;
			// inform generation that an inut has finished if applicable
			if (ptr->HasFlag(Input::Flags::GeneratedGrammar)) {
				_sessiondata->GetCurrentGeneration()->FailGeneration(1);
			}
		} else {
			logwarn("ptr invalid");
		}
		// call _callback if test has finished
		_threadpool->AddTask(test->_callback);
		logdebug("test cannot be started");
		profileDebug(TimeProfilingDebug, "");
		return false;
	}
	logdebug("started process");
	if (_oracle->GetOracletype() != Oracle::PUTType::CMD && _oracle->GetOracletype() != Oracle::PUTType::Script) {
		if (_enableFragments)
			test->WriteNext();
		else
			test->WriteAll();
	} else if (_oracle->GetOracletype() == Oracle::PUTType::Script) {
		// if it is a script we can dump a special string onto the stdin of the put
		test->WriteInput(test->_scriptArgs);
	}
	_currentTests++;
	_runningTests.push_back(test);
	logdebug("test started");
	profileDebug(TimeProfilingDebug, "");
	return true;
}

void ExecutionHandler::StopTest(std::shared_ptr<Test> test)
{
	// clean up _input length (cut the sequence to last executed
	if (_enableFragments) {
		test->TrimInput();
	}
	if (auto ptr = test->_input.lock(); ptr) {
		// write back test results to _input
		ptr->_hasfinished = true;
		ptr->_executiontime = std::chrono::duration_cast<std::chrono::nanoseconds>(test->_endtime - test->_starttime);
		ptr->_exitcode = test->GetExitCode();
		// give _input access to test information
		ptr->test = test;
		// inform generation that an inut has finished if applicable
		if (ptr->HasFlag(Input::Flags::GeneratedGrammar))
			_sessiondata->GetCurrentGeneration()->SetInputCompleted();
	} else {
		logwarn("ptr invalid");
	}
	// invalidate so no more functions can be called on the test
	test->InValidate();
	// call _callback if test has finished
	_threadpool->AddTask(test->_callback);
	_currentTests--;
}

void ExecutionHandler::InternalLoop()
{
	// time_point used to record enter times and to calculate timeouts
	auto time = std::chrono::steady_clock::now();
	// time of last iteration
	auto lasttime = time;
	// difference between last and current period
	std::chrono::nanoseconds diff;
	// tmp diff between two fragments
	std::chrono::microseconds difffrag;
	// time giver for period
	auto steady = time;
	// time diff between last cycle and this one doesn't exist. -1 is the value for unknown #Don'tApplyTimeouts
	int64_t nano = -1; 
	// used for sleep time calculation
	std::chrono::nanoseconds sleep = std::chrono::nanoseconds(-1);
	// tmp test var
	std::weak_ptr<Test> test;
	logdebug("Entering loop");
	int32_t newtests = 0;
	while (_stopHandler == false || _finishtests) {
		StartProfiling;
		logdebug("find new tests");
		if (_freeze)
			_frozen = true;
		else {
			// when not in freeze anymore, but still frozen, we have to catch up on tests that should have been stopped
			if (_frozen)
			{
				_frozen = false;
				for (auto tst : _stoppingTests)
					StopTest(tst);
				_stoppingTests.clear();
			}
		}
		newtests = 0;
		// while we are not at the max concurrent tests, there are tests waiting to be executed and we are not FROZEN
		while (_currentTests < _maxConcurrentTests && (_waitingTestsExec.size() > 0 || _waitingTests.size() > 0) && !_frozen) {
			test.reset();
			{
				std::unique_lock<std::mutex> guard(_lockqueue);
				if (_waitingTestsExec.size() > 0) {
					test = _waitingTestsExec.front();
					_waitingTestsExec.pop_front();
				}
				else
				{
					// if there aren't any tests ready for execution grab some that aren't and make them ready
					InitTestsLockFree();
					if (_waitingTestsExec.size() > 0) {
						test = _waitingTestsExec.front();
						_waitingTestsExec.pop_front();
					}
				}
			}
			if (auto ptr = test.lock(); ptr) {
				StartTest(ptr);
				//if (StartTest(ptr) == false)
				//	_internalwaitingTests.push_back(ptr);
			}
			newtests++;
		}
		if (newtests > 0)
			_threadpool->AddTask(Functions::ExecInitTestsCallback::CreateFull(_sessiondata));

		lasttime = time;
		time = std::chrono::steady_clock::now();  // record the enter time
		diff = std::chrono::duration_cast<std::chrono::nanoseconds>(time - lasttime);
		logdebug("time diff: {}ns", diff.count());
		// when applying timeouts we must take the timedifference between cycles into account
		// if we are simply slower in computation

		// exit condition wait until tests have finished
		if (_currentTests == 0 && _stopHandler == true && _finishtests == true)
		{
			loginfo("Finished all tests -> Exiting Handler");
			goto ExitHandler;
		}

		// check if we are running tests, if not wait until we are given one to run
		// if we found some above it was started and this should be above 0
		if (_currentTests == 0) {
			logdebug("no tests active -> wait for new tests");
			std::unique_lock<std::mutex> guard(_lockqueue);
			_waitforjob.wait_for(guard, std::chrono::seconds(1), [this] { return !_waitingTests.empty() || !_waitingTestsExec.empty() || _stopHandler; });
			if (_stopHandler && _finishtests == false) {
				profile(TimeProfiling, "Round");
				break;
			} else {
				profile(TimeProfiling, "Round");
				continue;
			}
		}
		logdebug("Handling running tests: {}", _currentTests);
		auto itr = _runningTests.begin();
		while (itr != _runningTests.end()) {
			test = *itr;
			if (auto ptr = test.lock(); ptr) {
				logdebug("Handling test {}", ptr->_identifier);
				// read _output accumulated in the mean-time
				// if process has ended there still may be something left over to read anyway
				ptr->_output += ptr->ReadOutput();
				logdebug("Read Output {}", ptr->_identifier);
				// check for running
				if (ptr->IsRunning() == false) {
					// test has finished. Get exit code and check end conditions
					ptr->_exitreason = Test::ExitReason::Natural;
					SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Natural);
					goto TestFinished;
				}
				// check for memory consumption
				if (_settings->tests.maxUsedMemory != 0) {
					// memory limitation enabled

					if (ptr->GetMemoryConsumption() > _settings->tests.maxUsedMemory) {
						ptr->KillProcess();
						// process killed, now set flags for oracle
						ptr->_exitreason = Test::ExitReason::Memory;
						SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Memory);
						goto TestFinished;
					}
				}
				// check for fragment completion
				if (_enableFragments && ptr->CheckInput()) {
					// fragment has been completed
					if (ptr->WriteNext() == false && _settings->tests.use_fragmenttimeout && std::chrono::duration_cast<std::chrono::microseconds>(time - ptr->_lasttime).count() > _settings->tests.fragmenttimeout) {
						ptr->_exitreason = Test::ExitReason::Natural | Test::ExitReason::LastInput;
						SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Natural);
						SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::LastInput);
						goto TestFinished;
					}
				}
				logdebug("Checked Input {}", ptr->_identifier);
				// compute timeouts
				if (_enableFragments && _settings->tests.use_fragmenttimeout) {
					difffrag = std::chrono::duration_cast<std::chrono::microseconds>(time - ptr->_lasttime);
					if (difffrag.count() > _settings->tests.fragmenttimeout) {
						ptr->KillProcess();
						ptr->_exitreason = Test::ExitReason::FragmentTimeout;
						SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::FragmentTimeout);
						goto TestFinished;
					}
				}
				if (_settings->tests.use_testtimeout) {
					difffrag = std::chrono::duration_cast<std::chrono::microseconds>(time - ptr->_starttime);
					if (difffrag.count() > _settings->tests.testtimeout) {
						ptr->KillProcess();
						ptr->_exitreason = Test::ExitReason::Timeout;
						SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Timeout);
						goto TestFinished;
					}
				}
				goto TestRunning;
TestFinished:
				{
					logdebug("Test {} has ended", ptr->_identifier);
					// if not frozen, stop test
					if (!_frozen)
						StopTest(ptr);
					// otherwise save test to stop it later
					else {
						std::unique_lock<std::mutex> guard(_freezelock);
						_stoppingTests.push_back(ptr);
					}
					// delete test from list
					itr = _runningTests.erase(itr);
					continue;
				}
TestRunning:;
			} else {
				itr = _runningTests.erase(itr);
				continue;
			}
			itr++;
		}
		// reset vars
		test.reset();

		// calculate the difference between the cycle period and what we used doing the cycle
		sleep = _waittime - std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - steady);
		// if we wasted large amounts of time, we would have a "catch up" effect with just this calculation
		// we update steady to last period skipped, so that we go forward
		if (sleep.count() >> (sizeof(sleep.count()) * CHAR_BIT - 1)) [[likely]] {
			// positive number: just advance period
			steady += _waittime;
		} else {
			// negative number: our value is the time we are hanging behind. Computer last actual period time
			steady += std::chrono::nanoseconds((abs(sleep.count()) / _waittimeL) * _waittimeL);
		}
		// go to sleep until the next period.
		logdebug("sleeping for {} ns", sleep.count());
		profile(TimeProfiling, "Round");
		std::this_thread::sleep_for(sleep);
	}

	// we will only get here once we terminate the handler
//KillRunningTests:

	// kill all running tests
	for (auto tst : _runningTests) {
		if (auto ptr = tst.lock(); ptr)
			ptr->KillProcess();
	}
	_runningTests.clear();
	
ExitHandler:
	// mark thread as inactive
	{
		logdebug("Exiting thread");
		std::unique_lock<std::mutex> guard(_toplevelsync);
		if (!_active) {
			logcritical("ExecutionHandler is marked as finished, before actually finishing");
			return;
		} else {
			_active = false;
			_waitforhandler.notify_all();
			loginfo("Finished Execution Handler");
		}
	}
}

void ExecutionHandler::Freeze()
{
	loginfo("Freezing execution...");
	_freeze = true;
	while (_frozen == false && _active == true)
		;
	loginfo("Frozen execution.");
}

void ExecutionHandler::Thaw()
{
	loginfo("Thawing execution...");
	_freeze = false;
	loginfo("Resumed execution.");
}

int32_t ExecutionHandler::GetWaitingTests()
{
	return (int32_t)_waitingTests.size();
}

int32_t ExecutionHandler::GetInitializedTests()
{
	return (int32_t)_waitingTestsExec.size();
}

int32_t ExecutionHandler::GetRunningTests()
{
	return (int32_t)_runningTests.size();
}

int32_t ExecutionHandler::GetStoppingTests()
{
	return (int32_t)_stoppingTests.size();
}

size_t ExecutionHandler::GetStaticSize(int32_t version)
{
	static size_t size0x1 = Form::GetDynamicSize()  // form base size
	                        + 4                     // version
	                        + 1                     // enableFragments
	                        + 8                     // waittimeL
	                        + 1                     // cleared
	                        + 8                     // nextid
	                        + 1                     // active
	                        + 4;                    // maxConcurrentTests
	switch (version) {
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t ExecutionHandler::GetDynamicSize()
{
	return GetStaticSize(classversion) + 8 /*len of ids*/ + _stoppingTests.size() * 8 + _runningTests.size() * 8 + _waitingTests.size() * 8 + _waitingTestsExec.size() * 8;
}

bool ExecutionHandler::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(_enableFragments, buffer, offset);
	Buffer::Write(_waittimeL, buffer, offset);
	Buffer::Write(_cleared, buffer, offset);
	Buffer::Write(_nextid, buffer, offset);
	Buffer::Write(_active, buffer, offset);
	Buffer::Write(_maxConcurrentTests, buffer, offset);
	// _waitingtests
	// save all tests running and waiting as waiting tests, since we cannot solve external programs
	Buffer::WriteSize(_waitingTests.size() + _waitingTestsExec.size() + _runningTests.size() + _stoppingTests.size(), buffer, offset);
	for (auto test : _runningTests) {
		if (auto ptr = test.lock(); ptr)
			Buffer::Write(ptr->GetFormID(), buffer, offset);
		else
			Buffer::Write((uint64_t)0, buffer, offset);
	}
	for (auto ptr : _stoppingTests) {
		Buffer::Write(ptr->GetFormID(), buffer, offset);
	}
	for (auto test : _waitingTests) {
		if (auto ptr = test.lock(); ptr)
			Buffer::Write(ptr->GetFormID(), buffer, offset);
		else
			Buffer::Write((uint64_t)0, buffer, offset);
	}
	for (auto test : _waitingTestsExec) {
		if (auto ptr = test.lock(); ptr)
			Buffer::Write(ptr->GetFormID(), buffer, offset);
		else
			Buffer::Write((uint64_t)0, buffer, offset);
	}
	return true;
}

bool ExecutionHandler::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version)
	{
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			_enableFragments = Buffer::ReadBool(buffer, offset);
			_waittimeL = Buffer::ReadInt64(buffer, offset);
			_waittime = std::chrono::nanoseconds(_waittimeL);
			_cleared = Buffer::ReadBool(buffer, offset);
			_nextid = Buffer::ReadUInt64(buffer, offset);
			_active = Buffer::ReadBool(buffer, offset);
			_maxConcurrentTests = Buffer::ReadInt32(buffer, offset);
			size_t len = Buffer::ReadSize(buffer, offset);
			std::vector<FormID> ids;
			for (int32_t i = 0; i < (int32_t)len; i++)
				ids.push_back(Buffer::ReadUInt64(buffer, offset));
			resolver->AddLateTask([this, ids, resolver]() {
				bool stateerror = false;
				_sessiondata = resolver->_data->CreateForm<SessionData>();
				_session = resolver->_data->CreateForm<Session>();
				_settings = resolver->_data->CreateForm<Settings>();
				_threadpool = resolver->_data->CreateForm<TaskController>();
				_oracle = resolver->_data->CreateForm<Oracle>();
				for (int32_t i = 0; i < (int32_t)ids.size(); i++) {
					if (ids[i] != 0) {
						auto test = resolver->ResolveFormID<Test>(ids[i]);
						test->Init(test->_callback, test->_identifier);
						// since we found a test we still need to execute, we need to make sure that our _input contains a valid _input sequence
						// and regenerate it if not
						if (auto ptr = test->_input.lock(); ptr && ptr->GetGenerated() == false)
							SessionFunctions::GenerateInput(ptr, this->_sessiondata);
						test->_cmdArgs = Lua::GetCmdArgs(std::bind(&Oracle::GetCmdArgs, _oracle, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), test, stateerror, false);
						if (_oracle->GetOracletype() == Oracle::PUTType::Script)
							test->_scriptArgs = Lua::GetScriptArgs(std::bind(&Oracle::GetScriptArgs, _oracle, std::placeholders::_1, std::placeholders::_2), test, stateerror);
						_waitingTests.push_back(test);
					}
					else
						logcritical("ExecutionHandler::Load cannot resolve test");
				}
				// make the first batch of tests ready for execution
				InitTests();
				if (_active) {
					_active = false;
				}
			});
		return true;
		}
		break;
	default:
		return false;
	}
}

void ExecutionHandler::Delete(Data*)
{
	StopHandler();
	Clear();
}



namespace Functions
{
	void ExecInitTestsCallback::Run()
	{
		_sessiondata->_exechandler->InitTests();
	}

	bool ExecInitTestsCallback::ReadData(unsigned char*, size_t&, size_t, LoadResolver* resolver)
	{
		// get id of saved controller and resolve link
		resolver->AddTask([this, resolver]() {
			this->_sessiondata = resolver->_data->CreateForm<SessionData>();
		});
		return true;
	}

	bool ExecInitTestsCallback::WriteData(unsigned char* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		return true;
	}

	size_t ExecInitTestsCallback::GetLength()
	{
		return BaseFunction::GetLength();
	}

	void ExecInitTestsCallback::Dispose()
	{
		_sessiondata.reset();
	}

	std::shared_ptr<BaseFunction> ExecInitTestsCallback::CreateFull(std::shared_ptr<SessionData> sessiondata)
	{
		auto ptr = std::make_shared<ExecInitTestsCallback>();
		ptr->_sessiondata = sessiondata;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}
}
