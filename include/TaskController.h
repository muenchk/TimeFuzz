#pragma once

#include <functional>
#include <queue>
#include <thread>
#include <vector>
#include <condition_variable>

class TaskController
{
public:
	using TaskFn = std::function<void()>;

	class TaskDelegate
	{
	public:
		virtual void Run() = 0;
		virtual void Dispose() = 0;
	};

	static TaskController* GetSingleton();

	void AddTask(TaskFn a_task);
	void AddTask(TaskDelegate* a_task);

	void Start(int numthreads = 0);
	void Stop(bool completeall = true);

	~TaskController();

	bool Busy();
	
private:
	class Task : public TaskDelegate
	{
	public:
		Task(TaskFn&& a_fn);

		void Run() override;
		void Dispose() override;

	private:
		TaskFn _fn;
	};


	void InternalLoop();

	bool terminate = false;
	bool wait = false;
	std::condition_variable condition;
	std::vector<std::thread> threads;
	std::queue<TaskDelegate*> tasks;
	std::mutex lock;
};
