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
#	include <fcntl.h>
#	include <poll.h>
#	include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include <Psapi.h>
#	include <Windows.h>
#endif

#define PIPE_SIZE 65536

Test::Test(std::shared_ptr<Functions::BaseFunction> a_callback, uint64_t id) :
	_callback(a_callback)
{
	_identifier = id;
	Init();
}

void Test::Init(std::shared_ptr<Functions::BaseFunction> a_callback, uint64_t id)
{
	_callback = a_callback;
	_identifier = id;
	_valid = true;
	Init();
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
		_exitreason = ExitReason::InitError;
		return;
	}
	//fcntl64(red_input[0], F_SETFL, O_NONBLOCK);
	//fcntl64(red_input[1], F_SETFL, O_NONBLOCK);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::string pipe_name_input = "\\\\.\\pipe\\" + std::to_string(intptr_t(this)) + "inp";
	std::string pipe_name_output = "\\\\.\\pipe\\" + std::to_string(intptr_t(this)) + "out";
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
		return;
	}

	BOOL success = ConnectNamedPipe(red_output[0], NULL);
	if (!success && GetLastError() != ERROR_PIPE_CONNECTED) {
		logcritical("cannot connect to named pipe {}", GetLastError());
		_exitreason = ExitReason::InitError;
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
		return;
	}

	success = ConnectNamedPipe(red_input[0], NULL);
	if (!success && GetLastError() != ERROR_PIPE_CONNECTED) {
		logcritical("cannot connect to named pipe {}", GetLastError());
		_exitreason = ExitReason::InitError;
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
	bool res = Processes::GetProcessRunning(processid, &exitcode);
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

void Test::WriteInput(std::string str)
{
	StartProfilingDebug;
	if (!_valid) {
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
	if (!_valid) {
		logcritical("called WriteNext after invalidation");
		return false;
	}
	if (_itr == _itrend)
		return false;
	WriteInput(*_itr);
	_lasttime = std::chrono::steady_clock::now();
	_lastwritten = *_itr;
	_itr++;
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
		WriteInput(*itra);
		_lastwritten += *itra;
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
	struct pollfd fds;
	int32_t events = 0;
	fds.fd = red_output[0];  // stdin
	fds.events = POLLIN;
	events = poll(&fds, 1, 0);
	logdebug("0: {}", events);
	while ((fds.revents & POLLIN) == POLLIN) {  // _input to read available
		logdebug("1");
		char buf[512];
		logdebug("2");
		logdebug("3: {}", red_output[0]);
		int32_t _read = read(red_output[0], buf, 511);
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
			// we are trimming to match our iterator
			ptr->_trimmed = true;
			auto aitr = ptr->begin();
			while (aitr != _itr) {
				ptr->_orig_sequence.push_back(*aitr);
				aitr++;
			}
			std::swap(ptr->_sequence, ptr->_orig_sequence);
			ptr->_trimmedlength = ptr->_sequence.size();
			profileDebug(TimeProfilingDebug, "");
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
	CloseHandle(red_input[0]);
	red_input[0] = nullptr;
	CloseHandle(red_input[1]);
	red_input[1] = nullptr;
	CloseHandle(red_output[0]);
	red_output[0] = nullptr;
	CloseHandle(red_output[1]);
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
	red_input[0] = nullptr;
	CloseHandle(red_input[1]);
	red_input[1] = nullptr;
	CloseHandle(red_output[0]);
	red_output[0] = nullptr;
	CloseHandle(red_output[1]);
	red_output[1] = nullptr;
	pi.hProcess = nullptr;
	pi.hThread = nullptr;
	pi.dwProcessId = 0;
	pi.dwThreadId = 0;
	pi = {};
	si = {};
#endif
	_itr = {};
}

size_t Test::GetStaticSize(int32_t version)
{
	static size_t size0x1 = Form::GetDynamicSize()  // form base size
	                        + 4                     // version
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
	switch (version)
	{
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t Test::GetDynamicSize()
{
	size_t sz = GetStaticSize(classversion);
	sz += Buffer::CalcStringLength(_lastwritten);
	sz += Buffer::ListBasic::GetListLength(_reactiontime);
	sz += Buffer::CalcStringLength(_output);
	//sz += Buffer::CalcStringLength(_cmdArgs);
	if (_callback)
		sz += _callback->GetLength();
	return sz;
}

bool Test::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(_running, buffer, offset);
	Buffer::Write(_starttime, buffer, offset);
	Buffer::Write(_endtime, buffer, offset);
	// shared ptr
	if (auto ptr = _input.lock(); ptr) {
		Buffer::Write(ptr->GetFormID(), buffer, offset);
	} else {
		Buffer::Write((int32_t)0, buffer, offset);
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
	Buffer::Write(_skipOracle, buffer, offset);
	return true;
}

bool Test::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
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
			FormID formid = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, formid]() {
				auto input = resolver->ResolveFormID<Input>(formid);
				if (input) {
					this->_itr = input->begin();
					this->_itrend = input->end();
					this->_input = input;
				}
			});
			if (length < offset - initoff + 8 || length < offset - initoff + Buffer::CalcStringLength(buffer, offset))
				return false;
			_lastwritten = Buffer::ReadString(buffer, offset);
			_lasttime = Buffer::ReadTime(buffer, offset);
			if (length < offset - initoff + 8 || length < offset - initoff + Buffer::ListBasic::GetListLength(buffer, offset))
				return false;
			Buffer::ListBasic::ReadList(_reactiontime, buffer, offset);
			if (length < offset - initoff + 8 || length < offset - initoff + Buffer::CalcStringLength(buffer, offset))
				return false;
			_output = Buffer::ReadString(buffer, offset);
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
	default:
		return false;
	}
}

void Test::Delete(Data*)
{
	Clear();
}

void Test::Clear()
{
	_running = false;
	_input.reset();
	InValidate();
	if (_callback)
	{
		_callback->Dispose();
		_callback.reset();
	}
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



namespace Functions
{
	void TestCallback::Run()
	{
		// what happens when a test has finsihed?
		// 1. Input has been executed
		//		Check the execution status and decide what to do with the test, i.e. the meaning
		// 2. Call global management functions




		// ----- DO SOME NICE INTERNAL COMPUTATION -----


		// ----- SESSION STUFF -----
		SessionFunctions::TestEnd(_sessiondata, _input);
		// schedule test generation
		SessionFunctions::GenerateTests(_sessiondata);
		// perform master checks
		SessionFunctions::MasterControl(_sessiondata);
	}

	bool TestCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of session and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
		});
		// get id of saved _input and resolve link
		uint64_t inputid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, inputid, resolver]() {
			this->_input = resolver->ResolveFormID<Input>(inputid);
		});
		return true;
	}

	bool TestCallback::WriteData(unsigned char* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_input->GetFormID(), buffer, offset);    // +8
		return true;
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
		_input.reset();
		_sessiondata.reset();
	}

	void ReplayTestCallback::Run()
	{
		// this is the run funtion for replaying already run tests, 
		// thus we don't want to actually save the test and the _input,
		// but delete it instead
		SessionFunctions::TestEnd(_sessiondata, _input, true);
	}
}

void Test::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
		Functions::RegisterFactory(Functions::TestCallback::GetTypeStatic(), Functions::TestCallback::Create);
	}
}
