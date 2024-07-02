#include "ExecutionHandler.h"
#include "Input.h"
#include "Oracle.h"
#include "Settings.h"
#include "Processes.h"
#include "Logging.h"
//#include <io.h>


#if defined(unix) || defined(__unix__) || defined(__unix)
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include <Windows.h>
#include <Psapi.h>
#endif

#define PIPE_SIZE 65536

Test::Test(std::function<void()>&& a_callback, uint64_t id) :
	callback(std::move(a_callback))
{
	loginfo("Init test");
	identifier = id;

#if defined(unix) || defined(__unix__) || defined(__unix)
	if (pipe2(red_output, O_NONBLOCK == -1)) {
		exitreason = ExitReason::InitError;
		return;
	}
	if (pipe2(red_input, O_NONBLOCK == -1)) {
		exitreason = ExitReason::InitError;
		return;
	}
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::string pipe_name_input = "\\\\.\\pipe\\" + std::to_string(intptr_t(this)) + "inp";
	std::string pipe_name_output = "\\\\.\\pipe\\" + std::to_string(intptr_t(this)) + "out";
	// create pipes for output

	// set security
	SECURITY_ATTRIBUTES saAttr;
	ZeroMemory(&saAttr, sizeof(saAttr));
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = FALSE;
	saAttr.lpSecurityDescriptor = NULL;
	// create pipes
	red_output[0] = CreateNamedPipeA(
		pipe_name_output.c_str(),
		PIPE_ACCESS_INBOUND,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
		1,
		PIPE_SIZE,
		PIPE_SIZE,
		0,
		&saAttr);
	if (red_output[0] == INVALID_HANDLE_VALUE) {
		logcritical("failed to create named pipe {}", GetLastError());
		exitreason = ExitReason::InitError;
		return;
	}
	ZeroMemory(&saAttr, sizeof(saAttr));
	saAttr.nLength = sizeof(saAttr);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;
	red_output[1] = CreateFileA(pipe_name_output.c_str(), GENERIC_WRITE, 0, &saAttr, OPEN_EXISTING, 0, NULL);
	if (red_output[1] == INVALID_HANDLE_VALUE) {
		logcritical("failed to create event handle {}", GetLastError());
		exitreason = ExitReason::InitError;
		return;
	}

	BOOL success = ConnectNamedPipe(red_output[0], NULL);
	if (!success && GetLastError() != ERROR_PIPE_CONNECTED) {
		logcritical("cannot connect to named pipe {}", GetLastError());
		exitreason = ExitReason::InitError;
		return;
	}

	// create pipes for input
	// set security
	ZeroMemory(&saAttr, sizeof(saAttr));
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;
	// create pipes
	red_input[0] = CreateNamedPipeA(
		pipe_name_input.c_str(),
		PIPE_ACCESS_INBOUND,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
		1,
		PIPE_SIZE,
		PIPE_SIZE,
		0,
		&saAttr);
	if (red_input[0] == INVALID_HANDLE_VALUE) {
		logcritical("failed to create named pipe {}", GetLastError());
		exitreason = ExitReason::InitError;
		return;
	}
	ZeroMemory(&saAttr, sizeof(saAttr));
	saAttr.nLength = sizeof(saAttr);
	saAttr.bInheritHandle = FALSE;
	saAttr.lpSecurityDescriptor = NULL;
	red_input[1] = CreateFileA(pipe_name_input.c_str(), GENERIC_WRITE, 0, &saAttr, OPEN_EXISTING, 0, NULL);
	if (red_input[1] == INVALID_HANDLE_VALUE) {
		logcritical("failed to create event handle {}", GetLastError());
		exitreason = ExitReason::InitError;
		return;
	}

	success = ConnectNamedPipe(red_input[0], NULL);
	if (!success && GetLastError() != ERROR_PIPE_CONNECTED) {
		logcritical("cannot connect to named pipe {}", GetLastError());
		exitreason = ExitReason::InitError;
		return;
	}

	/*
	if (!CreatePipe(&red_output[0], &red_output[1], &saAttr, PIPE_SIZE)) {
		exitreason = ExitReason::InitError;
		return;
	}
	// set readhandle for output to not be inherited
	SetHandleInformation(&red_output[0], HANDLE_FLAG_INHERIT, 0);
	if (!CreatePipe(&red_input[0], &red_input[1], &saAttr, PIPE_SIZE)) {
		exitreason = ExitReason::InitError;
		return;
	}
	// set writehandle for input to not be inherited
	SetHandleInformation(&red_input[1], HANDLE_FLAG_INHERIT, 0);*/
#endif
}

