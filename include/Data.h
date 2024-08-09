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
			Session = 1,
			TaskController = 2,
			Settings = 3,
			Oracle = 4,
			Generator = 5,
			ExclusionTree = 6,
			ExecutionHandler = 7,
		};
	};

private:
	const int32_t saveversion = 0x1;
	/// <summary>
	/// unique name of the save [i.e. "Testing"]
	/// </summary>
	std::string uniquename;
	/// <summary>
	/// save name of the save itself, contains unique name and saveidentifiers
	/// </summary>
	std::string savename;
	std::string extension = ".tfsave";
	std::filesystem::path savepath = std::filesystem::path(".") / "saves";
	/// <summary>
	/// number of the next save
	/// </summary>
	int32_t savenumber = 1;
	LoadResolver* _lresolve;

	const FormID _baseformid = 100;
	FormID _nextformid = _baseformid;
	std::mutex _lockformid;
	std::unordered_map<FormID, std::shared_ptr<Form>> _hashmap;
	std::shared_mutex _hashmaplock;

	friend LoadResolver;

	std::string GetSaveName();

	/// <summary>
	/// Loads a savefile, where [name] is the fully qualified filename
	/// </summary>
	/// <param name="name"></param>
	void LoadIntern(std::filesystem::path path);

public:

	bool _globalTasks = false;
	bool _globalExec = false;
	bool _loaded = false;

	Data();

	static Data* GetSingleton();

	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	std::shared_ptr<T> CreateForm()
	{
		loginfo("Create Form");
		std::shared_ptr<T> ptr = std::make_shared<T>();
		FormID formid = 0;
		{
			std::unique_lock<std::mutex> guard(_lockformid);
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

	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	void DeleteForm(std::shared_ptr<T> form)
	{
		if (form)
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.erase(form->GetFormID());
			form.reset();
			return;
		} else {
			form.reset();
			return;
		}
	}

	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	std::shared_ptr<T> LookupFormID(FormID formid)
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		auto itr = _hashmap.find(formid);
		if (itr != _hashmap.end())
			return dynamic_pointer_cast<T>(itr->second);
		return {};
	}

	/// <summary>
	/// saves the current state of the program to a savefile
	/// </summary>
	void Save();

	/// <summary>
	/// Loads the last savefile with the uniquename [name]
	/// </summary>
	/// <param name="name"></param>
	void Load(std::string name);
	
	/// <summary>
	/// Loads the [number]-th savefile with the uniquename [name]
	/// </summary>
	/// <param name="name"></param>
	/// <param name="number"></param>
	void Load(std::string name, int32_t number);

};

class LoadResolver
{
public:
	using TaskFn = std::function<void()>;

	class TaskDelegate
	{
	public:
		virtual void Run() = 0;
		virtual void Dispose() = 0;
	};

	static LoadResolver* GetSingleton();
	~LoadResolver();
	void SetData(Data* dat);
	Data* data = nullptr;

	void AddTask(TaskFn a_task);
	void AddTask(TaskDelegate* a_task);

	template <class T>
	std::shared_ptr<T> ResolveFormID(FormID formid)
	{
		auto ptr = data->_hashmap.at(formid);
		if (ptr)
			return dynamic_pointer_cast<T>(ptr);
		return {};
	}

	void Resolve();

private:
	std::queue<TaskDelegate*> tasks;
	std::mutex lock;

	class Task : public TaskDelegate
	{
	public:
		Task(TaskFn&& a_fn);

		void Run() override;
		void Dispose() override;

	private:
		TaskFn _fn;
	};
};
