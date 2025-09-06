#include "Test.h"
#include "Input.h"
#include "Processes.h"
#include "Logging.h"
#include "BufferOperations.h"
#include "Data.h"
#include "Session.h"
#include "SessionFunctions.h"
#include "LuaEngine.h"

#if defined(unix) || defined(__unix__) || defined(__unix)
#	include <poll.h>
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <fcntl.h>
#	include <unistd.h>
#	include <time.h>
#	include <stdlib.h>
#	include <stdio.h>
#	ifndef __USE_TIME64_REDIRECTS
#		ifndef __USE_FILE_OFFSET64
extern int fcntl(int __fd, int __cmd, ...);
#		else
#			ifdef __REDIRECT
extern int __REDIRECT(fcntl, (int __fd, int __cmd, ...), fcntl64);
#			else
#				define fcntl fcntl64
#			endif
#		endif
#		ifdef __USE_LARGEFILE64
extern int fcntl64(int __fd, int __cmd, ...);
#		endif
#	else /* __USE_TIME64_REDIRECTS */
#		ifdef __REDIRECT
extern int __REDIRECT_NTH(fcntl, (int __fd, int __request, ...),
	__fcntl_time64);
extern int __REDIRECT_NTH(fcntl64, (int __fd, int __request, ...),
	__fcntl_time64);
#		else
extern int __fcntl_time64(int __fd, int __request, ...) __THROW;
#			define fcntl64 __fcntl_time64
#			define fcntl __fcntl_time64
#		endif
#	endif

#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include <Psapi.h>
#	include <Windows.h>
#endif

Test::Test(std::shared_ptr<Functions::BaseFunction> a_callback, uint64_t id) :
	_callback(a_callback)
{
	_identifier = id;
	Init();
}

Test::~Test()
{
	Clear();
	if (_loadData)
		delete _loadData;
}

void Test::Init(std::shared_ptr<Functions::BaseFunction> a_callback, uint64_t id)
{
	_callback = a_callback;
	_identifier = id;
	_valid = true;
}

void Test::Init()
{
	StartProfiling;

#if defined(unix) || defined(__unix__) || defined(__unix)
	if (pipe2(red_output, O_NONBLOCK == -1)) {
		_exitreason = ExitReason::InitError;
		return;
	}
	//fcntl64(red_output[0], F_SETFL, O_NONBLOCK);
	//fcntl64(red_output[1], F_SETFL, O_NONBLOCK);
	if (pipe2(red_input, O_NONBLOCK == -1)) {
		close(red_output[0]);
		red_output[0] = -1;
		close(red_output[1]);
		red_output[1] = -1;
		_exitreason = ExitReason::InitError;
		return;
	}
	//fcntl(red_input[0], F_SETPIPE_SZ, PIPE_SIZE_LINUX);
	//fcntl(red_input[1], F_SETPIPE_SZ, PIPE_SIZE_LINUX);
	//fcntl(red_output[0], F_SETPIPE_SZ, PIPE_SIZE_LINUX);
	//fcntl(red_output[1], F_SETPIPE_SZ, PIPE_SIZE_LINUX);
	//fcntl64(red_input[0], F_SETFL, O_NONBLOCK);
	//fcntl64(red_input[1], F_SETFL, O_NONBLOCK);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::string tmpname = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	std::string pipe_name_input = "\\\\.\\pipe\\" + std::to_string(intptr_t(this)) + tmpname + "inp";
	std::string pipe_name_output = "\\\\.\\pipe\\" + std::to_string(intptr_t(this)) + tmpname + "out";
	// create pipes for _output

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
		_exitreason = ExitReason::InitError;
		return;
	}
	ZeroMemory(&saAttr, sizeof(saAttr));
	saAttr.nLength = sizeof(saAttr);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;
	red_output[1] = CreateFileA(pipe_name_output.c_str(), GENERIC_WRITE, 0, &saAttr, OPEN_EXISTING, 0, NULL);
	if (red_output[1] == INVALID_HANDLE_VALUE) {
		logcritical("failed to create event handle {}", GetLastError());
		_exitreason = ExitReason::InitError;
		if (red_output[0] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_output[0]);
			red_output[0] = INVALID_HANDLE_VALUE;
		}
		return;
	}

	BOOL success = ConnectNamedPipe(red_output[0], NULL);
	if (!success && GetLastError() != ERROR_PIPE_CONNECTED) {
		logcritical("cannot connect to named pipe {}", GetLastError());
		_exitreason = ExitReason::InitError;
		if (red_output[0] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_output[0]);
			red_output[0] = INVALID_HANDLE_VALUE;
		}
		if (red_output[1] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_output[1]);
			red_output[1] = INVALID_HANDLE_VALUE;
		}
		return;
	}

	// create pipes for _input
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
		_exitreason = ExitReason::InitError;
		if (red_output[0] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_output[0]);
			red_output[0] = INVALID_HANDLE_VALUE;
		}
		if (red_output[1] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_output[1]);
			red_output[1] = INVALID_HANDLE_VALUE;
		}
		return;
	}
	ZeroMemory(&saAttr, sizeof(saAttr));
	saAttr.nLength = sizeof(saAttr);
	saAttr.bInheritHandle = FALSE;
	saAttr.lpSecurityDescriptor = NULL;
	red_input[1] = CreateFileA(pipe_name_input.c_str(), GENERIC_WRITE, 0, &saAttr, OPEN_EXISTING, 0, NULL);
	if (red_input[1] == INVALID_HANDLE_VALUE) {
		logcritical("failed to create event handle {}", GetLastError());
		_exitreason = ExitReason::InitError;
		if (red_output[0] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_output[0]);
			red_output[0] = INVALID_HANDLE_VALUE;
		}
		if (red_output[1] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_output[1]);
			red_output[1] = INVALID_HANDLE_VALUE;
		}
		if (red_input[0] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_input[0]);
			red_input[0] = INVALID_HANDLE_VALUE;
		}
		return;
	}

	success = ConnectNamedPipe(red_input[0], NULL);
	if (!success && GetLastError() != ERROR_PIPE_CONNECTED) {
		logcritical("cannot connect to named pipe {}", GetLastError());
		_exitreason = ExitReason::InitError;
		if (red_output[0] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_output[0]);
			red_output[0] = INVALID_HANDLE_VALUE;
		}
		if (red_output[1] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_output[1]);
			red_output[1] = INVALID_HANDLE_VALUE;
		}
		if (red_input[0] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_input[0]);
			red_input[0] = INVALID_HANDLE_VALUE;
		}
		if (red_input[1] != INVALID_HANDLE_VALUE) {
			CloseHandle(red_input[1]);
			red_input[1] = INVALID_HANDLE_VALUE;
		}
		return;
	}

	/*
	if (!CreatePipe(&red_output[0], &red_output[1], &saAttr, PIPE_SIZE)) {
		_exitreason = ExitReason::InitError;
		return;
	}
	// set readhandle for _output to not be inherited
	SetHandleInformation(&red_output[0], HANDLE_FLAG_INHERIT, 0);
	if (!CreatePipe(&red_input[0], &red_input[1], &saAttr, PIPE_SIZE)) {
		_exitreason = ExitReason::InitError;
		return;
	}
	// set writehandle for _input to not be inherited
	SetHandleInformation(&red_input[1], HANDLE_FLAG_INHERIT, 0);*/
