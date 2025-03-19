#pragma once

#include <functional>
#include <deque>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "Form.h"
#include "Function.h"
#include "Logging.h"

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

private:
	const int32_t classversion = 0x2;

	friend class LoadResolver;
	friend class LoadResolverGrammar;

	/// <summary>
	/// Parallel function that executes the waiting tasks
	/// </summary>
	/// <param name="number"></param>
	void InternalLoop_Heavy(int32_t number);

	/// <summary>
	/// Parallel function that executes the waiting tasks
	/// </summary>
	/// <param name="number"></param>
	void InternalLoop_Medium(int32_t number);

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
	std::condition_variable _condition_medium;
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
	/// locks access to _tasks
	/// </summary>
	std::mutex _lock;
	/// <summary>
	/// locks access to _tasks_light
	/// </summary>
	std::mutex _lockLight;
	/// <summary>
	/// locks access to _tasks_medium
	/// </summary>
	std::mutex _lockMedium;
	/// <summary>
	/// number of jobs completed in this session/this instance
	/// </summary>
	std::atomic<uint64_t> _completedjobs;
	/// <summary>
	/// current thread status
	/// </summary>
	std::vector<ThreadStatus> _status;
	/// <summary>
	/// current thread working begin
	/// </summary>
	std::vector<std::chrono::steady_clock::time_point> _statusTime;
	/// <summary>
	/// current task handled by threads
	/// </summary>
	std::vector<const char*> _statusTask;
	/// <summary>
	/// number of threads used by this instance
	/// </summary>
	int32_t _numthreads = 0;
	int32_t _numLightThreads = 0;
	int32_t _numMediumThreads = 0;
	int32_t _numHeavyThreads = 0;
	int32_t _numAllThreads = 0;
	/// <summary>
	/// map holding the number of individual executed tasks
	/// </summary>
	std::unordered_map<std::string, int64_t> _executedTasks;

	std::mutex _executedTasksLock;

	bool _controlEnableFine = true;
	bool _controlEnableLight = true;
	bool _controlEnableMedium = true;
	bool _controlEnableHeavy = true;

