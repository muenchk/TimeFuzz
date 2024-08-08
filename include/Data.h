#pragma once

#include <string>
#include <queue>
#include <unordered_map>
#include <mutex>

#include "TaskController.h"
#include "Form.h"
#include "Session.h"
#include "Settings.h"
#include "Oracle.h"
#include "Generator.h"
#include "ExclusionTree.h"
#include "ExecutionHandler.h"
#include "Logging.h"

class LoadResolver;

class Data
{
	struct StaticFormIDs
	{
		enum StaticFormID
		{
			Session = 0,
			TaskController = 1,
			Settings = 2,
			Oracle = 3,
			Generator = 4,
			ExclusionTree = 5,
			ExecutionHandler = 6,
		};
	};

private:
	std::string uniquename;
	std::string savename;
	int32_t savenumber;
	LoadResolver* _lresolve;

	const FormID _baseformid = 100;
	FormID _nextformid = _baseformid;
	std::mutex _lockformid;
	std::unordered_map<FormID, std::shared_ptr<Form>> _hashmap;
	std::shared_mutex _hashmaplock;

	friend LoadResolver;

public:

	bool _globalTasks = false;
	bool _globalExec = false;

	Data();

	static Data* GetSingleton();

	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	std::shared_ptr<T> CreateForm()
	{
		loginfo("Create Form");
		std::shared_ptr<T> ptr = std::make_shared<T>();
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
	template<>
	std::shared_ptr<Session> CreateForm();
	template <>
	std::shared_ptr<TaskController> CreateForm();
	template <>
	std::shared_ptr<Settings> CreateForm();
	template <>
	std::shared_ptr<Oracle> CreateForm();
	template <>
	std::shared_ptr<Generator> CreateForm();
	template <>
	std::shared_ptr<ExclusionTree> CreateForm();
	template <>
	std::shared_ptr<ExecutionHandler> CreateForm();
	

	void Save();

	void Load();

};

class LoadResolver
{
	std::queue<TaskController::TaskDelegate*> tasks;
	std::mutex lock;

public:
	static LoadResolver* GetSingleton();
	~LoadResolver();
	void SetData(Data* dat);
	Data* data = nullptr;

	void AddTask(TaskController::TaskFn a_task);
	void AddTask(TaskController::TaskDelegate* a_task);

	template <class T>
	std::shared_ptr<T> ResolveFormID(FormID formid)
	{
		auto ptr = data->_hashmap.at(formid);
		if (ptr)
			return dynamic_pointer_cast<T>(ptr);
		return {};
	}

	void Resolve();
};