#endif
	_pipeinit = true;
	_avail = true;
	profile(TimeProfiling, "");
}

bool Test::IsRunning()
{
	StartProfilingDebug;
	if (!_valid) {
		logcritical("called IsRunning after invalidation");
		return false;
	}
#if defined(unix) || defined(__unix__) || defined(__unix)
	SpinlockA guard(_availFlag);
	bool res = Processes::GetProcessRunning(processid, &exitcode);
	if (res == false)
		_avail = false;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	bool res = Processes::GetProcessRunning(pi.hProcess);
#endif
	profileDebug(TimeProfilingDebug, "");
	if (res)
		return true;
	else {
		_endtime = std::chrono::steady_clock::now();
		_exitreason = ExitReason::Natural;
		return false;
	}
}

#if defined(unix) || defined(__unix__) || defined(__unix)
bool Test::WriteInput(std::string str, bool waitwrite)
#else
bool Test::WriteInput(std::string str, bool /*waitwrite*/)
#endif
{
	if (!_valid) {
		logcritical("called WriteInput after invalidation");
		return false;
	}
	int bSuccess = 0;
#if defined(unix) || defined(__unix__) || defined(__unix)
	SpinlockA guard(_availFlag);
	if (_avail == false)
		return false;
	size_t len = strlen(str.c_str());
	//if (IsRunning()) {
	struct pollfd fds;
	int32_t events = 0;
	fds.fd = red_input[1];  // stdin
	fds.events = POLLOUT;
	events = poll(&fds, 1, 0);
	// check it it is still running
	bool res = Processes::GetProcessRunning(processid, &exitcode);
	if (res == false) {
		_avail = false;
		return 0;
	}
	if ((fds.revents & POLLOUT) == POLLOUT) {
		if (waitwrite) {
			size_t totalwritten = 0;
			while (totalwritten != len) {
				size_t written = write(red_input[1], str.c_str() + totalwritten, len);
				if (written == -1) {
					if (errno == EPIPE) {
						_pipeError = true;
						//InValidate();
						return false;
					}
				}
				totalwritten += written;
			}
			return true;
		} else {
			if (size_t written = write(red_input[1], str.c_str(), len); written != len) {
				logcritical("Write to child is missing bytes: Supposed {}, written {}", len, written);
				if (written == -1) {
					if (errno == EPIPE) {
						_pipeError = true;
						//InValidate();
					}
				}
				return false;
			}
			return true;
		}
	} else
		return false;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	DWORD dwWritten;
	bSuccess = FALSE;
	const char* cstr = str.c_str();
	bSuccess = WriteFile(red_input[1], cstr, (DWORD)strlen(cstr), &dwWritten, NULL);
	if (bSuccess == false) {
		if (auto err = GetLastError(); err == ERROR_BROKEN_PIPE || err == ERROR_BAD_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
			_pipeError = true;
			//InValidate();
		}
	}
#endif
	return bSuccess;
}

