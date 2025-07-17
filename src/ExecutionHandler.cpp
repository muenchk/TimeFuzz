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
#include "IPCommManager.h"
#include "Settings.h"
#include <functional>
//#include <io.h>

ExecutionHandler::ExecutionHandler()
{
}
ExecutionHandler::~ExecutionHandler()
{
	Clear();
	if (_loadData)
		delete _loadData;
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
	if (_threadStopToken)
		_threadStopToken->request_stop();

	if (_thread.joinable())
		_thread.detach();
	_waitforjob.notify_all();
	_thread = {};
	if (_threadTSStopToken)
		_threadTSStopToken->request_stop();
	if (_threadTS.joinable())
		_threadTS.detach();
	_startingLockCond.notify_all();
	_threadTS = {};
	if (_threadTestStopToken)
		_threadTestStopToken->request_stop();
	if (_threadTest.joinable())
		_threadTest.detach();
#if defined(unix) || defined(__unix__) || defined(__unix)
	_waitCond.notify_all();
#endif
	_threadTest = {};
	Form::ClearForm();
	_cleared = true;
	while (!_waitingTests.empty())
	{
		std::shared_ptr<Test> test = _waitingTests.front();
		_waitingTests.pop_front();
		if (test && _session->data)
			_session->data->DeleteForm(test);
	}
	while (!_waitingTestsExec.empty()) {
		std::shared_ptr<Test> test = _waitingTestsExec.front();
		_waitingTestsExec.pop_front();
		if (test && _session->data)
			_session->data->DeleteForm(test);
	}
	for (auto test : _runningTests) {
			test->KillProcess();
			StopTest(test);
	}
	_runningTests.clear();
	_handleTests.clear();
	while (_startingTests.size() > 0)
		_startingTests.pop();
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
	_stopHandler = true;
	_finishtests = false;
}

void ExecutionHandler::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
		Functions::RegisterFactory(Functions::ExecInitTestsCallback::GetTypeStatic(), Functions::ExecInitTestsCallback::Create);
		Functions::RegisterFactory(Functions::WriteTestInputCallback::GetTypeStatic(), Functions::WriteTestInputCallback::Create);
	}
}