bool Test::IsRunning()
{
	if (!valid) {
		logcritical("called IsRunning after invalidation");
		return false;
	}
#if defined(unix) || defined(__unix__) || defined(__unix)
	bool running = Processes::GetProcessRunning(processid);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	bool running = Processes::GetProcessRunning(pi.hProcess);
#endif
	if (running)
		return true;
	else {
		endtime = std::chrono::steady_clock::now();
		exitreason = ExitReason::Natural;
		return false;
	}
}

void Test::WriteInput(std::string str)
{
	if (!valid) {
		logcritical("called WriteInput after invalidation");
		return;
	}
	logdebug("Writing input: \"{}\"", str);
#if defined(unix) || defined(__unix__) || defined(__unix)
	throw std::runtime_error("not implemented");
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	DWORD dwWritten;
	BOOL bSuccess = FALSE;
	const char* cstr = str.c_str();
	bSuccess = WriteFile(red_input[1], cstr, strlen(cstr), &dwWritten, NULL);
#endif
}

bool Test::WriteNext()
{
	if (!valid) {
		logcritical("called WriteNext after invalidation");
		return false;
	}
	if (itr == input->end())
		return false;
	WriteInput(*itr);
	lasttime = std::chrono::steady_clock::now();
	lastwritten = *itr;
	itr++;
	return true;
}

bool Test::WriteAll()
{
	if (!valid) {
		logcritical("called WriteAll after invalidation");
		return false;
	}
	auto itr = input->begin();
	while (itr != input->end())
	{
		WriteInput(*itr);
		lastwritten += *itr;
		itr++;
	}
	lasttime = std::chrono::steady_clock::now();
	return true;
}

bool Test::CheckInput()
{
	if (!valid) {
		logcritical("called CheckInput after invalidation");
		return false;
	}
	bool ret = false;
#if defined(unix) || defined(__unix__) || defined(__unix)
	struct pollfd fds;
	int events = 0;
	fds.fd = 0; // stdin
	fds.events = POLLIN;
	events = poll(&fds, 1, 0);
	if ((events & 0x1) > 0)
		ret = false;
	else
		ret = true;
	logdebug("CheckInput : {}", ret);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	//DWORD bytesAvail = 0;
	//BOOL bSuccess = PeekNamedPipe(red_input[0], NULL, 0, NULL, &bytesAvail, NULL);
	//if (bytesAvail > 0)
	//	ret = false;
	//else
	//	ret = true;
	DWORD dwRead;
	int sl = strlen(lastwritten.c_str());
	char* chBuf = new char[sl + 1];
	BOOL bSuccess = FALSE;
	bSuccess = ReadFile(red_input[0], chBuf, sl, &dwRead, NULL);
	if (dwRead > 0) {
		bSuccess = WriteFile(red_input[1], chBuf, dwRead, &dwRead, NULL);
		ret = false;
	}
	else
		ret = true;
	logdebug("CheckInput : {}, succ {}, err {}", ret, bSuccess, GetLastError());
#endif
	return ret;
}

std::string Test::ReadOutput()
{
	if (!valid) {
		logcritical("called ReadOutput after invalidation");
		return "";
	}
	std::string ret;
#if defined(unix) || defined(__unix__) || defined(__unix)
	throw std::runtime_error("not implemented");
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	DWORD dwRead;
	CHAR chBuf[512];
	BOOL bSuccess = FALSE;
	for (;;) {
		bSuccess = ReadFile(red_input[0], chBuf, 512, &dwRead, NULL);
		if (!bSuccess || dwRead == 0)
			break;
		else {
			// convert read to string
			std::string str(chBuf, dwRead);
			ret += str;
		}
	}
	return ret;
#endif
}

long Test::GetMemoryConsumption()
{
	if (!valid) {
		logcritical("called GetMemoryConsumption after invalidation");
		return -1;
	}
#if defined(unix) || defined(__unix__) || defined(__unix)
	return Processes::GetProcessMemory(processid);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	return Processes::GetProcessMemory(pi.hProcess);
#endif
}