long Test::Write(const char* data, size_t offset, size_t length)
{
	if (!_valid) {
		logcritical("called Write after invalidation");
		return false;
	}
	int bSuccess = 0;
#if defined(unix) || defined(__unix__) || defined(__unix)
	SpinlockA guard(_availFlag);
	if (_avail == false) {
		return 0;
	}
	//if (IsRunning()) {
	struct pollfd fds;
	int32_t events = 0;
	fds.fd = red_input[1];  // stdin
	fds.events = POLLOUT;
	events = poll(&fds, 1, 0);
	if ((fds.revents & POLLNVAL) || (fds.revents & POLLERR) || (fds.revents & POLLHUP)) {
		_pipeError = true;
		return -1;
	}
	if ((fds.revents & POLLOUT) == POLLOUT) {
		long written = write(red_input[1], data + offset, length);
		if (written == -1)
		{
			if (errno == EPIPE)
			{
				_pipeError = true;
				//InValidate();
			}
		}
		return written;
	} else
		return 0;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	DWORD dwWritten;
	bSuccess = FALSE;
	bSuccess = WriteFile(red_input[1], data + offset, (DWORD)length, &dwWritten, NULL);
	if (bSuccess)
		return dwWritten;
	else {
		if (auto err = GetLastError(); err == ERROR_BROKEN_PIPE || err == ERROR_BAD_PIPE || err == ERROR_PIPE_NOT_CONNECTED)
		{
			_pipeError = true;
			//InValidate();
		}
		return 0;
	}
#endif
}

bool Test::WriteNext(bool& error)
{
	if (!_valid) {
		logcritical("called WriteNext after invalidation");
		return false;
	}
	if (_itr == _itrend)
		return false;
	if (WriteInput(*_itr, false)) {
		_lasttime = std::chrono::steady_clock::now();
		_lastwritten = *_itr;
		_itr++;
		_executed++;
	} else
		error = true;
	return true;
}

bool Test::WriteAll()
{
	if (!_valid) {
		logcritical("called WriteAll after invalidation");
		return false;
	}
	auto itra = _itr;
	while (itra != _itrend) {
		WriteInput(*itra, true);
		_lastwritten += *itra;
		_executed++;
		itra++;
	}
	_lasttime = std::chrono::steady_clock::now();
	return true;
}