void ExecutionHandler::SetMaxConcurrentTests(int32_t maxConcurrenttests)
{
	loginfo("Set max concurrent tests to {}", maxConcurrenttests);
	_maxConcurrentTests = maxConcurrenttests > 0 ? maxConcurrenttests : 1;
	_handleTests.resize(_maxConcurrentTests);
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
	_threadStopToken = std::make_shared<stop_token>();
	_threadTSStopToken = std::make_shared<stop_token>();
	_thread = std::thread(std::bind(&ExecutionHandler::InternalLoop, this, std::placeholders::_1), _threadStopToken);
	_threadTS = std::thread(std::bind(&ExecutionHandler::TestStarter, this, std::placeholders::_1), _threadTSStopToken);
	_threadTestStopToken = std::make_shared<stop_token>();
#if defined(unix) || defined(__unix__) || defined(__unix)
	_threadTest = std::thread(std::bind(&ExecutionHandler::TestWaiter, this, std::placeholders::_1), _threadTestStopToken);
#endif
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
			auto test = _waitingTests.front();
			_waitingTests.pop_front();
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
		// delete all waiting initialized tests
		while (_waitingTestsExec.empty() == false) {
			auto test = _waitingTestsExec.front();
			_waitingTestsExec.pop_front();
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
		// delete all running tests
		for (std::shared_ptr<Test> test : _runningTests) {
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
	_threadStopToken = std::make_shared<stop_token>();
	_threadTSStopToken = std::make_shared<stop_token>();
	_thread = std::thread(std::bind(&ExecutionHandler::InternalLoop, this, std::placeholders::_1), _threadStopToken);
	_threadTS = std::thread(std::bind(&ExecutionHandler::TestStarter, this, std::placeholders::_1), _threadTSStopToken);
	_threadTestStopToken = std::make_shared<stop_token>();
#if defined(unix) || defined(__unix__) || defined(__unix)
	_threadTest = std::thread(std::bind(&ExecutionHandler::TestWaiter, this, std::placeholders::_1), _threadTestStopToken);
#endif
}

void ExecutionHandler::ReinitHandler()
{
	_threadStopToken->request_stop();
	_stale = true;
	_threadStopToken = std::make_shared<stop_token>();
	_thread.detach();
	_thread = std::thread(std::bind(&ExecutionHandler::InternalLoop, this, std::placeholders::_1), _threadStopToken);
}

void ExecutionHandler::StopHandler()
{
	_stopHandler = true;
	_waitforjob.notify_all();
	_startingLockCond.notify_all();
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

bool ExecutionHandler::IsStale(std::chrono::milliseconds dur)
{
	return (std::chrono::steady_clock::now() - _lastExec) > dur;
}

bool ExecutionHandler::AddTest(std::shared_ptr<Input> input, std::shared_ptr<Functions::BaseFunction> callback, bool bypass, bool replay)
{
	StartProfilingDebug;

	if (!input) {
		logcritical("Trying to add empty test");
		return false;
	}
	if (!callback)
	{
		logcritical("Trying to add test with empty callback");
		return false;
	}
	loginfo("{}: Adding new test", Utility::PrintForm(input));

	if (input->GetGenerated() == false)
	{
		// we are trying to add an _input that hasn't been generated or regenerated
		// try the generate it and if it succeeds add the test
		SessionFunctions::GenerateInput(input, _sessiondata);
		if (input->GetGenerated() == false)
			return false;
	}
	if (input->GetSequenceLength() == 0)
		return false;

	uint64_t id = 0;
	{
		std::unique_lock<std::mutex> guard(_lockqueue);
		id = _nextid++;
	}
	std::shared_ptr<Test> test = _sessiondata->data->CreateForm<Test>();
	test->SetFlag(Form::FormFlags::DoNotFree);
	test->Init(callback, id);
	if (test->_exitreason & Test::ExitReason::InitError) {
		_sessiondata->data->DeleteForm(test);
		return false;
	}
	test->_running = false;
	test->_input = input;
	//input->test = test;
	test->_itr = test->_input.lock()->begin();
	test->_itrend = test->_input.lock()->end();
	test->_reactiontime = {};
	test->_output.reset();
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
	_waitforjob.notify_all();
	_startingLockCond.notify_all();
	profileDebug(TimeProfilingDebug, "");
	loginfo("{}: Finished adding new test", Utility::PrintForm(input));
	return true;
}

void ExecutionHandler::InitTests()
{
	while (_waitingTestsExec.size() < _populationSize && _waitingTests.size() > 0) {
		std::shared_ptr<Test> test;
		{
			std::unique_lock<std::mutex> guard(_lockqueue);
			if (_waitingTests.size() > 0) {
				test = _waitingTests.front();
				_waitingTests.pop_front();
			} else
				return;
		}
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

void ExecutionHandler::InitTestsLockFree()
{
	while (_waitingTestsExec.size() < _populationSize && _waitingTests.size() > 0) {
		std::shared_ptr<Test> test = _waitingTests.front();
		_waitingTests.pop_front();
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

bool ExecutionHandler::StartTest(std::shared_ptr<Test> test)
{
	StartProfilingDebug;
	// everything that has less than 50 list entries is very likely to be already run when for 
	// for instance using delta debugging, so just check again to be sure we haven't run it already
	if (auto ptr = test->_input.lock(); test->_skipExclusionCheck == false && ptr && ptr->GetSequenceLength() <= 50)
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
				// inform generation that an input has finished if applicable
				if (!ptr->HasFlag(Input::Flags::GeneratedDeltaDebugging))
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
			if (!ptr->HasFlag(Input::Flags::GeneratedDeltaDebugging)) {
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
	test->_running = true;
	test->_starttime = std::chrono::steady_clock::now();
	test->_timeouttime = test->_starttime + std::chrono::nanoseconds(_settings->tests.testtimeout);
	if (_oracle->GetOracletype() != Oracle::PUTType::CMD && _oracle->GetOracletype() != Oracle::PUTType::Script) {
		if (_enableFragments) {
			bool error = false;
			test->WriteNext(error);
		} else {
			//test->WriteAll();
			if (auto ptr = test->_input.lock(); ptr) {
				std::string all = "";
				auto itr = ptr->begin();
				while (itr != ptr->end())
				{
					all += *itr;
					itr++;
				}
				IPCommManager::GetSingleton()->Write(test, all.c_str(), 0, all.size());
			}
		}
	} else if (_oracle->GetOracletype() == Oracle::PUTType::Script) {
		// if it is a script we can dump a special string onto the stdin of the put
		//test->WriteInput(test->_scriptArgs, true);

#if defined(unix) || defined(__unix__) || defined(__unix)
		//if (test->_scriptArgs.size() >= PIPE_SIZE_LINUX) {
			auto callback = dynamic_pointer_cast<Functions::WriteTestInputCallback>(Functions::WriteTestInputCallback::Create());
			callback->_test = test;
			callback->length = test->_scriptArgs.size();
			callback->data = new char[test->_scriptArgs.size()];
			memcpy(callback->data, test->_scriptArgs.c_str(), test->_scriptArgs.size());
			_threadpool->AddTask(callback, true);
		//} else {
		
		//	IPCommManager::GetSingleton()->Write(test, test->_scriptArgs.c_str(), 0, test->_scriptArgs.size());
			/* size_t offset = 0;
			long written = 1;
			size_t length = test->_scriptArgs.size();
			while (written != 0 && length > 0) {
				written = test->Write(test->_scriptArgs.c_str(), offset, length);
				// update write information
				offset += written;
				length -= written;
				if (written == -1) {
					break;
				} else if (length <= 0) {
					break;
				}
			}*/
		//}
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
		IPCommManager::GetSingleton()->Write(test, test->_scriptArgs.c_str(), 0, test->_scriptArgs.size());
#endif
	}
	_currentTests++;
	test->_exitreason = Test::ExitReason::Running;
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
		ptr->_olderinputs = SessionStatistics::TestsExecuted(_sessiondata);
		// give _input access to test information
		ptr->test = test;
		// inform generation that an inut has finished if applicable
		if (!ptr->HasFlag(Input::Flags::GeneratedDeltaDebugging))
			_sessiondata->GetCurrentGeneration()->SetInputCompleted();

		if ((int64_t)ptr->GetSequenceLength() > ptr->derive->_sequenceNodes)
			logcritical("Input is longer than dev tree large");
	} else {
		logwarn("ptr invalid");
	}
	if (!test->WaitAndKillProcess()) {
#if defined(unix) || defined(__unix__) || defined(__unix)
		std::unique_lock<std::mutex> guards(_waitQueueLock);
		_waitQueue.push_back(test->processid);
		//logmessage("Waiting Process, Queue: {}", _waitQueue.size());
		_waitCond.notify_one();
#endif
	}
	// invalidate so no more functions can be called on the test
	test->InValidate();
	// call _callback if test has finished
	_threadpool->AddTask(test->_callback);
	_currentTests--;
}

void ExecutionHandler::TestStarter(std::shared_ptr<stop_token> stoken)
{
	std::chrono::nanoseconds waittime = std::chrono::microseconds(100);
	if (!_settings->fixes.disableExecHandlerSleep)
		waittime = _waittime;
	std::shared_ptr<Test> test;
	std::shared_ptr<Test> testweak;
	int32_t newtests = 0;
	while (_stopHandler == false || _finishtests || stoken->stop_requested() == false) {
		std::unique_lock<std::mutex> guard(_startingLock);
		newtests = 0;
		if (_freeze)
		{
			_frozenStarter = true;
			if (!(_freeze_waitfortestcompletion == true && _waitingTestsExec.size() > 0 || _waitingTests.size() > 0)) {
				_freezecond.wait_for(guard, std::chrono::milliseconds(100), [this] { return _stopHandler || !_freeze; });
				if (_freeze)
					continue;
			}
		}
		else
		{
			if (_frozenStarter)
				_frozenStarter = false;
		}
		
		// while we are not at the max concurrent tests, there are tests waiting to be executed and we are not FROZEN
		if (_currentTests + newtests < _maxConcurrentTests && (_waitingTestsExec.size() > 0 || _waitingTests.size() > 0) && (!_frozen || _frozen && _freeze_waitfortestcompletion)) {
			std::unique_lock<std::mutex> guards(_lockqueue);
			while (_currentTests + newtests < _maxConcurrentTests && (_waitingTestsExec.size() > 0 || _waitingTests.size() > 0)) {
				testweak.reset();
				{
					if (_waitingTestsExec.size() > 0) {
						testweak = _waitingTestsExec.front();
						_waitingTestsExec.pop_front();
					} else {
						// if there aren't any tests ready for execution grab some that aren't and make them ready
						InitTestsLockFree();
						if (_waitingTestsExec.size() > 0) {
							testweak = _waitingTestsExec.front();
							_waitingTestsExec.pop_front();
						}
					}
				}
				_startingTests.push(testweak);
				newtests++;
			}
		}
		if (newtests > 0)
			_threadpool->AddTask(Functions::ExecInitTestsCallback::CreateFull(_sessiondata));
		while (_startingTests.size() > 0)
		{
			test = _startingTests.front();
				_startingTests.pop();
			if (test) {
				if (StartTest(test)) {
					Utility::SpinLock guardm(_runningTestsFlag);
					_runningTests.push_back(test);
					logdebug("Added test to running queue");
					_waitforjob.notify_all();
				}
			}
		}

		_startingLockCond.wait_for(guard, waittime, [this] { return _stopHandler || (!_waitingTests.empty() || !_waitingTestsExec.empty()) && _currentTests < _maxConcurrentTests; });
	}
}

void ExecutionHandler::TestWaiter(std::shared_ptr<stop_token> stoken)
{
	logmessage("Starting Test Waiter");
	if (stoken->stop_requested())
		return;
#if defined(unix) || defined(__unix__) || defined(__unix)
	logmessage("Starting Test Waiter Unix");
	pid_t pid;
	std::deque<pid_t> internalQueue;
	while (_stopHandler == false || stoken->stop_requested() == false) {
		std::unique_lock<std::mutex> guard(_waitLock);
		_waitCond.wait_for(guard, std::chrono::milliseconds(100), [this] { return _stopHandler || !_waitQueue.empty(); });
		bool fail = false;
		/*
		bool fail = false;
		{
			std::unique_lock<std::mutex> guards(_waitQueueLock);
			try {
				pid = _waitQueue.ts_front();
				fail = false;
			} catch (std::exception&) {
				fail = true;
			}
		}
		if (!fail) {
			logmessage("Waiting for process");
			Processes::WaitProcess(pid, true);
		}*/
		{
			std::unique_lock<std::mutex> guards(_waitQueueLock);
			while (!_waitQueue.empty()) {
				internalQueue.push_back(_waitQueue.front());
				_waitQueue.pop_front();
			}
		}
		/*
		if (!fail) {
			logmessage("Waiting for process");
			//Processes::WaitProcess(pid, true);
		}*/
		{
			//std::unique_lock<std::mutex> guards(_waitQueueLock);
			auto itr = internalQueue.begin();
			while (itr != internalQueue.end())
			{
				if (Processes::WaitProcess(*itr, false) == true) {
					itr = internalQueue.erase(itr);
				} else
					itr++;
			}
		}
	}
#endif
	logmessage("Ending Test Waiter");
}

void ExecutionHandler::InternalLoop(std::shared_ptr<stop_token> stoken)
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

	// if handler has been stale account for timeout times
	if (_stale) {
		// time handler has been stale
		std::chrono::nanoseconds stalediff = std::chrono::steady_clock::now() - _lastExec;
		auto itr = _runningTests.begin();
		while (itr != _runningTests.end()) {
			// account for total timeout
			(*itr)->_timeouttime += stalediff;
			// account for fragment timeout
			(*itr)->_lasttime += stalediff;
			itr++;
		}
		_stale = false;
	}

	_handleTests.resize(_maxConcurrentTests);
	int32_t tohandle = 0;

	auto settings = _settings;

	logdebug("Entering loop");
	int32_t newtests = 0;
	while (_stopHandler == false || _finishtests || stoken->stop_requested() == false) {
		StartProfiling;
		logdebug2("find new tests");
		_lastExec = std::chrono::steady_clock::now();
		_tstatus = ExecHandlerStatus::MainLoop;
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

		IPCommManager::GetSingleton()->PerformWrites();

		lasttime = time;
		time = std::chrono::steady_clock::now();  // record the enter time
		diff = std::chrono::duration_cast<std::chrono::nanoseconds>(time - lasttime);
		logdebug2("time diff: {}ns", diff.count());
		// when applying timeouts we must take the timedifference between cycles into account
		// if we are simply slower in computation

		// exit condition wait until tests have finished
		if (_currentTests == 0 && _stopHandler == true && _finishtests == true)
		{
			loginfo("Finished all tests -> Exiting Handler");
			_tstatus = ExecHandlerStatus::Exitted;
			goto ExitHandler;
		}

		// get tests to handle this round
		{
			tohandle = 0;
			Utility::SpinLock guard(_runningTestsFlag);
			auto itr = _runningTests.begin();
			while (itr != _runningTests.end() && tohandle < _maxConcurrentTests) {
				auto ptr = *itr;
				if (ptr->_running == false) {
					itr = _runningTests.erase(itr);
					continue;
				} else {
					_handleTests[tohandle] = ptr;
					tohandle++;
				}
				itr++;
			}
		}

		// check if we are running tests, if not wait until we are given one to run
		// if we found some above it was started and this should be above 0
		if (!settings->fixes.disableExecHandlerSleep && _currentTests == 0 && tohandle == 0) {
			logdebug2("no tests active -> wait for new tests");
			std::unique_lock<std::mutex> guard(_lockqueue);
			_tstatus = ExecHandlerStatus::Waiting;
			_waitforjob.wait_for(guard, std::chrono::milliseconds(200), [this] { return !_runningTests.empty() || !_waitingTests.empty() || !_waitingTestsExec.empty() || _stopHandler; });
			if (_stopHandler && _finishtests == false) {
				profileW(TimeProfiling, "Round");
				break;
			} else {
				profileW(TimeProfiling, "Round");
				continue;
			}
		}
		logdebug2("Handling running tests: {}", _currentTests.load());

		for (int32_t i = 0; i < tohandle; i++) {
			if (stoken->stop_requested()) {
				return;
			}
			_tstatus = ExecHandlerStatus::HandlingTests;
			auto ptr = _handleTests[i];
			logdebug2("Handling test {}", ptr->_identifier);
			if (ptr->PipeError())
			{
				ptr->KillProcess();
				ptr->_exitreason = Test::ExitReason::Pipe;
				SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Pipe);
				goto TestFinished;
			}
			// read _output accumulated in the mean-time
			// if process has ended there still may be something left over to read anyway
			ptr->_output += ptr->ReadOutput();
			logdebug2("Read Output {}", ptr->_identifier);
			// check for running
			if (ptr->IsRunning() == false) {
				// test has finished. Get exit code and check end conditions
				ptr->_exitreason = Test::ExitReason::Natural;
				SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Natural);
				goto TestFinished;
			}
			// check for memory consumption
			if (settings->tests.maxUsedMemory != 0) {
				// memory limitation enabled

				if (ptr->GetMemoryConsumption() > settings->tests.maxUsedMemory) {
					_tstatus = ExecHandlerStatus::KillingProcessMemory;
					ptr->KillProcess();
					// process killed, now set flags for oracle
					ptr->_exitreason = Test::ExitReason::Memory;
					SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Memory);
					goto TestFinished;
				}
			}
			// check for fragment completion
			if (_enableFragments && ptr->CheckInput()) {
				_tstatus = ExecHandlerStatus::WriteFragment;
				// fragment has been completed
				bool error = false; 
				// iff writenext and error flag are false the last fragment has been completed
				// otherwise it just couldn't be written
				if (ptr->WriteNext(error) == false && error == false && settings->tests.use_fragmenttimeout && std::chrono::duration_cast<std::chrono::microseconds>(time - ptr->_lasttime).count() > settings->tests.fragmenttimeout) {
					ptr->_exitreason = Test::ExitReason::Natural | Test::ExitReason::LastInput;
					SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Natural);
					SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::LastInput);
					goto TestFinished;
				}
			}
			if (stoken->stop_requested()) {
				return;
			}
			logdebug2("Checked Input {}", ptr->_identifier);
			// compute timeouts
			if (_enableFragments && settings->tests.use_fragmenttimeout) {
				difffrag = std::chrono::duration_cast<std::chrono::microseconds>(time - ptr->_lasttime);
				if (difffrag.count() > settings->tests.fragmenttimeout) {
					_tstatus = ExecHandlerStatus::KillingProcessTimeout;
					ptr->KillProcess();
					ptr->_exitreason = Test::ExitReason::FragmentTimeout;
					SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::FragmentTimeout);
					goto TestFinished;
				}
			}
			if (settings->tests.use_testtimeout) {
				difffrag = std::chrono::duration_cast<std::chrono::microseconds>(time - ptr->_starttime);
				if (difffrag.count() > settings->tests.testtimeout) {
					_tstatus = ExecHandlerStatus::KillingProcessTimeout;
					ptr->KillProcess();
					ptr->_exitreason = Test::ExitReason::Timeout;
					SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Timeout);
					goto TestFinished;
				}
			}
			goto TestRunning;
TestFinished:
			if (stoken->stop_requested())
				return;
			{
				_tstatus = ExecHandlerStatus::StoppingTest;
				ptr->_running = false;
				logdebug2("Test {} has ended", ptr->_identifier);
				// if not frozen, or if it is frozen but we are waiting for test completion, stop test
				if (!_frozen || _frozen && _freeze_waitfortestcompletion)
					StopTest(ptr);
				// otherwise save test to stop it later
				else {
					std::unique_lock<std::mutex> guard(_freezelock);
					_stoppingTests.push_back(ptr);
				}
				continue;
			}
TestRunning:;
		}

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
		profileW(TimeProfiling, "Round");
		if (sleep > 0ns && sleep < _waittime) {
			_tstatus = ExecHandlerStatus::Sleeping;
			std::this_thread::sleep_for(sleep);
		}
	}

	// we will only get here once we terminate the handler
