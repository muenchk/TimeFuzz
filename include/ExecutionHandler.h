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

class Settings;
class TaskController;
class Input;
class Oracle;

/* Class Notes
* The class is the overall handler for all gtest execution
* It heavily relies on the usage of function delegates to customize the test execution
* The two main modes are Fragments execution and Whole execution.
*	Fragment execution: executes small fragments and regularly checks whether the fragment
*		has finished executing. If it hasn't, a timeout will be aplied and the test aborted if
*		it has been exceeded. The test will also be aborted once a general tiemout is exceeded
*	Whole Execution: Executes the test in one go, and applies an overall timeout
*/

class Test
{
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

	Test(std::function<void()>&& a_callback, uint64_t id);

	/// <summary>
	/// whether test is currently running
	/// </summary>
	bool running = false;
	/// <summary>
	/// the time of the tests start
	/// </summary>
	std::chrono::steady_clock::time_point starttime;
	/// <summary>
	/// the time of the tests end
	/// </summary>
	std::chrono::steady_clock::time_point endtime;
	/// <summary>
	/// the input for the PUT
	/// </summary>
	std::shared_ptr<Input> input;
	/// <summary>
	/// the iterator the to the next sequence element to be given to executable
	/// </summary>
	std::list<std::string>::iterator itr;
	/// <summary>
	/// last string written to program
	/// </summary>
	std::string lastwritten;
	/// <summary>
	/// time the last input was given to the process
	/// </summary>
	std::chrono::steady_clock::time_point lasttime;
	/// <summary>
	/// reaction time for each fragment [on whole test this contains only one element]
	/// </summary>
	std::list<long> reactiontime;
	/// <summary>
	/// called after the test has been finished
	/// </summary>
	std::function<void()> callback;

	/// <summary>
	/// output of the PUT
	/// </summary>
	std::string output;

	/// <summary>
	/// unique test identifier
	/// </summary>
	uint64_t identifier;

	/// <summary>
	/// reason the process has exited
	/// </summary>
	uint64_t exitreason = ExitReason::Running;

	// process stuff
#if defined(unix) || defined(__unix__) || defined(__unix)
	pid_t processid;
	int red_input[2];
	int red_output[2];
	int exitcode = -1;
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	HANDLE red_input[2];
	HANDLE red_output[2];
#endif

	/// <summary>
	/// Returns whether the test is still running
	/// </summary>
	/// <returns></returns>
	bool IsRunning();

	/// <summary>
	/// Writes input to the stdint of the PUT
	/// </summary>
	/// <param name=""></param>
	void WriteInput(std::string);
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
	long GetMemoryConsumption();
	/// <summary>
	/// terminates the process
	/// </summary>
	/// <returns></returns>
	bool KillProcess();

	/// <summary>
	/// trims input to the last executed
	/// </summary>
	void TrimInput();

	/// <summary>
	/// returns the exitcode
	/// </summary>
	int GetExitCode();

	/// <summary>
	/// invalidates the test [functions cannot be called anymore]
	/// </summary>
	void InValidate();

private:
	bool valid = true;
};

class ExecutionHandler
{

private:
	Settings* _settings;
	std::shared_ptr<TaskController> _threadpool;
	std::shared_ptr<Oracle> _oracle;
	int _maxConcurrentTests = 1;
	int _currentTests = 0;
	std::mutex _lockqueue;
	std::condition_variable _waitforjob;
	std::queue<Test*> _waitingTests;
	std::list<Test*> _runningTests;
	std::mutex _toplevelsync;
	std::condition_variable _waitforhandler;
	bool _active = false;
	/// <summary>
	/// next test identifier
	/// </summary>
	uint64_t _nextid = 1;
	bool cleared = false;

	/// <summary>
	/// time handler waits between cycles in nanoseconds
	/// </summary>
	std::chrono::nanoseconds _waittime = std::chrono::nanoseconds(1000000000);
	long long _waittimeL = 1000000000;

	bool _enableFragments = false;

	bool _stopHandler = false;

	bool _finishtests = false;

	std::thread _thread;

	void InternalLoop();

	std::function<std::string(std::shared_ptr<Input>)> getCMDArgs;

	bool StartTest(Test* test);

	void StopTest(Test* test);

public:
	ExecutionHandler(Settings* settings, std::shared_ptr<TaskController> threadpool, int maxConcurrentTests, std::shared_ptr<Oracle> oracle, std::function<std::string(std::shared_ptr<Input>)>&& getCommandLineArgs);

	/// <summary>
	/// clears all internals and releases shared pointers
	/// DO NOT CALL BEFORE WAITING ON HANDLER!
	/// </summary>
	void Clear();

	/// <summary>
	/// enables execution of test fragments
	/// </summary>
	/// <param name="enable"></param>
	void SetEnableFragments(bool enable = true);

	/// <summary>
	/// Changes the maximum number of tests run concurrently
	/// </summary>
	/// <param name="maxConcurrentTests"></param>
	void SetMaxConcurrentTests(int maxConcurrentTests);

	/// <summary>
	/// starts the handler
	/// </summary>
	void StartHandler();
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

	/*Thoughts on test implementation:
	* What do we need a test function to return:
	*	The element of the input that failed
	*/

	/// <summary>
	/// Adds a new test to the queue [passes of test handling to external application/script]
	/// </summary>
	/// <param name="input">The input to run the test on</param>
	/// <param name="callback">called after the test has been finished</param>
	bool AddTest(std::shared_ptr<Input> input, std::function<void()> callback);

};