bool Test::CheckInput()
{
	StartProfilingDebug;
	if (!_valid) {
		logcritical("called CheckInput after invalidation");
		return false;
	}
	bool ret = false;
#if defined(unix) || defined(__unix__) || defined(__unix)
	SpinlockA guard(_availFlag);
	if (_avail == false)
		return false;
	struct pollfd fds;
	int32_t events = 0;
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
	int32_t sl = (int)strlen(_lastwritten.c_str());
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
	if (!_valid) {
		logcritical("called ReadOutput after invalidation");
		return "";
	}
	std::string ret;
#if defined(unix) || defined(__unix__) || defined(__unix)
	SpinlockA guard(_availFlag);
	if (_avail == false)
		return "";
	struct pollfd fds;
	int32_t events = 0;
	fds.fd = red_output[0];  // stdin
	fds.events = POLLIN;
	events = poll(&fds, 1, 0);
	while ((fds.revents & POLLIN) == POLLIN) {  // _input to read available
		char buf[512];
		int32_t _read = read(red_output[0], buf, 511);
		if (_read <= 0) {
			if (_read == -1) {
				if (errno == EBADF) // broken fd, count as pipe error
				{
					_pipeError = true;
					//InValidate();
				}
			}
			break;
		} else {
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
		if (!bSuccess) {
			if (auto err = GetLastError(); err == ERROR_BROKEN_PIPE || err == ERROR_BAD_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
				_pipeError = true;
				//InValidate();
				break;
			}
		}
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

int64_t Test::GetMemoryConsumption()
{
	if (!_valid) {
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
	if (!_valid) {
		logcritical("called KillProcess after invalidation");
		return false;
	}
	logdebug("Test {}: Killed Test", _identifier);
	_endtime = std::chrono::steady_clock::now();
#if defined(unix) || defined(__unix__) || defined(__unix)
	return Processes::KillProcess(processid);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	return Processes::KillProcess(pi.hProcess);
#endif
}

bool Test::WaitAndKillProcess()
{
#if defined(unix) || defined(__unix__) || defined(__unix)
	if (processid)
	{
		if (!Processes::WaitProcess(processid)) {
			//logmessage("Killing process");
			Processes::KillProcess(processid);
			return false;
		}
		return true;
	} else
		return true;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	return true;
#endif
}

void Test::TrimInput(int32_t executed)
{
	StartProfilingDebug;
	if (!_valid) {
		logcritical("called TrimInput after invalidation");
		return;
	}
	if (executed != -1)
	{
		if (auto ptr = _input.lock(); ptr) {
			ptr->TrimInput(executed);
			logdebug("Test {}: trimmed input to length", _identifier);
		}
	} else {
		if (auto ptr = _input.lock(); ptr) {
			ptr->TrimInput(_executed);
			/*// we are trimming to match our iterator
			ptr->_trimmed = true;
			auto aitr = ptr->begin();
			while (aitr != _itr && aitr != ptr->end()) {
				ptr->_orig_sequence.push_back(*aitr);
				aitr++;
			}
			std::swap(ptr->_sequence, ptr->_orig_sequence);
			ptr->_trimmedlength = ptr->_sequence.size();
			profileDebug(TimeProfilingDebug, "");*/
			logdebug("Test {}: trimmed input to iterator", _identifier);
		}
	}
}

int32_t Test::GetExitCode()
{
	if (!_valid) {
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
	_valid = false;
#if defined(unix) || defined(__unix__) || defined(__unix)
	processid = 0;
	close(red_input[0]);
	red_input[0] = -1;
	close(red_input[1]);
	red_input[1] = -1;
	close(red_output[0]);
	red_output[0] = -1;
	close(red_output[1]);
	red_output[1] = -1;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	if (red_input[0] != INVALID_HANDLE_VALUE)
		CloseHandle(red_input[0]);
	red_input[0] = INVALID_HANDLE_VALUE;
	if (red_input[1] != INVALID_HANDLE_VALUE)
		CloseHandle(red_input[1]);
	red_input[1] = INVALID_HANDLE_VALUE;
	if (red_output[0] != INVALID_HANDLE_VALUE)
		CloseHandle(red_output[0]);
	red_output[0] = INVALID_HANDLE_VALUE;
	if (red_output[1] != INVALID_HANDLE_VALUE)
		CloseHandle(red_output[1]);
	red_output[1] = INVALID_HANDLE_VALUE;
	if (pi.hProcess != INVALID_HANDLE_VALUE)
		CloseHandle(pi.hProcess);
	if (pi.hThread != INVALID_HANDLE_VALUE)
		CloseHandle(pi.hThread);
	pi.hProcess = INVALID_HANDLE_VALUE;
	pi.hThread = INVALID_HANDLE_VALUE;
	pi.dwProcessId = 0;
	pi.dwThreadId = 0;
	pi = {};
	si = {};
#endif
	_itr = {};
}

void Test::InValidatePreExec()
{
	_valid = false;
#if defined(unix) || defined(__unix__) || defined(__unix)
	processid = 0;
	close(red_input[0]);
	red_input[0] = -1;
	close(red_input[1]);
	red_input[1] = -1;
	close(red_output[0]);
	red_output[0] = -1;
	close(red_output[1]);
	red_output[1] = -1;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	CloseHandle(red_input[0]);
	red_input[0] = INVALID_HANDLE_VALUE;
	CloseHandle(red_input[1]);
	red_input[1] = INVALID_HANDLE_VALUE;
	CloseHandle(red_output[0]);
	red_output[0] = INVALID_HANDLE_VALUE;
	CloseHandle(red_output[1]);
	red_output[1] = INVALID_HANDLE_VALUE;
	pi.hProcess = INVALID_HANDLE_VALUE;
	pi.hThread = INVALID_HANDLE_VALUE;
	pi.dwProcessId = 0;
	pi.dwThreadId = 0;
	pi = {};
	si = {};
#endif
	_itr = {};
}

size_t Test::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4                     // version
	                        + 1                     // _running
	                        + 8                     // _starttime
	                        + 8                     // _endtime
	                        + 8                     // formid for _input
	                        + 8                     // _lasttime
	                        + 8                     // _identifier
	                        + 8                     // _exitreason
	                        + 1                     // valid
	                        + 1                     // has _callback
	                        + 1;                    // _skipOracle
	static size_t size0x2 = size0x1;

	switch (version)
	{
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	default:
		return 0;
	}
}

size_t Test::GetDynamicSize()
{
	size_t sz = Form::GetDynamicSize()  // form stuff
	            + GetStaticSize(classversion);
	sz += Buffer::_sizeof(_lastwritten);
	sz += Buffer::ArrayBuffer::GetContainerSize(_reactiontime);
	if (_storeoutput) {
		sz += Buffer::_sizeof(_output);
	} else {
		std::string str = std::string("");
		sz += Buffer::_sizeof(str);
	}
	//sz += Buffer::CalcStringLength(_cmdArgs);
	if (_callback)
		sz += _callback->GetLength() + 8 /*extra for writing to buffer*/;
	return sz;
}

void Test::PrintLengths()
{
	logcritical("Static Size: {}", GetStaticSize(classversion));
	logcritical("Form Size: {}", Form::GetDynamicSize());
	logcritical("_lastwritten: {}", Buffer::_sizeof(_lastwritten));
	logcritical("_reactiontime: {}", Buffer::ArrayBuffer::GetContainerSize(_reactiontime));
	if (_storeoutput) {
		logcritical("_output: {}", Buffer::_sizeof(_output));
	} else {
		std::string str = std::string("");
		logcritical("_output: {}", Buffer::_sizeof(str));
	}
	if (_callback)
		logcritical("_callback: {}", _callback->GetLength());
}

#define CHECK(x) if (x == false) { logcritical("Buffer overflow error, len: {}, off: {}", abuf.Size(), abuf.Free()); PrintLengths(); throw std::runtime_error(""); } 

bool Test::WriteData(std::ostream* buffer, size_t& offset, size_t length)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset, length);
	/*
	Buffer::Write(_running, buffer, offset);
	Buffer::Write(_starttime, buffer, offset);
	Buffer::Write(_endtime, buffer, offset);
	// shared ptr
	if (auto ptr = _input.lock(); ptr) {
		Buffer::Write(ptr->GetFormID(), buffer, offset);
	} else {
		Buffer::Write((FormID)0, buffer, offset);
	}
	Buffer::Write(_lastwritten, buffer, offset);
	Buffer::Write(_lasttime, buffer, offset);
	Buffer::ListBasic::WriteList(_reactiontime, buffer, offset);
	if (_storeoutput)
		Buffer::Write(_output, buffer, offset);
	else
		Buffer::Write(std::string(""), buffer, offset);
	Buffer::Write(_identifier, buffer, offset);
	Buffer::Write(_exitreason, buffer, offset);
	Buffer::Write(_valid, buffer, offset);
	Buffer::Write(_callback.operator bool(), buffer, offset);
	if (_callback)
		_callback->WriteData(buffer, offset);
	Buffer::Write(_skipOracle, buffer, offset);*/

	Buffer::ArrayBuffer abuf(length - offset);
	try {
		CHECK(abuf.Write(_running));
		CHECK(abuf.Write(_starttime));
		CHECK(abuf.Write(_endtime));
		if (auto ptr = _input.lock(); ptr) {
			CHECK(abuf.Write<FormID>(ptr->GetFormID()));
		} else {
			CHECK(abuf.Write<FormID>(0));
		}
		CHECK(abuf.Write(_lastwritten));
		CHECK(abuf.Write(_lasttime));
		CHECK(abuf.WriteList(_reactiontime));
		if (_storeoutput) {
			CHECK(abuf.Write(_output));
		} else {
			CHECK(abuf.Write<std::string>(""));
		}
		CHECK(abuf.Write(_identifier));
		CHECK(abuf.Write(_exitreason));
		CHECK(abuf.Write(_valid));
		CHECK(abuf.Write(_callback.operator bool()));
		if (_callback) {
			size_t sz = 0;
			auto buf = _callback->GetData(sz);
			CHECK(abuf.Write(buf, sz));
			delete buf;
		}
		CHECK(abuf.Write(_skipOracle));
	} catch (std::exception&) {
		auto [data, sz] = abuf.GetBuffer();
		buffer->write((char*)data, sz);
		offset += sz;
		return false;
	}
	
	auto [data, sz] = abuf.GetBuffer();
	buffer->write((char*)data, sz);
	offset += sz;

	if (offset - 12 > GetDynamicSize()) {
		logcritical("Overflow in Test::WriteData");
		//auto sz = GetDynamicSize();
	}
	return true;
}

bool Test::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	if (_loadData)
		delete _loadData;
	_loadData = new LoadData();

	size_t initoff = offset;
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version)
	{
	case 0x1:
		{
			if (length < GetStaticSize(version))
				return false;
			Form::ReadData(buffer, offset, length, resolver);
			_running = Buffer::ReadBool(buffer, offset);
			_starttime = Buffer::ReadTime(buffer, offset);
			_endtime = Buffer::ReadTime(buffer, offset);
			_loadData->formid = Buffer::ReadUInt64(buffer, offset);
			//if (length < offset - initoff + 8 || length < offset - initoff + Buffer::CalcStringLength(buffer, offset))
			//	return false;
			_lastwritten = Buffer::ReadString(buffer, offset);
			_lasttime = Buffer::ReadTime(buffer, offset);
			//if (length < offset - initoff + 8 || length < offset - initoff + Buffer::ListBasic::GetListLength(buffer, offset))
			//	return false;
			Buffer::ListBasic::ReadList(_reactiontime, buffer, offset);
			//if (length < offset - initoff + 8 || length < offset - initoff + Buffer::CalcStringLength(buffer, offset))
			//	return false;
			_output = Buffer::ReadString(buffer, offset);
			if (_output.empty() == false)
				_storeoutput = true;
			_identifier = Buffer::ReadUInt64(buffer, offset);
			_exitreason = Buffer::ReadUInt64(buffer, offset);
			_valid = Buffer::ReadBool(buffer, offset);
			bool hascall = Buffer::ReadBool(buffer, offset);
			if (hascall)
				_callback = Functions::BaseFunction::Create(buffer, offset, length, resolver);
			_skipOracle = Buffer::ReadBool(buffer, offset);
			// if this is a valid test that still needs to be run, generate the _cmdArgs anew
			//if (valid) {
			//	resolver->AddLateTask([this, resolver]() {
			//		bool stateerror = false;
			//		this->_cmdArgs = Lua::GetCmdArgs(std::bind(&Oracle::GetCmdArgs, resolver->_oracle, std::placeholders::_1, std::placeholders::_2), this, stateerror);
			//		if (stateerror) {
			//			logcritical("Read Test cannot be completed, as the calling thread lacks a lua context.");
			//			this->valid = false;
			//		}
			//	});
			//}
			// otherwise just leave them as is, we can generate them if we ever need them
			//else
				_cmdArgs = "";
			// do not init here, or we create an insane amount of useless file pointers
			//Init();
			return true;
		}
	case 0x2:
		{
			Form::ReadData(buffer, offset, length, resolver);
			try {
				Buffer::ArrayBuffer data(buffer, length - offset);
				offset += length - offset;
				_running = data.Read<bool>();
				_starttime = data.Read<std::chrono::steady_clock::time_point>();
				_endtime = data.Read<std::chrono::steady_clock::time_point>();
				_loadData->formid = data.Read<FormID>();
				_lastwritten = data.Read<std::string>();
				_lasttime = data.Read<std::chrono::steady_clock::time_point>();
				_reactiontime = data.ReadList<uint64_t>();
				_output = data.Read<std::string>();
				if (_output.empty() == false)
					_storeoutput = true;
				_identifier = data.Read<FormID>();
				_exitreason = data.Read<FormID>();
				_valid = data.Read<bool>();
				bool hascall = data.Read<bool>();
				if (hascall) {
					int64_t len = 0;
					unsigned char* buf = data.Read(&len);
					size_t off = 0;
					_callback = Functions::BaseFunction::Create(buf, off, len, resolver);
				}
				_skipOracle = data.Read<bool>();
				_cmdArgs = "";
			}
			catch (std::exception &e){
				logcritical("Exception in read method: {}", e.what());
			}
			return true;
		}
	default:
		return false;
	}
}

