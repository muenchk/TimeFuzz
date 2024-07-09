#include "Test.h"
#include "Input.h"
#include "Processes.h"
#include "Logging.h"

#if defined(unix) || defined(__unix__) || defined(__unix)
#	include <fcntl.h>
#	include <poll.h>
#	include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include <Psapi.h>
#	include <Windows.h>
#endif

#define PIPE_SIZE 65536

Test::Test(std::function<void()>&& a_callback, uint64_t id) :
	callback(std::move(a_callback))
{
	StartProfiling;
	loginfo("Init test");
	identifier = id;

#if defined(unix) || defined(__unix__) || defined(__unix)
	if (pipe2(red_output, O_NONBLOCK == -1)) {
		exitreason = ExitReason::InitError;
		return;
	}
	//fcntl64(red_output[0], F_SETFL, O_NONBLOCK);
	//fcntl64(red_output[1], F_SETFL, O_NONBLOCK);
	if (pipe2(red_input, O_NONBLOCK == -1)) {
		exitreason = ExitReason::InitError;
		return;
	}
	//fcntl64(red_input[0], F_SETFL, O_NONBLOCK);
	//fcntl64(red_input[1], F_SETFL, O_NONBLOCK);
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
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,  // | PIPE_NOWAIT,
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
	profile(TimeProfiling, "");
}

bool Test::IsRunning()
{
	StartProfilingDebug;
	if (!valid) {
		logcritical("called IsRunning after invalidation");
		return false;
	}
#if defined(unix) || defined(__unix__) || defined(__unix)
	bool res = Processes::GetProcessRunning(processid, &exitcode);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	bool res = Processes::GetProcessRunning(pi.hProcess);
#endif
	profileDebug(TimeProfilingDebug, "");
	if (res)
		return true;
	else {
		endtime = std::chrono::steady_clock::now();
		exitreason = ExitReason::Natural;
		return false;
	}
}

void Test::WriteInput(std::string str)
{
	StartProfilingDebug;
	if (!valid) {
		logcritical("called WriteInput after invalidation");
		return;
	}
	logdebug("Writing input: \"{}\"", str);
#if defined(unix) || defined(__unix__) || defined(__unix)
	size_t len = strlen(str.c_str());
	if (size_t written = write(red_input[1], str.c_str(), len); written != len) {
		logcritical("Write to child is missing bytes: Supposed {}, written {}", len, written);
		return;
	}
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	DWORD dwWritten;
	BOOL bSuccess = FALSE;
	const char* cstr = str.c_str();
	bSuccess = WriteFile(red_input[1], cstr, (DWORD)strlen(cstr), &dwWritten, NULL);
#endif
	profileDebug(TimeProfilingDebug, "");
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
	auto itra = input->begin();
	while (itra != input->end()) {
		WriteInput(*itra);
		lastwritten += *itra;
		itra++;
	}
	lasttime = std::chrono::steady_clock::now();
	return true;
}

bool Test::CheckInput()
{
	StartProfilingDebug;
	if (!valid) {
		logcritical("called CheckInput after invalidation");
		return false;
	}
	bool ret = false;
#if defined(unix) || defined(__unix__) || defined(__unix)
	struct pollfd fds;
	int events = 0;
	fds.fd = red_input[0];  // stdin
	fds.events = POLLIN;
	events = poll(&fds, 1, 0);
	if ((fds.revents & POLLIN) == POLLIN) {
		ret = false;
	} else
		ret = true;
	logdebug("CheckInput : {}", ret);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	DWORD bytesAvail = 0;
	BOOL bSuccess = PeekNamedPipe(red_input[0], NULL, 0, NULL, &bytesAvail, NULL);
	if (bytesAvail > 0)
		ret = false;
	else
		ret = true;
		/*DWORD dwRead;
	int sl = (int)strlen(lastwritten.c_str());
	char* chBuf = new char[sl + 1];
	BOOL bSuccess = FALSE;
	bSuccess = ReadFile(red_input[0], chBuf, sl, &dwRead, NULL);
	if (dwRead > 0) {
		bSuccess = WriteFile(red_input[1], chBuf, dwRead, &dwRead, NULL);
		ret = false;
	}
	else
		ret = true;
	logdebug("CheckInput : {}, succ {}, err {}", ret, bSuccess, GetLastError());*/
#endif
	profileDebug(TimeProfilingDebug, "");
	return ret;
}

std::string Test::ReadOutput()
{
	StartProfilingDebug;
	if (!valid) {
		logcritical("called ReadOutput after invalidation");
		return "";
	}
	std::string ret;
#if defined(unix) || defined(__unix__) || defined(__unix)
	struct pollfd fds;
	int events = 0;
	fds.fd = red_output[0];  // stdin
	fds.events = POLLIN;
	events = poll(&fds, 1, 0);
	logdebug("0: {}", events);
	while ((fds.revents & POLLIN) == POLLIN) {  // input to read available
		logdebug("1");
		char buf[512];
		logdebug("2");
		logdebug("3: {}", red_output[0]);
		int _read = read(red_output[0], buf, 511);
		logdebug("4: {}", _read);
		if (_read <= 0) {
			logdebug("5");
			break;
		} else {
			logdebug("6");
			// convert to string
			std::string str(buf, _read);
			ret += str;
		}
		fds.revents = 0;
		events = poll(&fds, 1, 0);
	}
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	DWORD dwRead;
	CHAR chBuf[512];
	BOOL bSuccess = FALSE;
	for (;;) {
		bSuccess = ReadFile(red_output[0], chBuf, 512, &dwRead, NULL);
		if (!bSuccess || dwRead == 0)
			break;
		else {
			// convert read to string
			std::string str(chBuf, dwRead);
			ret += str;
		}
	}
#endif
	logdebug("7");
	profileDebug(TimeProfilingDebug, "");
	return ret;
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

void Test::TrimInput(int executed)
{
	StartProfilingDebug;
	if (!valid) {
		logcritical("called TrimInput after invalidation");
		return;
	}
	if (executed != -1)
	{
		// we are trimming to a specific length
		input->trimmed = true;
		int count = 0;
		auto aitr = input->begin();
		// add as long as we don't reach the end of the sequence and haven't added more than [executed] inputs
		while (aitr != input->end() && count != executed) {
			input->orig_sequence.push_back(*aitr);
			aitr++;
			count++;
		}
		std::swap(input->sequence, input->orig_sequence);
		logdebug("Test {}: trimmed input to length", identifier);

	} else {
		// we are trimming to match our iterator
		input->trimmed = true;
		auto aitr = input->begin();
		while (aitr != itr) {
			input->orig_sequence.push_back(*aitr);
			aitr++;
		}
		std::swap(input->sequence, input->orig_sequence);
		profileDebug(TimeProfilingDebug, "");
		logdebug("Test {}: trimmed input to iterator", identifier);
	}
}

int Test::GetExitCode()
{
	if (!valid) {
		logcritical("called GetExitCode after invalidation");
		return -1;
	}
#if defined(unix) || defined(__unix__) || defined(__unix)
	return exitcode;
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