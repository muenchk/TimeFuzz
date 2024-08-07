#include "Data.h"

#include <memory>
#include <exception>

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

void Data::Save()
{
	// create new file on disc

	// write main information about savefile: name, savenumber, nextformid, etc.

	// write session data
	throw std::runtime_error("Data::Save is not yet implemented");
}

void Data::Load()
{
	throw std::runtime_error("Data::Load is not yet implemented");
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
		tasks.front()->Dispose();
		tasks.pop();
	}
}

void LoadResolver::AddTask(TaskController::TaskFn a_task)
{
	AddTask(new TaskController::Task(std::move(a_task)));
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

void LoadResolver::Resolve()
{
	while (!tasks.empty()) {
		TaskController::TaskDelegate* del;
		del = tasks.front();
		tasks.pop();
		del->Run();
		del->Dispose();
	}
}