void Test::InitializeEarly(LoadResolver* resolver)
{
	if (_loadData) {
		auto input = resolver->ResolveFormID<Input>(_loadData->formid);
		if (input) {
			this->_itr = input->begin();
			this->_itrend = input->end();
			this->_input = input;
			if (!input->test) {
				input->test = resolver->ResolveFormID<Test>(_formid);
			}
		}
	}
}

void Test::InitializeLate(LoadResolver* /*resolver*/)
{
	if (_loadData) {
		__hash = Hashing::hash(this);
		delete _loadData;
		_loadData = nullptr;
	}
}

void Test::Delete(Data*)
{
	Clear();
}

void Test::Clear()
{
	Form::ClearForm();
	_running = false;
	_executed = 0;
	_lastwritten = "";
	_reactiontime.clear();
	_lua_reactiontime_next = _reactiontime.end();
	_cmdArgs.reset();
	_scriptArgs.reset();
	_output.reset();
	_exitreason = ExitReason::Running;
	_skipOracle = false;
	_skipExclusionCheck = false;
	_valid = true;
	_pipeinit = false;
	_input.reset();
	//if (_valid)
	//	InValidate();
	_callback.reset();
}

void Test::FreeMemory()
{
	if (TryLock()) {
		if (HasFlag(Form::FormFlags::DoNotFree) == false)
		{
			if (_storeoutput == false)
				_output.reset();
			_scriptArgs.reset();
			_cmdArgs.reset();
			_lastwritten = "";
			_lastwritten.shrink_to_fit();
		}
		Form::Unlock();
	}
}

