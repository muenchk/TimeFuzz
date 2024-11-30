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

class Settings;
class TaskController;
class Input;
class Oracle;
class Session;
class SessionData;

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
			/// An error occured when starting the test -> discard result
			/// </summary>
			InitError = 1 << 10,
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
	std::list<std::string>::iterator _itr;
	/// <summary>
	/// the number of fragments executed
	/// </summary>
	int32_t _executed = 0;
	/// <summary>
	/// the end iterator of the sequence
	/// </summary>
	std::list<std::string>::iterator _itrend;
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
	std::string _cmdArgs;
	/// <summary>
	/// stdin arguments for scripts
	/// </summary>
	std::string _scriptArgs;

	/// <summary>
	/// output of the PUT
	/// </summary>
	std::string _output;
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
	/// Returns whether the test is still running
	/// </summary>
	/// <returns></returns>
	bool IsRunning();

	/// <summary>
	/// Writes input to the stdint of the PUT
	/// </summary>
	/// <param name=""></param>
	bool WriteInput(std::string);
	/// <summary>
	/// Writes next input to PUT
	/// </summary>
	bool WriteNext();
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

	void DeepCopy(std::shared_ptr<Test> other);

	const int32_t classversion = 0x1;

	size_t GetStaticSize(int32_t version = 0x1);
	size_t GetDynamicSize();
	virtual bool WriteData(std::ostream* buffer, size_t& offset);
	virtual bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver);
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


private:
	inline static bool _registeredFactories = false;
	bool _valid = true;
	bool _pipeinit = false;
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

		bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver);
		bool WriteData(std::ostream* buffer, size_t& offset);

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<TestCallback>()); }
		void Dispose();
		size_t GetLength();
	};

	class ReplayTestCallback : public TestCallback
	{
	public:
		UI::UIInputInformation* _feedback;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'RTES'; }
		uint64_t GetType() override { return 'RTES'; }
		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<ReplayTestCallback>()); }
	};
}