//KillRunningTests:

	// kill all running tests
	for (auto ptr : _runningTests) {
			ptr->KillProcess();
	}
	_runningTests.clear();
	
ExitHandler:
	// mark thread as inactive
	{
		loginfo("Exiting thread");
		std::unique_lock<std::mutex> guard(_toplevelsync);
		if (!_active) {
			logcritical("ExecutionHandler is marked as finished, before actually finishing");
			return;
		} else {
			_active = false;
			_waitforhandler.notify_all();
			_startingLockCond.notify_all();
			loginfo("Finished Execution Handler");
		}
	}
}

void ExecutionHandler::Freeze(bool waitfortestcompletion)
{
	loginfo("Freezing execution...");
	_freeze_waitfortestcompletion = waitfortestcompletion;
	//if (_sessiondata->GetGenerationEnding())
	//	_freeze_waitfortestcompletion = true;
	_freeze = true;
	while ((_frozen == false || _freeze_waitfortestcompletion == true && WaitingTasks() > 0) && _active == true)
		;
	loginfo("Frozen execution.");
}

bool ExecutionHandler::IsFrozen()
{
	return _freeze;
}

void ExecutionHandler::Thaw()
{
	loginfo("Thawing execution...");
	_freeze = false;
	_freezecond.notify_all();
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

void ExecutionHandler::ClearTests()
{
	// get top level lock so we cannot get into conflict with top level functions when erasing the tests
	std::unique_lock<std::mutex> guardtop(_toplevelsync);
	// get lock on queue so we do not conflict with AddTests
	std::unique_lock<std::mutex> guardqueue(_lockqueue);

	std::unique_lock<std::mutex> guardstart(_startingLock);
	_waitingTests.clear();
	while (!_waitingTestsExec.empty()) {
		std::shared_ptr<Test> test = _waitingTestsExec.front();
		_waitingTestsExec.pop_front();
		test->InValidatePreExec();
	}
	{
		Utility::SpinLock guard(_runningTestsFlag);
		auto itr = _runningTests.begin();
		while (itr != _runningTests.end()) {
			if ( *itr && (*itr)->IsRunning()) {
				(*itr)->KillProcess();
				StopTest(*itr);
			}
			if (!_runningTests.empty())
				_runningTests.pop_front();
			itr = _runningTests.begin();
		}
	}
	while (!_startingTests.empty()) {
		std::shared_ptr<Test> test = _startingTests.front();
		_startingTests.pop();
		test->InValidatePreExec();

	}
	_currentTests = 0;
}

size_t ExecutionHandler::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4                       // version
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
	return Form::GetDynamicSize()  // form stuff
		+ GetStaticSize(classversion) + 8 /*len of ids*/ + _stoppingTests.size() * 8 + _runningTests.size() * 8 + _waitingTests.size() * 8 + _waitingTestsExec.size() * 8;
}

