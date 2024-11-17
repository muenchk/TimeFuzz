#pragma once

#include <functional>
#include <deque>
#include <thread>
#include <vector>
#include <condition_variable>

#include "Form.h"
#include "Function.h"

class LoadResolver;
class LoadResolverGrammar;
class SessionData;

class TaskController : public Form
{
public:

	enum class ThreadStatus
	{
		Initializing,
		Running,
		Waiting
	};

	/// <summary>
	/// Returns a singleton for the TaskController class
	/// </summary>
	/// <returns></returns>
	static TaskController* GetSingleton();

	/// <summary>
	/// Adds a new task to the execution queue
	/// </summary>
	/// <param name="a_task"></param>
	void AddTask(std::shared_ptr<Functions::BaseFunction> a_task);

	/// <summary>
	/// Starts the TaskController
	/// </summary>
	/// <param name="numthreads"></param>
	void Start(std::shared_ptr<SessionData> session, int32_t numthreads = 0);
	/// <summary>
	/// Stops the TaskController, optionally waiting for the completion of all tasks
	/// </summary>
	/// <param name="completeall"></param>
	void Stop(bool completeall = true);

	~TaskController();

	/// <summary>
	/// Checks whether there are tasks being executed or currently waiting
	/// </summary>
	/// <returns></returns>
	bool Busy();

	/// <summary>
	/// Freezes task execution
	/// </summary>
	void Freeze();
	/// <summary>
	/// Resumes task execution
	/// </summary>
	void Thaw();

	/// <summary>
	/// Disables Lua support
	/// </summary>
	inline void SetDisableLua() { _disableLua = true; }

	/// <summary>
	/// Returns the number of completed jobs
	/// </summary>
	/// <returns></returns>
	uint64_t GetCompletedJobs();
	/// <summary>
	/// Returns the number of waiting jobs
	/// </summary>
	/// <returns></returns>
	int32_t GetWaitingJobs();
	int32_t GetWaitingLightJobs();
	int32_t GetWaitingMediumJobs();

	#pragma region InheritedForm

	size_t GetDynamicSize() override;
	size_t GetStaticSize(int32_t version = 0x1) override;
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);
	int32_t GetType() override
	{
		return FormType::TaskController;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::TaskController;
	}
	void Delete(Data* data) override;
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();

	#pragma endregion
	
private:
	const int32_t classversion = 0x1;

	friend class LoadResolver;
	friend class LoadResolverGrammar;

	/// <summary>
	/// Parallel function that executes the waiting tasks
	/// </summary>
	/// <param name="number"></param>
	void InternalLoop(int32_t number);

	/// <summary>
	/// Parallel function that executes the waiting tasks
	/// </summary>
	/// <param name="number"></param>
	void InternalLoop_Light(int32_t number);

	/// <summary>
	/// Parallel function that only executes the waiting light tasks
	/// </summary>
	/// <param name="number"></param>
	void InternalLoop_LightExclusive(int32_t number);

	/// <summary>
	/// Parallel function that only executes the waiting light tasks
	/// </summary>
	/// <param name="number"></param>
	void InternalLoop_SingleThread(int32_t number);

	/// <summary>
	/// shared pointer to session
	/// </summary>
	std::shared_ptr<SessionData> _sessiondata{};

	/// <summary>
	/// disables lua support
	/// </summary>
	bool _disableLua = false;

	/// <summary>
	/// whether to terminate the TaskController
	/// </summary>
	bool _terminate = false;
	/// <summary>
	/// whether to wait for the completion of all tasks
	/// </summary>
	bool _wait = false;
	/// <summary>
	/// freezes thread execution without terminating threads or discarding tasks
	/// </summary>
	bool _freeze = false;
	std::condition_variable _condition;
	std::condition_variable _condition_light;
	/// <summary>
	/// threads that run InternalLoop
	/// </summary>
	std::vector<std::thread> _threads;
	/// <summary>
	/// light queue, tasks will be handled before medium and regular tasks if available
	/// </summary>
	std::deque<std::shared_ptr<Functions::BaseFunction>> _tasks_light;
	/// <summary>
	/// medium queue, tasks will be handled before regular tasks if available
	/// </summary>
	std::deque<std::shared_ptr<Functions::BaseFunction>> _tasks_medium;
	/// <summary>
	/// regular queue, tasks will be handled first come first serve
	/// </summary>
	std::deque<std::shared_ptr<Functions::BaseFunction>> _tasks;
	/// <summary>
	/// locks access to _tasks_light and _tasks
	/// </summary>
	std::mutex _lock;
	/// <summary>
	/// number of jobs completed in this session/this instance
	/// </summary>
	std::atomic<uint64_t> _completedjobs;
	/// <summary>
	/// current thread status
	/// </summary>
	std::vector<ThreadStatus> _status;
	/// <summary>
	/// number of threads used by this instance
	/// </summary>
	int32_t _numthreads = 0;
	/// <summary>
	/// whether to optimize function execution by allowing light tasks to skip queue
	/// [only available when two or more threads are in use]
	/// </summary>
	bool _optimizeFuncExec = true;
};