bool Test::Freed()
{
	if (_cmdArgs.empty() && _scriptArgs.empty() && _output.empty())
		return true;
	return false;
}

size_t Test::MemorySize()
{
	return sizeof(Test) + _output.size() + _cmdArgs.size() + _scriptArgs.size() + _reactiontime.size() * sizeof(uint64_t) * 2;
}

void Test::DeepCopy(std::shared_ptr<Test> other)
{
	other->_valid = _valid;
	other->_exitreason = _exitreason;
	other->_identifier = _identifier;
	other->_storeoutput = _storeoutput;
	other->_output = _output;
	other->_cmdArgs = _cmdArgs;
	other->_reactiontime = _reactiontime;
	other->_lasttime = _lasttime;
	other->_lastwritten = _lastwritten;
	other->_itrend = _itrend;
	other->_itr = _itr;
	other->_endtime = _endtime;
	other->_starttime = _starttime;
	other->_running = _running;
}

bool Test::HasChanged()
{
	return Form::HasChanged() || __hash != Hashing::hash(this);
}

namespace Hashing
{
	size_t hash(Test* test)
	{
		if (!test)
			return 0;
		size_t hs = 0;

		hs = std::hash<bool>{}(test->_running);
		hs = hs ^ (std::hash<uint64_t>{}(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::time_point_cast<std::chrono::nanoseconds>(test->_starttime).time_since_epoch()).count()) << 1);
		hs = hs ^ (std::hash<uint64_t>{}(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::time_point_cast<std::chrono::nanoseconds>(test->_endtime).time_since_epoch()).count()) << 1);
		if (auto ptr = test->_input.lock(); ptr)
			hs = hs ^ (std::hash<FormID>{}(ptr->GetFormID()) << 1);
		else
			hs = hs ^ (std::hash<FormID>{}(0) << 1);

		hs = hs ^ (std::hash<std::string>{}(test->_lastwritten) << 1);
		hs = hs ^ (std::hash<uint64_t>{}(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::time_point_cast<std::chrono::nanoseconds>(test->_lasttime).time_since_epoch()).count()) << 1);
		hs = hs ^ (Hashing::hash(test->_reactiontime) << 1);
		hs = hs ^ (std::hash<std::string>{}(test->_output) << 1);
		hs = hs ^ (std::hash<bool>{}(test->_storeoutput) << 1);
		hs = hs ^ (std::hash<uint64_t>{}(test->_identifier) << 1);
		hs = hs ^ (std::hash<uint64_t>{}(test->_exitreason) << 1);
		hs = hs ^ (std::hash<bool>{}(test->_skipOracle) << 1);
		hs = hs ^ (std::hash<bool>{}(test->_valid) << 1);
		return hs;
	}
}


