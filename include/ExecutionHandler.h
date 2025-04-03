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
#include <deque>
#include <condition_variable>
#include <list>
#include <semaphore>

#include "Test.h"
#include "Form.h"
#include "Record.h"
#include "Function.h"

class Settings;
class TaskController;
class Input;
class Oracle;
class Session;

/* Class Notes
* The class is the overall handler for all gtest execution
* It heavily relies on the usage of function delegates to customize the test execution
* The two main modes are Fragments execution and Whole execution.
*	Fragment execution: executes small fragments and regularly checks whether the fragment
*		has finished executing. If it hasn't, a timeout will be aplied and the test aborted if
*		it has been exceeded. The test will also be aborted once a general timeout is exceeded
*	Whole Execution: Executes the test in one go, and applies an overall timeout
*/

struct stop_token
{
private:
	bool stop = false;

public:
	void request_stop()
	{
		stop = true;
	}

	bool stop_requested()
	{
		return stop;
	}
};

namespace Functions
{
	class ExecInitTestsCallback : public BaseFunction
	{
	public:
		std::shared_ptr<SessionData> _sessiondata;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'EHTC'; }
		uint64_t GetType() override { return 'EHTC'; }

		FunctionType GetFunctionType() override { return FunctionType::Light; }

		virtual std::shared_ptr<BaseFunction> DeepCopy() override;

		bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool WriteData(std::ostream* buffer, size_t& offset) override;

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<ExecInitTestsCallback>()); }
		static std::shared_ptr<BaseFunction> CreateFull(std::shared_ptr<SessionData> sessiondata);
		void Dispose() override;
		size_t GetLength() override;

		virtual const char* GetName() override
		{
			return "ExecInitTestsCallback";
		}
	};

	class WriteTestInputCallback : public BaseFunction
	{
	public:
		std::shared_ptr<Test> _test;
		char* data;
		size_t length;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'WTIC'; }
		uint64_t GetType() override { return 'WTIC'; }

		FunctionType GetFunctionType() override { return FunctionType::Light; }

		virtual std::shared_ptr<BaseFunction> DeepCopy() override;

		bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool WriteData(std::ostream* buffer, size_t& offset) override;

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<WriteTestInputCallback>()); }
		void Dispose() override;
		size_t GetLength() override;

		virtual const char* GetName() override
		{
			return "WriteTestInputCallback";
		}
	};
}

enum class ExecHandlerStatus
{
	None,
	Sleeping,
	MainLoop,
	StartingTests,
	Exitted,
	Waiting,
	HandlingTests,
	KillingProcessMemory,
	WriteFragment,
	KillingProcessTimeout,
	StoppingTest
};