bool Test::KillProcess()
{
	if (!valid) {
		logcritical("called KillProcess after invalidation");
		return false;
	}
	logdebug("Test {}: Killed Test", identifier);
	endtime = std::chrono::steady_clock::now();
#if defined(unix) || defined(__unix__) || defined(__unix)
	return Processes::KillProcess(processid);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	return Processes::KillProcess(pi.hProcess);
#endif
}

void Test::TrimInput()
{
	if (!valid) {
		logcritical("called TrimInput after invalidation");
		return;
	}
	logdebug("Test {}: trimmed input", identifier);
	input->trimmed = true;
	auto aitr = input->begin();
	while (aitr != itr) {
		input->orig_sequence.push_back(*aitr);
		aitr++;
	}
	std::swap(input->sequence, input->orig_sequence);
}

int Test::GetExitCode()
{
	if (!valid) {
		logcritical("called GetExitCode after invalidation");
		return -1;
	}
#if defined(unix) || defined(__unix__) || defined(__unix)
	return Processes::GetExitCode(processid);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	return Processes::GetExitCode(pi.hProcess);
#endif
}

void Test::InValidate()
{
	valid = false;
#if defined(unix) || defined(__unix__) || defined(__unix)
	processid = 0;
	close(red_input[0]);
	red_input[0] = -1;
	close(red_input[1]);
	red_input[1] = -1;
	close(red_output[0]);
	red_output[0] = -1;
	red_output[1] = -1;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	CloseHandle(red_input[0]);
	red_input[0] = nullptr;
	CloseHandle(red_input[1]);
	red_input[1] = nullptr;
	CloseHandle(red_output[0]);
	red_output[0] = nullptr;
	red_output[1] = nullptr;
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	pi.hProcess = nullptr;
	pi.hThread = nullptr;
	pi.dwProcessId = 0;
	pi.dwThreadId = 0;
	pi = {};
	si = {};
#endif
	itr = {};
}

ExecutionHandler::ExecutionHandler(Settings* settings, std::shared_ptr<TaskController> threadpool, int maxConcurrentTests, std::shared_ptr<Oracle> oracle, std::function<std::string(std::shared_ptr<Input>)>&& getCommandLineArgs) :
	getCMDArgs(std::move(getCommandLineArgs))
{
	loginfo("Init execution handler");
	_maxConcurrentTests = maxConcurrentTests > 0 ? maxConcurrentTests : 1;
	_threadpool = threadpool;
	_settings = settings;
	_oracle = oracle;
}

void ExecutionHandler::SetMaxConcurrentTests(int maxConcurrenttests)
{
	loginfo("Set max concurrent tests to {}", maxConcurrenttests);
	_maxConcurrentTests = maxConcurrenttests > 0 ? maxConcurrenttests : 1;
}

void ExecutionHandler::StartHandler()
{
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
			Test* test = _waitingTests.front();
			_waitingTests.pop();
			test->input->hasfinished = true;
			test->exitreason = Test::ExitReason::InitError;
			test->InValidate();
			test->input->test = test;
			test->input.reset();
			// call callback if test has finished
			_threadpool->AddTask(test->callback);
		}
		// delete all running tests
		for (Test* test : _runningTests) {
			test->KillProcess();
			test->input->hasfinished = true;
			test->exitreason = Test::ExitReason::InitError;
			test->InValidate();
			test->input->test = test;
			test->input.reset();
			// call callback if test has finished
			_threadpool->AddTask(test->callback);
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
	loginfo("Adding new test");
	uint64_t id = 0;
	{
		std::unique_lock<std::mutex> guard(_lockqueue);
		id = _nextid++;
	}
	Test* test = new Test(std::move(callback), id);
	if (test->exitreason == Test::ExitReason::InitError)
		return false;
	test->running = false;
	test->input = input;
	test->itr = test->input->begin();
	test->reactiontime = {};
	test->output = "";
	{
		std::unique_lock<std::mutex> guard(_lockqueue);
		_waitingTests.push(test);
	}
	_waitforjob.notify_one();
	return true;
}