bool ExecutionHandler::WriteData(std::ostream* buffer, size_t& offset, size_t length)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset, length);
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
		if (test->HasFlag(Form::FormFlags::Deleted) == false)
			Buffer::Write(test->GetFormID(), buffer, offset);
		else
			Buffer::Write((uint64_t)0, buffer, offset);
	}
	for (auto ptr : _stoppingTests) {
		Buffer::Write(ptr->GetFormID(), buffer, offset);
	}
	for (auto test : _waitingTests) {
		if (test->HasFlag(Form::FormFlags::Deleted) == false)
			Buffer::Write(test->GetFormID(), buffer, offset);
		else
			Buffer::Write((uint64_t)0, buffer, offset);
	}
	for (auto test : _waitingTestsExec) {
		if (test->HasFlag(Form::FormFlags::Deleted) == false)
			Buffer::Write(test->GetFormID(), buffer, offset);
		else
			Buffer::Write((uint64_t)0, buffer, offset);
	}
	return true;
}

bool ExecutionHandler::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	SetChanged();
	ClearTests();
	if (_loadData)
		delete _loadData;
	_loadData = new LoadData();
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
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
			for (int32_t i = 0; i < (int32_t)len; i++)
				_loadData->ids.push_back(Buffer::ReadUInt64(buffer, offset));
			// this will always have changed so we can just set to changed from the get go
			return true;
		}
		break;
	default:
		return false;
	}
}

