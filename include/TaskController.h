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

class TaskController : public Form
{
public:

	enum class ThreadStatus
	{
		Initializing,
		Running,
		Waiting
	};

	static TaskController* GetSingleton();

	/// <summary>
	/// Adds a new task to the execution queue
	/// </summary>
	/// <param name="a_task"></param>
	void AddTask(Functions::BaseFunction* a_task);

	/// <summary>
	/// Starts the TaskController
	/// </summary>
	/// <param name="numthreads"></param>
	void Start(int32_t numthreads = 0);
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

	friend class LoadResolver;
	friend class LoadResolverGrammar;


	void InternalLoop(int32_t number);

	bool terminate = false;
	bool wait = false;
	/// <summary>
	/// freezes thread execution without terminating threads or discarding tasks
	/// </summary>
	bool freeze = false;
	std::condition_variable condition;
	std::vector<std::thread> threads;
	std::deque<Functions::BaseFunction*> tasks;
	std::mutex lock;
	std::vector<ThreadStatus> status;

	int32_t _numthreads = 0;
	const int32_t classversion = 0x1;
};
