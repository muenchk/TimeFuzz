#pragma once 

#if defined(unix) || defined(__unix__) || defined(__unix)
#	include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include <Windows.h>
#	include <processthreadsapi.h>
#endif

#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <list>

#include "Form.h"
#include "Function.h"
#include "UIClasses.h"
#include "Types.h"

class Settings;
class TaskController;
class Input;
class Oracle;
class Session;
class SessionData;

#define PIPE_SIZE 1048576
#define PIPE_SIZE_LINUX 65536

class Test;

namespace Hashing
{
	size_t hash(Test* node);
}

class Test : public Form
{
	void Init();

public:
	struct ExitReason
	{
		enum ExitReasons
		{
			/// <summary>
			/// Process hasn't exited
			/// </summary>
			Running = 0,
			/// <summary>
			/// Process has exited naturally after finishing computation
			/// </summary>
			Natural = 1 << 0,
			/// <summary>
			/// Last input was written and worked and there wasn't a result
			/// </summary>
			LastInput = 1 << 1,
			/// <summary>
			/// Process has been terminated, for some reason
			/// </summary>
			Terminated = 1 << 2,
			/// <summary>
			/// Process has been terminated due to exceeding a timeout
			/// </summary>
			Timeout = 1 << 3,
			/// <summary>
			/// Process has been terminated due to exceeding timeout fragment
			/// </summary>
			FragmentTimeout = 1 << 4,
			/// <summary>
			/// Process has been terminated due to using up too much memory
			/// </summary>
			Memory = 1 << 5,
			/// <summary>
			/// Process has been terminated due to a pipe error
			/// </summary>
			Pipe = 1 << 6,
			/// <summary>
			/// An error occured when starting the test -> discard result
			/// </summary>
			InitError = 1 << 10,
			/// <summary>
			/// Not really an exit reason as this is decided by the oracle
			/// </summary>
			Repeat = 1 << 11,
		};
	};

	Test(std::shared_ptr<Functions::BaseFunction> a_callback, uint64_t id);
	Test() {}
	void Init(std::shared_ptr<Functions::BaseFunction> a_callback, uint64_t id);

	void PrepareForExecution() { Init(); }

	/// <summary>
	/// whether test is currently running
	/// </summary>
	bool _running = false;
	/// <summary>
	/// the time of the tests start
	/// </summary>
	std::chrono::steady_clock::time_point _starttime;
	/// <summary>
	/// the time of the tests end
	/// </summary>
	std::chrono::steady_clock::time_point _endtime;

	/// <summary>
	/// _runtime variable that defines when an input is considered timeouted
	/// </summary>
	std::chrono::steady_clock::time_point _timeouttime;
	/// <summary>
	/// the input for the PUT
	/// </summary>
	std::weak_ptr<Input> _input;
	/// <summary>
	/// the iterator the to the next sequence element to be given to executable
	/// </summary>
	std::vector<std::string>::iterator _itr;
	/// <summary>
	/// the number of fragments executed
	/// </summary>
	int32_t _executed = 0;
	/// <summary>
	/// the end iterator of the sequence
	/// </summary>
	std::vector<std::string>::iterator _itrend;
	/// <summary>
	/// last string written to program
	/// </summary>
	std::string _lastwritten;
	/// <summary>
	/// time the last input was given to the process
	/// </summary>
	std::chrono::steady_clock::time_point _lasttime;
	/// <summary>
	/// reaction time for each fragment [on whole test this contains only one element]
	/// </summary>
	std::list<uint64_t> _reactiontime;
	std::list<uint64_t>::iterator _lua_reactiontime_next;
	/// <summary>
	/// called after the test has been finished
	/// </summary>
	std::shared_ptr<Functions::BaseFunction> _callback;
	/// <summary>
	/// command line args for the test
	/// </summary>
	String _cmdArgs;
	/// <summary>
	/// stdin arguments for scripts
	/// </summary>
	String _scriptArgs;

	/// <summary>
	/// output of the PUT
	/// </summary>
	String _output;
	/// <summary>
	/// whether to store the put output
	/// </summary>
	bool _storeoutput;

	/// <summary>
	/// unique test identifier
	/// </summary>
	uint64_t _identifier;

	/// <summary>
	/// reason the process has exited
	/// </summary>
	uint64_t _exitreason = ExitReason::Running;

	// process stuff
#if defined(unix) || defined(__unix__) || defined(__unix)
	pid_t processid;
	int32_t red_input[2];
	int32_t red_output[2];
	int32_t exitcode = -1;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	HANDLE red_input[2];
	HANDLE red_output[2];
#endif

	/// <summary>
	/// this is a flag indicating that this test was already run and no oracle is necessary
	/// </summary>
	bool _skipOracle = false;

	/// <summary>
	/// If set to true, this test bypasses addtional exclusion checks
	/// for instance if the test is a replay of an already executed test
	/// </summary>
	bool _skipExclusionCheck = false;

	/// <summary>
	/// whether there was an error in the pipe
	/// </summary>
	bool _pipeError = false;