void ExecutionHandler::InitializeEarly(LoadResolver* /*resolver*/)
{
}

void ExecutionHandler::InitializeLate(LoadResolver* resolver)
{
	if (_loadData) {
		resolver->current = "ExecutionHandler Late";
		bool stateerror = false;
		_sessiondata = resolver->_data->CreateForm<SessionData>();
		_session = resolver->_data->CreateForm<Session>();
		_settings = resolver->_data->CreateForm<Settings>();
		_threadpool = resolver->_data->CreateForm<TaskController>();
		_oracle = resolver->_data->CreateForm<Oracle>();
		for (int32_t i = 0; i < (int32_t)_loadData->ids.size(); i++) {
			if (!CmdArgs::_clearTasks) {
				if (_loadData->ids[i] != 0) {
					auto test = resolver->ResolveFormID<Test>(_loadData->ids[i]);
					if (test->HasFlag(Form::FormFlags::DoNotFree) == false)
						test->SetFlag(Form::FormFlags::DoNotFree);
					test->Init(test->_callback, test->_identifier);
					// since we found a test we still need to execute, we need to make sure that our _input contains a valid _input sequence
					// and regenerate it if not
					if (auto ptr = test->_input.lock(); ptr) {
						resolver->AddRegeneration(ptr->GetFormID());
						/*if (!ptr->test)
										ptr->test = test;
									if (ptr->GetGenerated() == false)
										SessionFunctions::GenerateInput(ptr, this->_sessiondata);
									std::shared_ptr<Input> tmp = resolver->ResolveFormID<Input>(ptr->GetParentID());
									while (tmp) {
										tmp->FreeMemory();
										if (tmp->derive)
											tmp->derive->FreeMemory();
										tmp = resolver->ResolveFormID<Input>(tmp->GetParentID());
									}*/
					}
					test->_cmdArgs = Lua::GetCmdArgs(std::bind(&Oracle::GetCmdArgs, _oracle, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), test, stateerror, false);
					if (_oracle->GetOracletype() == Oracle::PUTType::Script)
						test->_scriptArgs = Lua::GetScriptArgs(std::bind(&Oracle::GetScriptArgs, _oracle, std::placeholders::_1, std::placeholders::_2), test, stateerror);
					_waitingTests.push_back(test);
				} else
					logcritical("ExecutionHandler::Load cannot resolve test");
			}
		}
		// make the first batch of tests ready for execution
		InitTests();
		if (_active) {
			_active = false;
		}

		delete _loadData;
		_loadData = nullptr;
	}
}


