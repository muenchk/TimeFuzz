#include <atomic>
#include <queue>
#include <exception>

#include "TaskController.h"
#include "Threading.h"


void TaskController::AddTask(TaskFn a_task)
{
	AddTask(new Task(std::move(a_task)));
}

void TaskController::AddTask(TaskDelegate* a_task)
{
	{
		std::unique_lock<std::mutex> guard(lock);
		tasks.push(a_task);
	}
	condition.notify_one();
}

void TaskController::Start(int numthreads)
{
	if (numthreads == 0)
		throw std::runtime_error("Cannot start a TaskController with 0 threads.");
	for (int i = 0; i < numthreads; i++)
		threads.emplace_back(std::thread(&TaskController::InternalLoop, this));
}

void TaskController::Stop(bool completeall)
{
	{
		std::unique_lock<std::mutex> guard(lock);
		terminate = true;
		wait = completeall;
	}
	condition.notify_all();
	for (std::thread& t : threads)
		t.join();
	threads.clear();
	while (tasks.empty() == false)
	{
		tasks.front()->Dispose();
		tasks.pop();
	}
}

TaskController::~TaskController()
{
	if (terminate == false)
	{
		// we are exiting unexpectedly
		// set terminate and detach the threads so we do not have to wait on them
		terminate = true;
		for (auto& thread : threads)
			thread.detach();
		while (tasks.empty() == false) {
			tasks.front()->Dispose();
			tasks.pop();
		}
	}
}

bool TaskController::Busy()
{
	std::unique_lock<std::mutex> guard(lock);
	return !tasks.empty();
}

void TaskController::InternalLoop()
{
	while (true)
	{
		TaskDelegate* del;
		{
			std::unique_lock<std::mutex> guard(lock);
			condition.wait(guard, [this] { return !tasks.empty() || terminate && wait == false || terminate && tasks.empty(); });
			if (terminate && wait == false || terminate && tasks.empty())
				return;
			del = tasks.front();
			tasks.pop();
		}
		del->Run();
		del->Dispose();
	}
}

TaskController::Task::Task(TaskFn&& a_fn) :
	_fn(std::move(a_fn))
{}

void TaskController::Task::Run()
{
	_fn();
}

void TaskController::Task::Dispose()
{
	delete this;
}
