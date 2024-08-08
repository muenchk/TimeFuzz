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

#include "Test.h"
#include "Form.h"

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
*		it has been exceeded. The test will also be aborted once a general tiemout is exceeded
*	Whole Execution: Executes the test in one go, and applies an overall timeout
*/

class ExecutionHandler: public Form
{

private:
	std::shared_ptr<Session> _session;
	std::shared_ptr<Settings> _settings;
	std::shared_ptr<TaskController> _threadpool;
	std::shared_ptr<Oracle> _oracle;
	int32_t _maxConcurrentTests = 1;
	int32_t _currentTests = 0;
	std::mutex _lockqueue;
	std::condition_variable _waitforjob;
	std::queue<std::weak_ptr<Test>> _waitingTests;
	std::list<std::weak_ptr<Test>> _runningTests;
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
	std::chrono::nanoseconds _waittime = std::chrono::nanoseconds(100000000);
	int64_t _waittimeL = 100000000;

	bool _enableFragments = false;

	bool _stopHandler = false;

	bool _finishtests = false;

	std::thread _thread;

	void InternalLoop();

	std::function<std::string(std::shared_ptr<Input>)> getCMDArgs;

	bool StartTest(std::shared_ptr<Test> test);

	void StopTest(std::shared_ptr<Test> test);

public:
	ExecutionHandler();

	void Init(std::shared_ptr<Session> session, std::shared_ptr<Settings> settings, std::shared_ptr<TaskController> threadpool, int32_t maxConcurrentTests, std::shared_ptr<Oracle> oracle, std::function<std::string(std::shared_ptr<Input>)>&& getCommandLineArgs);

	static ExecutionHandler* GetSingleton();

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
	void SetMaxConcurrentTests(int32_t maxConcurrentTests);

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

	void Delete(Data*) {}
};
