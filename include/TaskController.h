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
	inline void SetDisableLua() { disableLua = true; }

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
	/// shared pointer to session
	/// </summary>
	std::shared_ptr<SessionData> _sessiondata{};

	/// <summary>
	/// disables lua support
	/// </summary>
	bool disableLua = false;

	/// <summary>
	/// whether to terminate the TaskController
	/// </summary>
	bool terminate = false;
	/// <summary>
	/// whether to wait for the completion of all tasks
	/// </summary>
	bool wait = false;
	/// <summary>
	/// freezes thread execution without terminating threads or discarding tasks
	/// </summary>
	bool freeze = false;
	std::condition_variable condition;
	/// <summary>
	/// threads that run InternalLoop
	/// </summary>
	std::vector<std::thread> threads;
	/// <summary>
	/// regular queue, tasks will be handled first come first serve
	/// </summary>
	std::deque<std::shared_ptr<Functions::BaseFunction>> tasks_light;
	std::deque<std::shared_ptr<Functions::BaseFunction>> tasks;
	std::mutex lock;
	std::atomic<uint64_t> completedjobs;
	/// <summary>
	/// current thread status
	/// </summary>
	std::vector<ThreadStatus> status;

	int32_t _numthreads = 0;
	const int32_t classversion = 0x1;
	bool _optimizeFuncExec = false;
};
