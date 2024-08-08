#pragma once

#include <functional>
#include <queue>
#include <thread>
#include <vector>
#include <condition_variable>

#include "Form.h"

class LoadResolver;
class LoadResolverGrammar;

class TaskController : public Form
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

	void Start(int32_t numthreads = 0);
	void Stop(bool completeall = true);

	~TaskController();

	bool Busy();

	void Freeze();

	void Unfreeze();

	#pragma region InheritedForm

	size_t GetDynamicSize() override;
	size_t GetStaticSize(int32_t version = 0x1) override;
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);
	static int32_t GetType()
	{
		return FormType::TaskController;
	}
	void Delete(Data* data);

	#pragma endregion
	
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

	friend class LoadResolver;
	friend class LoadResolverGrammar;


	void InternalLoop();

	bool terminate = false;
	bool wait = false;
	std::condition_variable condition;
	std::vector<std::thread> threads;
	std::queue<TaskDelegate*> tasks;
	std::mutex lock;

	int32_t _numthreads = 0;
	const int32_t classversion = 0x1;
};