class ExecutionHandler: public Form
{

private:
	/// <summary>
	/// the current session
	/// </summary>
	std::shared_ptr<Session> _session;
	/// <summary>
	/// the current sessiondata
	/// </summary>
	std::shared_ptr<SessionData> _sessiondata;
	/// <summary>
	/// settings of the current session
	/// </summary>
	std::shared_ptr<Settings> _settings;
	/// <summary>
	/// taskcontroller used to execute callbacks
	/// </summary>
	std::shared_ptr<TaskController> _threadpool;
	/// <summary>
	/// test oracle
	/// </summary>
	std::shared_ptr<Oracle> _oracle;
	/// <summary>
	/// maximum amount of concurrently active tests
	/// </summary>
	int32_t _maxConcurrentTests = 1;
	/// <summary>
	/// currently active tests
	/// </summary>
	int32_t _currentTests = 0;
	/// <summary>
	/// Maximum amount of tests that can be fully initialized for execution at the same time
	/// </summary>
	int32_t _populationSize = 50;
	/// <summary>
	/// lock for the internal loop and test queue synchronisation
	/// </summary>
	std::mutex _lockqueue;
	/// <summary>
	/// condition variable for waiting in the InternalQueue
	/// </summary>
	std::condition_variable _waitforjob;
	/// <summary>
	/// queue for tests waiting to be initialized for execution
	/// </summary>
	std::deque<std::weak_ptr<Test>> _waitingTests;
	/// <summary>
	/// queue for tests waiting to be executed
	/// </summary>
	std::deque<std::weak_ptr<Test>> _waitingTestsExec;
	/// <summary>
	/// list holding currently active tests
	/// </summary>
	std::list<std::weak_ptr<Test>> _runningTests;
	/// <summary>
	///  atomic flag for access to _runningTests
	/// </summary>
	std::atomic_flag _runningTestsFlag = ATOMIC_FLAG_INIT;
	/// <summary>
	/// internal vector holding temp references to actively handled tests
	/// </summary>
	std::vector<std::shared_ptr<Test>> _handleTests;
	/// <summary>
	/// test currently starting
	/// </summary>
	std::queue<std::shared_ptr<Test>> _startingTests;
	std::mutex _startingLock;
	std::condition_variable _startingLockCond;
	/// <summary>
	/// list holding stopped tests while handler is frozen
	/// </summary>
	std::list<std::shared_ptr<Test>> _stoppingTests;
	/// <summary>
	/// synchronises access to all top-level functions, including start, stop, wait
	/// </summary>
	std::mutex _toplevelsync;
	/// <summary>
	/// condition used to wait on handler exiting
	/// </summary>
	std::condition_variable _waitforhandler;
	/// <summary>
	/// whether the execution handler is currently running
	/// </summary>
	bool _active = false;
	/// <summary>
	/// next test identifier
	/// </summary>
	uint64_t _nextid = 1;
	/// <summary>
	/// whether the instace has been cleared for deletion
	/// </summary>
	bool _cleared = false;
	/// <summary>
	/// freezes test execution
	/// </summary>
	bool _freeze = false;
	/// <summary>
	/// tells the freeze strategy to wait for test completion
	/// </summary>
	bool _freeze_waitfortestcompletion = false;
	/// <summary>
	/// response to freezing
	/// </summary>
	bool _frozen = false;
	/// <summary>
	/// response to freezing
	/// </summary>
	bool _frozenStarter = false;
	/// <summary>
	/// condition used to work with frozen TestStarter
	/// </summary>
	std::condition_variable _freezecond;
	/// <summary>
	/// locks access to _stoppingtests to provide a consistent evironment during freeze
	/// </summary>
	std::mutex _freezelock;

	/// <summary>
	/// time handler waits between cycles in nanoseconds
	/// </summary>
	std::chrono::nanoseconds _waittime = std::chrono::nanoseconds(100000000);
	/// <summary>
	/// int-value for waittime
	/// </summary>
	int64_t _waittimeL = 100000000;

	/// <summary>
	/// last time the InternalLoop ran the main loop
	/// </summary>
	std::chrono::steady_clock::time_point _lastExec;

	/// <summary>
	/// whether to enable fragment execution
	/// </summary>
	bool _enableFragments = false;

	/// <summary>
	/// whether to stop the execution handler
	/// </summary>
	bool _stopHandler = false;

	/// <summary>
	/// whether to finish tests before stopping the handler
	/// </summary>
	bool _finishtests = false;

	/// <summary>
	/// indicates that the handler has been stale, forces recomputation of test timeouts
	/// </summary>
	bool _stale = false;

	/// <summary>
	/// status of the internal thread;
	/// </summary>
	ExecHandlerStatus _tstatus;

	std::thread _thread;

	std::shared_ptr<stop_token> _threadStopToken;

	std::thread _threadTS;

	std::shared_ptr<stop_token> _threadTSStopToken;

	void InternalLoop(std::shared_ptr<stop_token> stoken);

	void TestStarter(std::shared_ptr<stop_token> stoken);

	bool StartTest(std::shared_ptr<Test> test);

	void StopTest(std::shared_ptr<Test> test);

	/// <summary>
	/// class version
	/// </summary>
	const int32_t classversion = 0x1;

	void InitTestsLockFree();

public:
	ExecutionHandler();
	~ExecutionHandler();

	void Init(std::shared_ptr<Session> session, std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Settings> settings, std::shared_ptr<TaskController> threadpool, int32_t maxConcurrentTests, std::shared_ptr<Oracle> oracle);

	static ExecutionHandler* GetSingleton();