void ExecutionHandler::Delete(Data*)
{
	StopHandler();
	Clear();
}

void ExecutionHandler::ClearChanged()
{
	Form::ClearChanged();
	SetChanged();
}

void ExecutionHandler::SetPeriod(std::chrono::nanoseconds period)
{
	_waittime = period;
	_waittimeL = period.count();
}

ExecHandlerStatus ExecutionHandler::GetThreadStatus()
{
	return _tstatus;
}



namespace Functions
{
	void ExecInitTestsCallback::Run()
	{
		_sessiondata->_exechandler->InitTests();
	}

	std::shared_ptr<BaseFunction> ExecInitTestsCallback::DeepCopy()
	{
		auto ptr = std::make_shared<ExecInitTestsCallback>();
		ptr->_sessiondata = _sessiondata;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	bool ExecInitTestsCallback::ReadData(std::istream*, size_t&, size_t, LoadResolver* resolver)
	{
		// get id of saved controller and resolve link
		if (CmdArgs::_clearTasks == false) {
			resolver->AddTask([this, resolver]() {
				this->_sessiondata = resolver->_data->CreateForm<SessionData>();
			});
		}
		return true;
	}

	bool ExecInitTestsCallback::ReadData(unsigned char*, size_t&, size_t, LoadResolver* resolver)
	{
		// get id of saved controller and resolve link
		if (CmdArgs::_clearTasks == false) {
			resolver->AddTask([this, resolver]() {
				this->_sessiondata = resolver->_data->CreateForm<SessionData>();
			});
		}
		return true;
	}

	bool ExecInitTestsCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		return true;
	}

