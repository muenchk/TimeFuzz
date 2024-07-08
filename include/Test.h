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
	void TrimInput(int executed = -1);

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