public:

	/// <summary>
	/// Returns a singleton for the TaskController class
	/// </summary>
	/// <returns></returns>
	static TaskController* GetSingleton();

	/// <summary>
	/// Adds a new task to the execution queue
	/// </summary>
	/// <param name="a_task"></param>
	template <class T, typename = std::enable_if<std::is_base_of<Functions::BaseFunction, T>::value>>
	void AddTask(std::shared_ptr<T> a_task, bool bypass = false)
	{
		if (a_task) {
			if (_controlEnableFine) {
				Functions::FunctionType type = a_task->GetFunctionType();
				switch (type) {
				case Functions::FunctionType::Light:
					if (_controlEnableLight) {
						std::unique_lock<std::mutex> guard(_lockLight);
						if (!bypass) [[likely]]
							_tasks_light.push_back(a_task);
						else
							_tasks_light.push_front(a_task);
						_condition_light.notify_one();
						break;
					}
				case Functions::FunctionType::Medium:
					if (_controlEnableMedium) {
						std::unique_lock<std::mutex> guard(_lockMedium);
						if (!bypass) [[likely]]
							_tasks_medium.push_back(a_task);
						else
							_tasks_medium.push_front(a_task);
						_condition_medium.notify_one();
						break;
					}
				case Functions::FunctionType::Heavy:
					{
						std::unique_lock<std::mutex> guard(_lock);
						if (!bypass) [[likely]]
							_tasks.push_back(a_task);
						else
							_tasks.push_front(a_task);
						_condition.notify_one();
					}
					break;
				};
			}
			else {
				Functions::FunctionType type = a_task->GetFunctionType();
				switch (type) {
				case Functions::FunctionType::Light:
					if (_controlEnableLight) {
						std::unique_lock<std::mutex> guard(_lock);
						if (!bypass) [[likely]]
							_tasks_light.push_back(a_task);
						else
							_tasks_light.push_front(a_task);
						_condition.notify_one();
						break;
					}
				case Functions::FunctionType::Medium:
					if (_controlEnableMedium) {
						std::unique_lock<std::mutex> guard(_lock);
						if (!bypass) [[likely]]
							_tasks_medium.push_back(a_task);
						else
							_tasks_medium.push_front(a_task);
						_condition.notify_one();
						break;
					}
				case Functions::FunctionType::Heavy:
					{
						std::unique_lock<std::mutex> guard(_lock);
						if (!bypass) [[likely]]
							_tasks.push_back(a_task);
						else
							_tasks.push_front(a_task);
						_condition.notify_one();
					}
					break;
				};
			}
			std::unique_lock<std::mutex> execLock(_executedTasksLock);
			int32_t type = (int32_t)a_task->GetType();
			std::string name = std::string(a_task->GetName()/*typeid(T).name()) + " | " + std::string((char*)&type* 4*/);
			auto itr = _executedTasks.find(name);
			if (itr != _executedTasks.end())
				_executedTasks.insert_or_assign(name, itr->second + 1);
			else
				_executedTasks.insert_or_assign(name, 1);
		} else {
			logcritical("Tried to add nullptr as task to taskcontroller");
		}
	}

	int32_t GetNumThreads()
	{
		return _numthreads;
	}

	void LockExecutedTasks()
	{
		_executedTasksLock.lock();
	}

	void UnlockExecutedTasks()
	{
		_executedTasksLock.unlock();
	}

	auto BeginExecutedTasks()
	{
		return _executedTasks.begin();
	}

	auto EndExecutedTasks()
	{
		return _executedTasks.end();
	}

	/// <summary>
	/// Starts the TaskController
	/// </summary>
	/// <param name="numthreads"></param>
	void Start(std::shared_ptr<SessionData> session, int32_t numthreads = 0);
	/// <summary>
	/// Starts the TaskController
	/// </summary>
	/// <param name="session"></param>
	/// <param name="numthreads"></param>
	void Start(std::shared_ptr<SessionData> session, int32_t numLightThreads, int32_t numMediumThreads, int32_t numHeavyThreads, int32_t numAllThreads);
	/// <summary>
	/// Stops the TaskController, optionally waiting for the completion of all tasks
	/// </summary>
	/// <param name="completeall"></param>
	void Stop(bool completeall = true);

	/// <summary>
	/// returns the number of threads handling heavy tasks
	/// </summary>
	/// <returns></returns>
	int32_t GetHeavyThreadCount();
	/// <summary>
	/// returns the number of threads handling medium tasks
	/// </summary>
	/// <returns></returns>
	int32_t GetMediumThreadCount();
	/// <summary>
	/// returns the number of threads handling light tasks
	/// </summary>
	/// <returns></returns>
	int32_t GetLightThreadCount();

	~TaskController();

	/// <summary>
	/// Checks whether there are tasks being executed or currently waiting
	/// </summary>
	/// <returns></returns>
	bool Busy();

	/// <summary>
	/// instead of waiting for the controller to freeze, the function returns after notifying the threads of the freeze
	/// </summary>
	void RequestFreeze();
	/// <summary>
	/// Freezes task execution
	/// </summary>
	void Freeze();
	/// <summary>
	/// Resumes task execution
	/// </summary>
	void Thaw();

	/// <summary>
	/// Clears all waiting tasks
	/// </summary>
	void ClearTasks();

	/// <summary>
	/// returns whether the controller is currently frozen
	/// </summary>
	/// <returns></returns>
	bool IsFrozen();

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

	void GetThreadStatus(std::vector<ThreadStatus>& status, std::vector<const char*>& names, std::vector<std::string>& time);

	#pragma region InheritedForm

	size_t GetDynamicSize() override;
	size_t GetStaticSize(int32_t version) override;
	bool WriteData(std::ostream* buffer, size_t& offset) override;
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver);
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
	size_t MemorySize() override;

	#pragma endregion
	
};