	/// <summary>
	/// enables execution of test fragments
	/// </summary>
	/// <param name="enable"></param>
	void SetEnableFragments(bool enable = true);

	/// <summary>
	/// Changes the maximum number of tests run concurrently
	/// </summary>
	/// <param name="maxConcurrentTests"></param>
	void SetMaxConcurrentTests(int32_t maxConcurrentTests);

	/// <summary>
	/// starts the handler
	/// </summary>
	void StartHandler();
	/// <summary>
	/// resets the internal thread
	/// </summary>
	void ReinitHandler();
	/// <summary>
	/// starts the handler without invalidating existing tests
	/// </summary>
	void StartHandlerAsIs();
	/// <summary>
	/// stops the handler after all tests have been completed.
	/// Controll will not return until the handler stops
	/// </summary>
	void StopHandlerAfterTestsFinishAndWait();
	/// <summary>
	/// tells the handler to terminate [kills all running tests]
	/// </summary>
	void StopHandler();
	/// <summary>
	/// waits for the handler to actually terminate
	/// </summary>
	void WaitOnHandler();

	/// <summary>
	/// returns whether the internal thread is stale, i.e. the last main loop execution is older than [dur]
	/// </summary>
	/// <param name="dur"></param>
	/// <returns></returns>
	bool IsStale(std::chrono::milliseconds dur);

	/// <summary>
	/// Returns the number of currently waiting tests
	/// </summary>
	/// <returns></returns>
	size_t WaitingTasks() { return _waitingTests.size() + _waitingTestsExec.size() + _runningTests.size() + _stoppingTests.size() + _startingTests.size(); }

	/// <summary>
	/// sets the period of the test engine
	/// </summary>
	/// <param name="period"></param>
	void SetPeriod(std::chrono::nanoseconds period);

	/// <summary>
	/// Adds a new test to the queue [passes of test handling to external application/script]
	/// </summary>
	/// <param name="input">The input to run the test on</param>
	/// <param name="callback">called after the test has been finished</param>
	/// <param name="bypass">Bypass queue and start as soon as possible</param>
	bool AddTest(std::shared_ptr<Input> input, std::shared_ptr<Functions::BaseFunction> callback, bool bypass = false, bool replay = false);

	void InitTests();

	/// <summary>
	/// Freezes test execution
	/// </summary>
	void Freeze(bool waitfortestcompletion);
	/// <summary>
	/// Resumes test execution
	/// </summary>
	void Thaw();
	/// <summary>
	/// returns whether the executionhandler is currently frozen
	/// </summary>
	/// <returns></returns>
	bool IsFrozen();

	/// <summary>
	/// Clears all active and waiting tests 
	/// </summary>
	void ClearTests();

	/// <summary>
	/// Returns the number of waiting tests
	/// </summary>
	/// <returns></returns>
	int32_t GetWaitingTests();
	/// <summary>
	/// Returns the number of waiting tests
	/// </summary>
	/// <returns></returns>
	int32_t GetInitializedTests();
	/// <summary>
	/// Returns the number of running tests
	/// </summary>
	/// <returns></returns>
	int32_t GetRunningTests();
	/// <summary>
	/// Returns the number of stopping tests
	/// </summary>
	/// <returns></returns>
	int32_t GetStoppingTests();

	/// <summary>
	/// returns the status if the internal thread
	/// </summary>
	/// <returns></returns>
	ExecHandlerStatus GetThreadStatus();

	size_t GetStaticSize(int32_t version = 0x1) override;
	size_t GetDynamicSize() override;
	bool WriteData(std::ostream* buffer, size_t& offset) override;
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	static int32_t GetTypeStatic()
	{
		return FormType::ExecutionHandler;
	}
	int32_t GetType() override
	{
		return FormType::ExecutionHandler;
	}
	inline static bool _registeredFactories = false;
	static void RegisterFactories();

	void Delete(Data*) override;
	/// <summary>
	/// clears all internals and releases shared pointers
	/// DO NOT CALL BEFORE WAITING ON HANDLER!
	/// </summary>
	void Clear() override;

	friend void Records::CreateRecord(ExecutionHandler* value, std::ostream* buffer, size_t& offset, size_t& length);
};
