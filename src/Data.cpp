#include "Data.h"

#include <memory>

Data::Data()
{
	lresolve = new LoadResolver();
	lresolve->SetData(this);
}

Data* Data::GetSingleton()
{
	static Data data;
	return std::addressof(data);
}


LoadResolver* LoadResolver::GetSingleton()
{
	static LoadResolver resolver;
	return std::addressof(resolver);
}

LoadResolver::~LoadResolver()
{
	data = nullptr;
	while (!tasks.empty())
	{
		delete tasks.front();
		tasks.pop();
	}
}

void LoadResolver::AddTask(TaskController::TaskFn a_task)
{
	AddTask(new Task(std::move(a_task)));
}

void LoadResolver::AddTask(TaskController::TaskDelegate* a_task)
{
	{
		std::unique_lock<std::mutex> guard(lock);
		tasks.push(a_task);
	}
}

void LoadResolver::SetData(Data* dat)
{
	data = dat;
}