namespace Functions
{
	TestCallback::~TestCallback()
	{
		_input.reset();
		_sessiondata.reset();
	}

	void TestCallback::Run()
	{
		if (!_input) {
			logcritical("TestCallback was called, but _input was empty.");
			return;
		}
		if (!_sessiondata) {
			logcritical("TestCallback was called, but _sessiondata was empty.");
		}
		// check whether the input length doesn't match the devtree size
		if (_input->derive && (int64_t)_input->GetSequenceLength() > _input->derive->_sequenceNodes) {
			logcritical("Input is longer than dev tree large, Form: {}", Utility::PrintForm(_input));
			loginfo("Repeating test");
			_input->IncRetries();
			if (_input->test)
				_input->test->_exitreason = Test::ExitReason::Repeat;
			_input->Debug_ClearSequence();
			_input->SetGenerated(false);
			auto test = _input->test;
			_input->test.reset();
			auto callback = this->DeepCopy();
			_sessiondata->data->DeleteForm(test);
			_sessiondata->_exechandler->AddTest(_input, callback, true, false);
			SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Repeat);
			return;
		}

		// what happens when a test has finsihed?
		// 1. Input has been executed
		//		Check the execution status and decide what to do with the test, i.e. the meaning
		// 2. Call global management functions


		// if the test we just ran is a duplicate of an existing test,we can smply discard it
		// as the result is already in out data
		if (_input) {
			if (_input->HasFlag(Input::Flags::Duplicate)) {
				_sessiondata->data->DeleteForm(_input->test);
				if (_input->derive)
					_sessiondata->data->DeleteForm(_input->derive);
				_sessiondata->data->DeleteForm(_input);
			} else {
				// ----- SESSION STUFF -----
				if (SessionFunctions::TestEnd(_sessiondata, _input))
				{
					// the oracle decided there was a problem with the test, so go and repeat it
					loginfo("Repeating test");
					_input->IncRetries();
					if (_input->test)
						_input->test->_exitreason = Test::ExitReason::Repeat;
					_input->Debug_ClearSequence();
					_input->SetGenerated(false);
					auto test = _input->test;
					_input->test.reset();
					auto callback = this->DeepCopy();
					_sessiondata->data->DeleteForm(test);
					_sessiondata->_exechandler->AddTest(_input, callback, true, false);
					SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Repeat);
					// test has to be repeated
					return;
				}
				_input->UnsetFlag(Form::FormFlags::DoNotFree);
				_input->test->UnsetFlag(Form::FormFlags::DoNotFree);
				// free memory to save reclaim time
				_input->FreeMemory();
				if (_input->GetGenerated() == false && _input->test && _input->test->IsValid() == false) {
					if (_input->derive)
						_input->derive->FreeMemory();
					if (_input->test)
						_input->test->FreeMemory();
				}
			}
		}
		// schedule test generation
		SessionFunctions::GenerateTests(_sessiondata);
		// perform master checks
		SessionFunctions::MasterControl(_sessiondata);
	}

	std::shared_ptr<BaseFunction> TestCallback::DeepCopy()
	{
		auto ptr = std::make_shared<TestCallback>();
		ptr->_sessiondata = _sessiondata;
		ptr->_input = _input;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	bool TestCallback::ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of session and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		if (CmdArgs::_clearTasks == false)
			resolver->AddTask([this, sessid, resolver]() {
				this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
			});
		// get id of saved _input and resolve link
		uint64_t inputid = Buffer::ReadUInt64(buffer, offset);
		if (CmdArgs::_clearTasks == false)
			resolver->AddTask([this, inputid, resolver]() {
				this->_input = resolver->ResolveFormID<Input>(inputid);
			});
		return true;
	}

	bool TestCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of session and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		if (CmdArgs::_clearTasks == false)
			resolver->AddTask([this, sessid, resolver]() {
				this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
			});
		// get id of saved _input and resolve link
		uint64_t inputid = Buffer::ReadUInt64(buffer, offset);
		if (CmdArgs::_clearTasks == false)
			resolver->AddTask([this, inputid, resolver]() {
				this->_input = resolver->ResolveFormID<Input>(inputid);
			});
		return true;
	}

	bool TestCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_input->GetFormID(), buffer, offset);    // +8
		return true;
	}

	unsigned char* TestCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_input->GetFormID(), buffer, offset);        // +8
		size = GetLength();
		return buffer;
	}

	size_t TestCallback::GetLength()
	{
		return BaseFunction::GetLength() + 16;
	}

	void TestCallback::Dispose()
	{
		if (_input)
			if (_input->test)
				_input->test->_callback.reset();
		//_input.reset();
		//_sessiondata.reset();
	}

	void ReplayTestCallback::Run()
	{
		// this is the run funtion for replaying already run tests, 
		// thus we don't want to actually save the test and the _input,
		// but delete it instead
		if (SessionFunctions::TestEnd(_sessiondata, _input, true)) {
			// the oracle decided there was a problem with the test, so go and repeat it
			loginfo("Repeating test");
			_input->IncRetries();
			if (_input->test)
				_input->test->_exitreason = Test::ExitReason::Repeat;
			_input->Debug_ClearSequence();
			_input->SetGenerated(false);
			auto test = _input->test;
			_input->test.reset();
			auto callback = this->DeepCopy();
			_sessiondata->data->DeleteForm(test);
			_sessiondata->_exechandler->AddTest(_input, callback, true, false);
			SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Repeat);
			return;
		}

		if (_feedback != nullptr)
		{
			_feedback->Set(_input, _sessiondata);
		}

		_input->UnsetFlag(Form::FormFlags::DoNotFree);
		_input->test->UnsetFlag(Form::FormFlags::DoNotFree);
	}

	std::shared_ptr<BaseFunction> ReplayTestCallback::DeepCopy()
	{
		auto ptr = std::make_shared<ReplayTestCallback>();
		ptr->_sessiondata = _sessiondata;
		ptr->_input = _input;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}
}

void Test::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
		Functions::RegisterFactory(Functions::TestCallback::GetTypeStatic(), Functions::TestCallback::Create);
		Functions::RegisterFactory(Functions::ReplayTestCallback::GetTypeStatic(), Functions::ReplayTestCallback::Create);
	}
}