bool ExecutionHandler::StartTest(Test* test)
{
	logdebug("start test {}", uintptr_t(test));
	// start test
	// if successful: add test to list of active tests and update _currentTests
	// give test its first input on startup
	// if unsuccessful: report error and complete test-case as failed
	test->starttime = std::chrono::steady_clock::now();
	if (!Processes::StartPUTProcess(test, _oracle->path().string(), getCMDArgs(test->input)))
	{
		test->exitreason = Test::ExitReason::InitError;
		test->input->hasfinished = true;
		// invalidate so no more functions can be called on the test
		test->InValidate();
		// give input access to test information
		test->input->test = test;
		test->input.reset();
		// call callback if test has finished
		_threadpool->AddTask(test->callback);
		logdebug("test cannot be started");
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
	return true;
}

void ExecutionHandler::StopTest(Test* test)
{
	// clean up input length (cut the sequence to last executed
	if (_enableFragments) {
		test->TrimInput();
	}
	// write back test results to input
	test->input->hasfinished = true;
	test->input->executiontime = std::chrono::duration_cast<std::chrono::nanoseconds>(test->endtime - test->starttime);
	test->input->exitcode = test->GetExitCode();
	// invalidate so no more functions can be called on the test
	test->InValidate();
	// give input access to test information
	test->input->test = test;
	// reset pointer to input, otherwise we have a loop
	test->input.reset();
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
	long nano = -1; 
	// used for sleep time calculation
	std::chrono::nanoseconds sleep = std::chrono::nanoseconds(-1);
	// tmp test var
	Test* test = nullptr;
	logdebug("Entering loop");
	while (_stopHandler == false || _finishtests) {
		logdebug("find new tests");
		while (_currentTests < _maxConcurrentTests && _waitingTests.size() > 0) {
			Test* test = nullptr;
			{
				std::unique_lock<std::mutex> guard(_lockqueue);
				if (_waitingTests.size() > 0) {
					test = _waitingTests.front();
					_waitingTests.pop();
				}
			}
			if (test != nullptr) {
				StartTest(test);
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
			if (_stopHandler && _finishtests == false)
				break;
			else
				continue;
		}
		logdebug("Handling running tests: {}", _currentTests);
		auto itr = _runningTests.begin();
		while (itr != _runningTests.end()) {
			Test* test = *itr;
			logdebug("Handling test {}", test->identifier);
			// read output accumulated in the mean-time
			// if process has ended there still may be something left over to read anyway
			test->output += test->ReadOutput();
			// check for running
			if (test->IsRunning() == false) {
				// test has finished. Get exit code and check end conditions
				goto TestFinished;
			}
			// check for memory consumption
			if (_settings->tests.maxUsedMemory != 0) {
				// memory limitation enabled

				if (test->GetMemoryConsumption() > _settings->tests.maxUsedMemory) {
					test->KillProcess();
					// process killed, now set flags for oracle
					test->exitreason = Test::ExitReason::Memory;
					goto TestFinished;
				}
			}
			// check for fragment completion
			if (_enableFragments && test->CheckInput()) {
				// fragment has been completed
				if (test->WriteNext() == false && _settings->tests.use_fragmenttimeout && std::chrono::duration_cast<std::chrono::microseconds>(time - test->lasttime).count() > _settings->tests.fragmenttimeout)
				{
					test->exitreason = Test::ExitReason::Natural | Test::ExitReason::LastInput;
					goto TestFinished;
				}
			}
			// compute timeouts
			if (_enableFragments && _settings->tests.use_fragmenttimeout) {
				difffrag = std::chrono::duration_cast<std::chrono::microseconds>(time - test->lasttime);
				if (difffrag.count() > _settings->tests.fragmenttimeout) {
					test->KillProcess();
					test->exitreason = Test::ExitReason::FragmentTimeout;
					goto TestFinished;
				}
			}
			if (_settings->tests.use_testtimeout) {
				difffrag = std::chrono::duration_cast<std::chrono::microseconds>(time - test->starttime);
				if (difffrag.count() > _settings->tests.testtimeout) {
					test->KillProcess();
					test->exitreason = Test::ExitReason::Timeout;
					goto TestFinished;
				}
			}
			goto TestRunning;
TestFinished:
			{
				logdebug("Test {} has ended", test->identifier);
				StopTest(test);
				// delete test from list
				itr = _runningTests.erase(itr);
				continue;
			}
TestRunning:
			itr++;
		}
		// reset vars
		test = nullptr;

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
		std::this_thread::sleep_for(sleep);
	}

	// we will only get here once we terminate the handler
KillRunningTests:

	// kill all running tests

	
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