	/// <summary>
	/// Returns whether the test is still running
	/// </summary>
	/// <returns></returns>
	bool IsRunning();

	/// <summary>
	/// Writes input to the stdint of the PUT
	/// </summary>
	/// <param name=""></param>
	bool WriteInput(std::string, bool waitwrite);
	/// <summary>
	/// Writes data to the stdint of the PUT
	/// </summary>
	/// <param name="data">the data to write</param>
	/// <param name="offset">the offset to start at</param>
	/// <param name="length">the number of bytes to write</param>
	/// <returns></returns>
	long Write(const char* data, size_t offset, size_t length);
	/// <summary>
	/// Writes next input to PUT
	/// </summary>
	bool WriteNext(bool& error);
	/// <summary>
	/// Writes all input to PUT
	/// </summary>
	/// <returns></returns>
	bool WriteAll();
	/// <summary>
	/// Checks whether the PUT has reacted to the last input, by reading from and rewriting to pipe.
	/// </summary>
	/// <returns>returns whether input has been handled</returns>
	bool CheckInput();
	/// <summary>
	/// reads the output of the PUT
	/// </summary>
	/// <returns></returns>
	std::string ReadOutput();

	/// <summary>
	/// returns the memory used by the process
	/// </summary>
	/// <returns></returns>
	int64_t GetMemoryConsumption();
	/// <summary>
	/// terminates the process
	/// </summary>
	/// <returns></returns>
	bool KillProcess();

	bool WaitAndKillProcess();

	/// <summary>
	/// trims input to the last executed
	/// </summary>
	void TrimInput(int32_t executed = -1);

	/// <summary>
	/// returns the exitcode
	/// </summary>
	int32_t GetExitCode();

	/// <summary>
	/// invalidates the test [functions cannot be called anymore]
	/// </summary>
	void InValidate();
	/// <summary>
	/// invalidates the test [functions cannot be called anymore]
	/// </summary>
	void InValidatePreExec();

	/// <summary>
	/// returns whether the test is valid and still needs to be run
	/// </summary>
	/// <returns></returns>
	bool IsValid() { return _valid; }

	/// <summary>
	/// returns wether the test is valid and the pipes are initialized
	/// </summary>
	/// <returns></returns>
	bool IsInitialized() { return _valid && _pipeinit; }

	bool PipeError() { return _pipeError; }

	void DeepCopy(std::shared_ptr<Test> other);

	const int32_t classversion = 0x2;

	void PrintLengths();

private:
	struct LoadData
	{
		FormID formid;
	};
	LoadData* _loadData = nullptr;

public:

	size_t GetStaticSize(int32_t version);
	size_t GetDynamicSize();
	virtual bool WriteData(std::ostream* buffer, size_t& offset, size_t length);
	virtual bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver);
	/// <summary>
	/// Early initialiazation pass [is executed once all forms have been loaded
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeEarly(LoadResolver* resolver) override;
	/// <summary>
	/// late initialization pass [is executed once all Initialize Early has been called for all forms]
	/// </summary>
	/// <param name="resolver"></param>
	void InitializeLate(LoadResolver* resolver) override;
	int32_t GetType() override
	{
		return FormType::Test;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::Test;
	}
	void Delete(Data* data) override;
	void Clear() override;
	static void RegisterFactories();

	void FreeMemory() override;
	/// <summary>
	/// returns whether the memmory of this form has been freed
	/// </summary>
	/// <returns></returns>
	bool Freed() override;
	size_t MemorySize() override;

	virtual bool HasChanged() override;

private:
	inline static bool _registeredFactories = false;
	bool _valid = true;
	bool _pipeinit = false;
	bool _avail = false;
	std::atomic_flag _availFlag = ATOMIC_FLAG_INIT;

	size_t __hash = 0;
	friend size_t Hashing::hash(Test* node);
};

namespace Functions
{
	class TestCallback : public BaseFunction
	{
	public:
		std::shared_ptr<SessionData> _sessiondata;
		std::shared_ptr<Input> _input;

		~TestCallback();

		void Run() override;
		static uint64_t GetTypeStatic() { return 'TEST'; }
		uint64_t GetType() override { return 'TEST'; }

		FunctionType GetFunctionType() override { return FunctionType::Light; }

		virtual std::shared_ptr<BaseFunction> DeepCopy() override;

		bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool WriteData(std::ostream* buffer, size_t& offset) override;
		unsigned char* GetData(size_t& size) override;

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<TestCallback>()); }
		void Dispose() override;
		size_t GetLength() override;

		virtual const char* GetName() override
		{
			return "TestCallback";
		}
	};

	class ReplayTestCallback : public TestCallback
	{
	public:
		UI::UIInputInformation* _feedback = nullptr;

		void Run() override;

		virtual std::shared_ptr<BaseFunction> DeepCopy() override;

		static uint64_t GetTypeStatic() { return 'RTES'; }
		uint64_t GetType() override { return 'RTES'; }
		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<ReplayTestCallback>()); }

		virtual const char* GetName() override
		{
			return "ReplayTestCallback";
		}
	};
}
