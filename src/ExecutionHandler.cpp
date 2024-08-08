#include "ExecutionHandler.h"
#include "Input.h"
#include "Oracle.h"
#include "Settings.h"
#include "Processes.h"
#include "Logging.h"
#include "Session.h"
#include "Data.h"
//#include <io.h>

ExecutionHandler::ExecutionHandler()
{
}

void ExecutionHandler::Init(std::shared_ptr<Session> session, std::shared_ptr<Settings> settings, std::shared_ptr<TaskController> threadpool, int32_t maxConcurrentTests, std::shared_ptr<Oracle> oracle, std::function<std::string(std::shared_ptr<Input>)>&& getCommandLineArgs)
{
	loginfo("Init execution handler");
	_maxConcurrentTests = maxConcurrentTests > 0 ? maxConcurrentTests : 1;
	_threadpool = threadpool;
	_settings = settings;
	_session = session;
	_oracle = oracle;
	getCMDArgs = std::move(getCommandLineArgs);
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
	cleared = true;
	_session.reset();
	_threadpool.reset();
	_oracle.reset();
	while (!_waitingTests.empty())
	{
		std::weak_ptr<Test> test = _waitingTests.front();
		_waitingTests.pop();
		if (auto ptr = test.lock(); ptr)
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
	_currentTests = 0;
	_active = false;
	_nextid = 1;
	_enableFragments = false;
	_stopHandler = false;
	_finishtests = false;
	_thread = {};
	getCMDArgs = {};
}

void ExecutionHandler::SetMaxConcurrentTests(int32_t maxConcurrenttests)
{
	loginfo("Set max concurrent tests to {}", maxConcurrenttests);
	_maxConcurrentTests = maxConcurrenttests > 0 ? maxConcurrenttests : 1;
}

void ExecutionHandler::StartHandler()
{
	if (cleared) {
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
			_waitingTests.pop();
			if (auto test = weak.lock(); test) {
				if (auto ptr = test->input.lock(); ptr) {
					ptr->hasfinished = true;
					ptr->test = test;
				}
				test->exitreason = Test::ExitReason::InitError;
				test->InValidate();
				test->input.reset();
				// call callback if test has finished
				_threadpool->AddTask(test->callback);
			}
		}
		// delete all running tests
		for (std::weak_ptr<Test> weak : _runningTests) {
			if (auto test = weak.lock(); test) {
				test->KillProcess();
				if (auto ptr = test->input.lock(); ptr) {
					ptr->hasfinished = true;
					ptr->test = test;
				}
				test->exitreason = Test::ExitReason::InitError;
				test->InValidate();
				test->input.reset();
				// call callback if test has finished
				_threadpool->AddTask(test->callback);
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

bool ExecutionHandler::AddTest(std::shared_ptr<Input> input, std::function<void()> callback)
{
	StartProfilingDebug;
	loginfo("Adding new test");
	uint64_t id = 0;
	{
		std::unique_lock<std::mutex> guard(_lockqueue);
		id = _nextid++;
	}
	std::shared_ptr<Test> test = _session->data->CreateForm<Test>();
	test->Init(std::move(callback), id);
	if (test->exitreason & Test::ExitReason::InitError) {
		_session->data->DeleteForm(test);
		return false;
	}
	test->running = false;
	test->input = input;
	test->itr = test->input.lock()->begin();
	test->itrend = test->input.lock()->end();
	test->reactiontime = {};
	test->output = "";
	{
		std::unique_lock<std::mutex> guard(_lockqueue);
		_waitingTests.push(test);
	}
	_waitforjob.notify_one();
	profileDebug(TimeProfilingDebug, "");
	return true;
}

bool ExecutionHandler::StartTest(std::shared_ptr<Test> test)
{
	StartProfilingDebug;
	logdebug("start test {}", uintptr_t(test.get()));
	// start test
	// if successful: add test to list of active tests and update _currentTests
	// give test its first input on startup
	// if unsuccessful: report error and complete test-case as failed
	test->starttime = std::chrono::steady_clock::now();
	if (!Processes::StartPUTProcess(test, _oracle->path().string(), getCMDArgs(test->input.lock())))
	{
		test->exitreason = Test::ExitReason::InitError;
		if (auto ptr = test->input.lock(); ptr) {
			ptr->hasfinished = true;
			// invalidate so no more functions can be called on the test
			test->InValidate();
			// give input access to test information
			ptr->test = test;
		}
		test->input.reset();
		// call callback if test has finished
		_threadpool->AddTask(test->callback);
		logdebug("test cannot be started");
		profileDebug(TimeProfilingDebug, "");
		return false;
	}
	logdebug("started process");
	if (_enableFragments)
		test->WriteNext();
	else
		test->WriteAll();
	_currentTests++;
	_runningTests.push_back(test);
	logdebug("test started");
	profileDebug(TimeProfilingDebug, "");
	return true;
}

void ExecutionHandler::StopTest(std::shared_ptr<Test> test)
{
	// clean up input length (cut the sequence to last executed
	if (_enableFragments) {
		test->TrimInput();
	}
	if (auto ptr = test->input.lock(); ptr) {
		// write back test results to input
		ptr->hasfinished = true;
		ptr->executiontime = std::chrono::duration_cast<std::chrono::nanoseconds>(test->endtime - test->starttime);
		ptr->exitcode = test->GetExitCode();
		// give input access to test information
		ptr->test = test;
	}
	// reset pointer to input, otherwise we have a loop
	test->input.reset();
	// invalidate so no more functions can be called on the test
	test->InValidate();
	// evaluate oracle
	auto result = _oracle->Evaluate(test);
	// call callback if test has finished
	_threadpool->AddTask(test->callback);
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
	while (_stopHandler == false || _finishtests) {
		StartProfiling;
		logdebug("find new tests");
		while (_currentTests < _maxConcurrentTests && _waitingTests.size() > 0) {
			test.reset();
			{
				std::unique_lock<std::mutex> guard(_lockqueue);
				if (_waitingTests.size() > 0) {
					test = _waitingTests.front();
					_waitingTests.pop();
				}
			}
			if (auto ptr = test.lock(); ptr) {
				StartTest(ptr);
			}
		}

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
			_waitforjob.wait_for(guard, std::chrono::seconds(1), [this] { return !_waitingTests.empty() || _stopHandler; });
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
				logdebug("Handling test {}", ptr->identifier);
				// read output accumulated in the mean-time
				// if process has ended there still may be something left over to read anyway
				ptr->output += ptr->ReadOutput();
				logdebug("Read Output {}", ptr->identifier);
				// check for running
				if (ptr->IsRunning() == false) {
					// test has finished. Get exit code and check end conditions
					goto TestFinished;
				}
				// check for memory consumption
				if (_settings->tests.maxUsedMemory != 0) {
					// memory limitation enabled

					if (ptr->GetMemoryConsumption() > _settings->tests.maxUsedMemory) {
						ptr->KillProcess();
						// process killed, now set flags for oracle
						ptr->exitreason = Test::ExitReason::Memory;
						goto TestFinished;
					}
				}
				// check for fragment completion
				if (_enableFragments && ptr->CheckInput()) {
					// fragment has been completed
					if (ptr->WriteNext() == false && _settings->tests.use_fragmenttimeout && std::chrono::duration_cast<std::chrono::microseconds>(time - ptr->lasttime).count() > _settings->tests.fragmenttimeout) {
						ptr->exitreason = Test::ExitReason::Natural | Test::ExitReason::LastInput;
						goto TestFinished;
					}
				}
				logdebug("Checked Input {}", ptr->identifier);
				// compute timeouts
				if (_enableFragments && _settings->tests.use_fragmenttimeout) {
					difffrag = std::chrono::duration_cast<std::chrono::microseconds>(time - ptr->lasttime);
					if (difffrag.count() > _settings->tests.fragmenttimeout) {
						ptr->KillProcess();
						ptr->exitreason = Test::ExitReason::FragmentTimeout;
						goto TestFinished;
					}
				}
				if (_settings->tests.use_testtimeout) {
					difffrag = std::chrono::duration_cast<std::chrono::microseconds>(time - ptr->starttime);
					if (difffrag.count() > _settings->tests.testtimeout) {
						ptr->KillProcess();
						ptr->exitreason = Test::ExitReason::Timeout;
						goto TestFinished;
					}
				}
				goto TestRunning;
TestFinished:
				{
					logdebug("Test {} has ended", ptr->identifier);
					StopTest(ptr);
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