	unsigned char* ExecInitTestsCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		size = GetLength();
		return buffer;
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

	void WriteTestInputCallback::Run()
	{
		if (_test && data != nullptr)
		{
			size_t offset = 0;
			size_t written = 1;
			while (written != 0 && length > 0) {
				written = _test->Write(data, offset, length);
				// update write information
				offset += written;
				length -= written;
				if (written == -1) {
					break;
				} else if (length <= 0) {
					break;
				}
			}
			if (data != nullptr) {
				delete[] data;
				data = nullptr;
			}
		}
		else if (data != nullptr)
		{
			delete[] data;
			data = nullptr;
		}
	}

	std::shared_ptr<BaseFunction> WriteTestInputCallback::DeepCopy()
	{
		// not copyable
		auto ptr = std::make_shared<WriteTestInputCallback>();
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	bool WriteTestInputCallback::ReadData(std::istream*, size_t&, size_t, LoadResolver*)
	{
		_test = {};
		return true;
	}

	bool WriteTestInputCallback::ReadData(unsigned char*, size_t&, size_t, LoadResolver*)
	{
		_test = {};
		return true;
	}

	bool WriteTestInputCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		return true;
	}

	unsigned char* WriteTestInputCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		size = GetLength();
		return buffer;
	}

	size_t WriteTestInputCallback::GetLength()
	{
		return BaseFunction::GetLength();
	}

	void WriteTestInputCallback::Dispose()
	{
		_test.reset();
		if (data != nullptr)
			delete[] data;
		data = nullptr;
	}
}
