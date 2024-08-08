#include "Data.h"

#include <memory>
#include <exception>

Data::Data()
{
	_lresolve = new LoadResolver();
	_lresolve->SetData(this);
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


template<>
std::shared_ptr<Session> Data::CreateForm()
{
	std::shared_ptr<Session> ptr;
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		ptr = dynamic_pointer_cast<Session>(_hashmap.at(StaticFormIDs::Session));
	}
	if (ptr)
		return ptr;
	else {
		ptr = std::make_shared<Session>();
		FormID formid = 0;
		{
			std::lock_guard<std::mutex> guard(_lockformid);
			formid = _nextformid++;
		}
		ptr->SetFormID(formid);
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ formid, dynamic_pointer_cast<Form>(ptr) });
		}
		return ptr;
	}
}

template <>
std::shared_ptr<TaskController> Data::CreateForm()
{
	std::shared_ptr<TaskController> ptr;
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		ptr = dynamic_pointer_cast<TaskController>(_hashmap.at(StaticFormIDs::TaskController));
	}
	if (ptr)
		return ptr;
	else {
		ptr = std::make_shared<TaskController>();
		FormID formid = 0;
		{
			std::lock_guard<std::mutex> guard(_lockformid);
			formid = _nextformid++;
		}
		ptr->SetFormID(formid);
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ formid, dynamic_pointer_cast<Form>(ptr) });
		}
		return ptr;
	}
}

template <>
std::shared_ptr<Settings> Data::CreateForm()
{
	std::shared_ptr<Settings> ptr;
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		ptr = dynamic_pointer_cast<Settings>(_hashmap.at(StaticFormIDs::Settings));
	}
	if (ptr)
		return ptr;
	else {
		ptr = std::make_shared<Settings>();
		FormID formid = 0;
		{
			std::lock_guard<std::mutex> guard(_lockformid);
			formid = _nextformid++;
		}
		ptr->SetFormID(formid);
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ formid, dynamic_pointer_cast<Form>(ptr) });
		}
		return ptr;
	}
}

template <>
std::shared_ptr<Oracle> Data::CreateForm()
{
	std::shared_ptr<Oracle> ptr;
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		ptr = dynamic_pointer_cast<Oracle>(_hashmap.at(StaticFormIDs::Oracle));
	}
	if (ptr)
		return ptr;
	else {
		ptr = std::make_shared<Oracle>();
		FormID formid = 0;
		{
			std::lock_guard<std::mutex> guard(_lockformid);
			formid = _nextformid++;
		}
		ptr->SetFormID(formid);
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ formid, dynamic_pointer_cast<Form>(ptr) });
		}
		return ptr;
	}
}

template <>
std::shared_ptr<Generator> Data::CreateForm()
{
	std::shared_ptr<Generator> ptr;
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		ptr = dynamic_pointer_cast<Generator>(_hashmap.at(StaticFormIDs::Generator));
	}
	if (ptr)
		return ptr;
	else {
		ptr = std::make_shared<Generator>();
		FormID formid = 0;
		{
			std::lock_guard<std::mutex> guard(_lockformid);
			formid = _nextformid++;
		}
		ptr->SetFormID(formid);
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ formid, dynamic_pointer_cast<Form>(ptr) });
		}
		return ptr;
	}
}

template <>
std::shared_ptr<ExclusionTree> Data::CreateForm()
{
	std::shared_ptr<ExclusionTree> ptr;
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		ptr = dynamic_pointer_cast<ExclusionTree>(_hashmap.at(StaticFormIDs::ExclusionTree));
	}
	if (ptr)
		return ptr;
	else {
		ptr = std::make_shared<ExclusionTree>();
		FormID formid = 0;
		{
			std::lock_guard<std::mutex> guard(_lockformid);
			formid = _nextformid++;
		}
		ptr->SetFormID(formid);
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ formid, dynamic_pointer_cast<Form>(ptr) });
		}
		return ptr;
	}
}

template <>
std::shared_ptr<ExecutionHandler> Data::CreateForm()
{
	std::shared_ptr<ExecutionHandler> ptr;
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		ptr = dynamic_pointer_cast<ExecutionHandler>(_hashmap.at(StaticFormIDs::ExecutionHandler));
	}
	if (ptr)
		return ptr;
	else {
		ptr = std::make_shared<ExecutionHandler>();
		FormID formid = 0;
		{
			std::lock_guard<std::mutex> guard(_lockformid);
			formid = _nextformid++;
		}
		ptr->SetFormID(formid);
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ formid, dynamic_pointer_cast<Form>(ptr) });
		}
		return ptr;
	}
}
