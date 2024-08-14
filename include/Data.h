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
	const uint64_t guid1 = 0xe30db97c4f1e478f;
	const uint64_t guid2 = 0x8b03f3d9e946dcf3;
	const int32_t saveversion = 0x1;
	/// <summary>
	/// unique name of the save [i.e. "Testing"]
	/// </summary>
	std::string uniquename;
	/// <summary>
	/// save name of the save itself, contains unique name and saveidentifiers
	/// </summary>
	std::string savename;
	/// <summary>
	/// file extension used by savefiles
	/// </summary>
	std::string extension = ".tfsave";
	/// <summary>
	/// path to the save files
	/// </summary>
	std::filesystem::path savepath = std::filesystem::path(".") / "saves";
	/// <summary>
	/// number of the next save
	/// </summary>
	int32_t savenumber = 1;
	/// <summary>
	/// load resolver, used to resolve forms after loading has been completed
	/// </summary>
	LoadResolver* _lresolve;

	/// <summary>
	/// base form id for dynamic forms
	/// </summary>
	const FormID _baseformid = 100;
	/// <summary>
	/// id of the next form
	/// </summary>
	FormID _nextformid = _baseformid;
	/// <summary>
	/// mutex for _nextformid access
	/// </summary>
	std::mutex _lockformid;
	/// <summary>
	/// hashmap holding all forms
	/// </summary>
	std::unordered_map<FormID, std::shared_ptr<Form>> _hashmap;
	/// <summary>
	/// mutex for _hashmap access
	/// </summary>
	std::shared_mutex _hashmaplock;

	friend LoadResolver;

	/// <summary>
	/// returns the full name of the next save
	/// </summary>
	/// <returns></returns>
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

	/// <summary>
	/// Returns a singleton for the Data class
	/// </summary>
	/// <returns></returns>
	static Data* GetSingleton();

	/// <summary>
	/// Creates a new dynamic form and registers it
	/// </summary>
	/// <typeparam name="T">Type of the form to create</typeparam>
	/// <returns></returns>
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
	template <>
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

	/// <summary>
	/// Registers an existing form [used during loading]
	/// </summary>
	/// <typeparam name="T">Type of the form to register</typeparam>
	/// <param name="form">The form to register</param>
	/// <returns></returns>
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	bool RegisterForm(std::shared_ptr<T> form)
	{
		if (form)
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ form->GetFormID(), dynamic_pointer_cast<Form>(form) });
			return true;
		}
		return false;
	}

	/// <summary>
	/// Unregisters/deletes a registered form
	/// </summary>
	/// <typeparam name="T">Type of the form to delete</typeparam>
	/// <param name="form">The form to delete</param>
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

	/// <summary>
	/// Looks up a form by its id
	/// </summary>
	/// <typeparam name="T">Type of the form to find</typeparam>
	/// <param name="formid">The id of the form to find</param>
	/// <returns></returns>
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
	/// Returns a copy of the hashmap with weak pointers instead of the shared pointers
	/// </summary>
	/// <returns></returns>
	std::unordered_map<FormID, std::weak_ptr<Form>> GetWeakHashmapCopy();

	/// <summary>
	/// clears all forms
	/// </summary>
	void Clear();

	/// <summary>
	/// saves the current state of the program to a savefile
	/// </summary>
	void Save();

	/// <summary>
	/// sets the unique name of the saves
	/// </summary>
	/// <param name=""></param>
	void SetSaveName(std::string name);

	/// <summary>
	/// Sets the path for savefiles
	/// </summary>
	/// <param name="path"></param>
	void SetSavePath(std::filesystem::path path);

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
		auto itr = data->_hashmap.find(formid);
		if (itr != data->_hashmap.end()) {
			if (itr->second)
				return dynamic_pointer_cast<T>(itr->second);
		}
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
