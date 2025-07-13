#pragma once

#if defined(unix) || defined(__unix__) || defined(__unix)
#include <linux/types.h>
#include <asm/types.h>
#endif

#include <sys/types.h>

#include <string>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <chrono>
#include <boost/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/unordered_map.hpp>
#include <algorithm>
#include <functional>

#include "TaskController.h"
#include "Form.h"
#include "Session.h"
#include "Settings.h"
#include "Oracle.h"
#include "Generator.h"
#include "ExclusionTree.h"
#include "ExecutionHandler.h"
#include "Logging.h"
#include "Utility.h"

class LoadResolver;

class Data
{
public:
	struct LoadSaveArgs
	{
		bool skipExlusionTree = false;
	};

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
			SessionData = 8,
		};
	};

	enum class VisitAction
	{
		None,
		DeleteForm,
	};

	struct SaveStats
	{
		int64_t _Input = 0;
		int64_t _Grammar = 0;
		int64_t _DevTree = 0;
		int64_t _ExclTree = 0;
		int64_t _ExclTreeNode = 0;
		int64_t _Generator = 0;
		int64_t _Session = 0;
		int64_t _Settings = 0;
		int64_t _Test = 0;
		int64_t _TaskController = 0;
		int64_t _ExecutionHandler = 0;
		int64_t _Oracle = 0;
		int64_t _SessionData = 0;
		int64_t _Fail = 0;
		int64_t _DeltaController = 0;
		int64_t _Generation = 0;
	};

private:
	struct ObjStorage
	{
	private:
		std::queue<std::shared_ptr<Form>> storage;
		std::atomic_flag _flag = ATOMIC_FLAG_INIT;

	public:
		std::shared_ptr<Form> GetObj()
		{
			Utility::SpinLock spin(_flag);
			if (storage.size() > 0) {
				auto ret = storage.front();
				storage.pop();
				return ret;
			}
			return {};
		}
		void StoreObj(std::shared_ptr<Form> obj)
		{
			Utility::SpinLock spin(_flag);
			storage.push(obj);
		}
	};

	const uint64_t guid1 = 0xe30db97c4f1e478f;
	const uint64_t guid2 = 0x8b03f3d9e946dcf3;
	const int32_t saveversion = 0x3;
	/// <summary>
	/// unique name of the save [i.e. "Testing"]
	/// </summary>
	std::string _uniquename;
	/// <summary>
	/// save name of the save itself, contains unique name and saveidentifiers
	/// </summary>
	std::string _savename;
	/// <summary>
	/// file extension used by savefiles
	/// </summary>
	std::string _extension = ".tfsave";
	/// <summary>
	/// path to the save files
	/// </summary>
	std::filesystem::path _savepath = std::filesystem::path(".") / "saves";
	/// <summary>
	/// Stores all available save files with the given unique name
	/// </summary>
	std::map<int32_t, std::filesystem::path> _saves;
	std::vector<int32_t> _priorsaves;
	/// <summary>
	/// number of the next save
	/// </summary>
	int32_t _savenumber = 1;
	/// <summary>
	/// number of the currently loaded save
	/// </summary>
	int32_t _loadedsavenumber = 0;
	/// <summary>
	/// lock restricting access to save routine
	/// </summary>
	std::mutex _savelock;
	/// <summary>
	/// start time of this session
	/// </summary>
	std::chrono::steady_clock::time_point _sessionBegin = std::chrono::steady_clock::now();
	/// <summary>
	/// Overall session runtime
	/// </summary>
	std::chrono::nanoseconds _runtime = std::chrono::nanoseconds(0);
	/// <summary>
	/// whether session has ended
	/// </summary>
	bool _endedSession = false;

public:
	/// <summary>
	/// load resolver, used to resolve forms after loading has been completed
	/// </summary>
	LoadResolver* _lresolve;

private:
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
	//std::unordered_map<FormID, std::shared_ptr<Form>> _hashmap;
	boost::unordered_map<FormID, std::shared_ptr<Form>> _hashmap;
	/// <summary>
	/// mutex for _hashmap access
	/// </summary>
	std::shared_mutex _hashmaplock;

	/// <summary>
	/// id of the next string in the registry
	/// </summary>
	FormID _stringNextFormID = 100;
	/// <summary>
	/// Type definition of BiMap for ease-of-access
	/// </summary>
	typedef boost::bimap<boost::bimaps::unordered_set_of<FormID>, boost::bimaps::unordered_set_of<std::string>> StringHashmap;
	/// <summary>
	/// string registry
	/// </summary>
	StringHashmap _stringHashmap;
	/// <summary>
	/// mutex for _stringHashmap access
	/// </summary>
	std::mutex _stringHashmapLock;

	/// <summary>
	/// holds references to reusable objects
	/// </summary>
	std::unordered_map<int32_t, ObjStorage*> _objectRecycler;

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
	void LoadIntern(std::filesystem::path path, LoadSaveArgs& loadArgs, bool ignorepriorsaves = false);

	void RegisterForms();

	size_t GetStringHashmapSize();

	bool WriteStringHashmap(std::ostream* buffer, size_t& offset, size_t length);

	bool ReadStringHashmap(std::istream* buffer, size_t& offset, size_t length);

public:
	bool _globalTasks = false;
	bool _globalExec = false;
	bool _loaded = false;

	Data();

	std::string _status;
	int32_t _record;
	bool _actionloadsave = false;
	uint64_t _actionloadsave_max = 0;
	uint64_t _actionloadsave_current = 0;
	size_t _actionrecord_len = 0;
	size_t _actionrecord_offset = 0;

	/// <summary>
	/// Returns a singleton for the Data class
	/// </summary>
	/// <returns></returns>
	static Data* GetSingleton();

	/// <summary>
	/// Sets the session start time to the current time
	/// </summary>
	void StartClock();
	/// <summary>
	/// stops runtime clock
	/// </summary>
	void EndClock()
	{
		_endedSession = true;
		_runtime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - _sessionBegin);
	}
	void StopClock()
	{
		_runtime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - _sessionBegin);
	}
	/// <summary>
	/// Returns the overall session runtime
	/// </summary>
	/// <returns></returns>
	std::chrono::nanoseconds GetRuntime()
	{
		if (!_endedSession)
			return _runtime + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - _sessionBegin);
		else
			return _runtime;
	}

	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	[[deprecated]] void RecycleObject(std::shared_ptr<T> form)
	{
		if (form) {
			auto stor = _objectRecycler.at(form->GetType());
			stor->StoreObj(form);
		}
	}
	/// <summary>
	/// [[deprecated]] returns a recycled object
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <typeparam name=""></typeparam>
	/// <returns>valid shared_ptr</returns>
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	[[deprecated]] std::shared_ptr<T> GetRecycledObject()
	{
		auto stor = _objectRecycler.at(T::GetTypeStatic());
		return dynamic_pointer_cast<T>(stor->GetObj());
	}

	/// <summary>
	/// Creates a new dynamic form and registers it
	/// </summary>
	/// <typeparam name="T">Type of the form to create</typeparam>
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	std::shared_ptr<T> CreateForm()
	{
		//loginfo("Create Form");
		//std::shared_ptr<T> ptr = GetRecycledObject<T>();
		//if (!ptr)
		//	ptr = std::make_shared<T>();
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
		ptr->SetChanged();
		return ptr;
	}

	/// <summary>
	/// Registers an existing form [used during loading]
	/// </summary>
	/// <typeparam name="T">Type of the form to register</typeparam>
	/// <param name="form">The form to register</param>
	/// <returns>whether the form was registered successfully, false otherwise</returns>
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	bool RegisterForm(std::shared_ptr<T> form)
	{
		if (form) {
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			//if (_hashmap.contains(form->GetFormID()) && _hashmap.at(form->GetFormID())->GetType() != form->GetType()) {
			//	logcritical("Already assigned formid for form of different type");
			//}
			_hashmap.insert(std::pair<FormID, std::shared_ptr<Form>>{ form->GetFormID(), dynamic_pointer_cast<Form>(form) });
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
		if (form) {
			if (form->CanDelete(this) == true) {
				std::unique_lock<std::shared_mutex> guard(_hashmaplock);
				form->SetFlag(Form::FormFlags::Deleted);
				// if form was in an earlier save file we don't delete it from the hashmap
				// such that we can save to a newer savefile that it was deleted
				if (form->WasSaved() == false)
					_hashmap.erase(form->GetFormID());
				//form->Clear();
				//RecycleObject<T>(form);
				form.reset();
				return;
			}
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
	/// <returns>valid shared_ptr if formid has been found, invalid shared_ptr otherwise</returns>
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	std::shared_ptr<T> LookupFormID(FormID formid)
	{
		std::shared_lock<std::shared_mutex> guard(_hashmaplock);
		auto itr = _hashmap.find(formid);
		if (itr != _hashmap.end() && itr->second->HasFlag(Form::FormFlags::Deleted) == false) {
			return dynamic_pointer_cast<T>(itr->second);
		}
		return {};
	}

	/// <summary>
	/// Returns a vector with all database entries that match the given type
	/// </summary>
	/// <typeparam name="T">type to find</typeparam>
	/// <returns>vector with entries of type T</returns>
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	std::vector<std::shared_ptr<T>> GetFormArray()
	{
		std::vector<std::shared_ptr<T>> results;
		std::shared_lock<std::shared_mutex> guard(_hashmaplock);
		for (auto& [_, form] : _hashmap) {
			if (form->GetType() == T::GetTypeStatic() && form->HasFlag(Form::FormFlags::Deleted) == false)
				results.push_back(dynamic_pointer_cast<T>(form));
		}
		return results;
	}

	/// <summary>
	/// Counts how many objects of the same type existed before
	/// </summary>
	/// <typeparam name="T">type to find</typeparam>
	/// <returns>vector with entries of type T</returns>
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	int64_t CountOlderObjects(std::shared_ptr<T> compare)
	{
		std::vector<std::shared_ptr<T>> results;
		std::shared_lock<std::shared_mutex> guard(_hashmaplock);
		FormID formid = compare->GetFormID();
		int64_t count = 0;
		for (auto& [_, form] : _hashmap) {
			if (form->GetType() == T::GetTypeStatic() && form->GetFormID() < formid)
				count++;
		}
		return count;
	}

	/// <summary>
	/// returns the size of the internal hashmap
	/// </summary>
	/// <returns>size of the internal hashmap</returns>
	size_t GetHashmapSize();

	/// <summary>
	/// Applies the [visitor] function to all forms in the hashmap
	/// </summary>
	/// <param name="visitor">predicate that is applied to all database entries</param>
	void Visit(std::function<VisitAction(std::shared_ptr<Form>)> visitor);

	/// <summary>
	/// Returns a copy of the hashmap with weak pointers instead of the shared pointers
	/// </summary>
	/// <returns>hashmap with weak copies of database entries</returns>
	std::unordered_map<FormID, std::weak_ptr<Form>> GetWeakHashmapCopy();

	/// <summary>
	/// Returns the ID associated with the given string
	/// </summary>
	/// <param name="str">string to find / generate id for</param>
	/// <returns>id of the string</returns>
	FormID GetIDFromString(std::string str);

	/// <summary>
	/// Returns the string associated with the given ID, and a boolean that indicates whether the value exists
	/// </summary>
	/// <param name="id">ID of the string to find</param>
	/// <returns>returns whether a string was found and if so, the string</returns>
	std::pair<std::string, bool> GetStringFromID(FormID id);

	/// <summary>
	/// clears all forms
	/// </summary>
	void Clear();

	/// <summary>
	/// saves the current state of the program to a savefile and calls a callback afterwards
	/// </summary>
	/// <param name="callback">callback to execute after save</param>
	void Save(std::shared_ptr<Functions::BaseFunction> callback);

	/// <summary>
	/// sets the unique name of the saves
	/// </summary>
	/// <param name="">name of the save</param>
	void SetSaveName(std::string name);

	/// <summary>
	/// Sets the path for savefiles
	/// </summary>
	/// <param name="path">path to save files</param>
	void SetSavePath(std::filesystem::path path);

	/// <summary>
	/// Loads the last savefile with the uniquename [name]
	/// </summary>
	/// <param name="name">name of the save to load</param>
	void Load(std::string name, LoadSaveArgs& loadArgs);

	/// <summary>
	/// Loads the [number]-th savefile with the uniquename [name]
	/// </summary>
	/// <param name="name">name of the save to load</param>
	/// <param name="number">the number of the save to load</param>
	void Load(std::string name, int32_t number, LoadSaveArgs& loadArgs);

	
	/// <summary>
	/// Counts how many objects of the same type existed before
	/// </summary>
	/// <typeparam name="T">type to find</typeparam>
	/// <returns>vector with entries of type T</returns>
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	std::shared_ptr<T> FindRandomObject(EnumType matchflags, EnumType excludeflags, std::set<FormID>& excluded,std::function<bool(std::shared_ptr<Form>)> pred)
	{
		std::shared_lock<std::shared_mutex> guard(_hashmaplock);
		FormID formid = Utility::RandomInt(_baseformid, _nextformid - 1);
		std::shared_ptr<T> result;
		for (auto& [_, form] : _hashmap) {
			if (form->GetType() == T::GetTypeStatic() && excluded.contains(form->GetFormID()) == false && form->GetFormID() >= formid && (form->GetFlags() & matchflags) > 0 && (form->GetFlags() & excludeflags) == 0 && pred(form)) {
				result = dynamic_pointer_cast<T>(form);
				break;
			}
		}
		return result;
	}
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

	/// <summary>
	/// returns a singleton instance
	/// </summary>
	/// <returns>singleton</returns>
	static LoadResolver* GetSingleton();
	/// <summary>
	/// sets the database pointer
	/// </summary>
	/// <param name="dat"></param>
	void SetData(Data* dat);
	/// <summary>
	/// ptr to database
	/// </summary>
	Data* _data = nullptr;
	/// <summary>
	/// ptr to oracle
	/// </summary>
	std::shared_ptr<Oracle> _oracle;
	/// <summary>
	/// current task if available
	/// </summary>
	std::string current = "";
	/// <summary>
	/// whether this is the final save to be loaded
	/// </summary>
	bool finalsave = false;

	/// <summary>
	/// adds task to queue
	/// </summary>
	/// <param name="a_task">task to add to queue</param>
	void AddTask(TaskFn a_task);
	/// <summary>
	/// adds task to queue
	/// </summary>
	/// <param name="a_task">task to add to queue</param>
	void AddTask(TaskDelegate* a_task);

	/// <summary>
	/// adds a task to the late queue
	/// </summary>
	/// <param name="a_task">task to add to late queue</param>
	void AddLateTask(TaskFn a_task);
	/// <summary>
	/// adds a task to the late queue
	/// </summary>
	/// <param name="a_task">task to add to late queue</param>
	void AddLateTask(TaskDelegate* a_task);

	void AddRegeneration(FormID formid);

	/// <summary>
	/// returns a form matching the given type and form id
	/// </summary>
	/// <typeparam name="T">Type to search database for</typeparam>
	/// <param name="formid">form id to search for</param>
	/// <returns>valid shared_ptr if form was found, invalid shared_ptr otherwise</returns>
	template <class T>
	std::shared_ptr<T> ResolveFormID(FormID formid)
	{
		auto itr = _data->_hashmap.find(formid);
		if (itr != _data->_hashmap.end()) {
			if (itr->second)
				return dynamic_pointer_cast<T>(itr->second);
		}
		return {};
	}

	/// <summary>
	/// Executes all tasks waiting in the queue
	/// </summary>
	/// <param name="progress">variable incremented and overwritten for each resolved task</param>
	void Resolve(uint64_t& progress);
	/// <summary>
	/// Executes all tasks waiting in the late queue
	/// </summary>
	/// <param name="progress">variable incremented and overwritten for each resolved task</param>
	void ResolveLate(uint64_t& progress);

	void Regenerate(uint64_t& progress, uint64_t& max, std::shared_ptr<SessionData> sessiondata, int32_t numthreads);

	void Regenrate_Intern(std::shared_ptr<SessionData> sessiondata, uint64_t* progress);

	void Regenerate_Free(std::shared_ptr<SessionData> sessiondata, uint64_t* progress);

	/// <summary>
	/// returns the total number of tasks waiting in queue
	/// </summary>
	/// <returns>number of waiting tasks</returns>
	size_t TaskCount() { return _tasks.size() + _latetasks.size(); }
	size_t TaskCountEarly() { return _tasks.size(); }
	size_t TaskCountLate() { return _latetasks.size(); }

private:
	/// <summary>
	/// task queue for regular tasks
	/// </summary>
	std::queue<TaskDelegate*> _tasks;
	/// <summary>
	/// task queue for late tasks
	/// </summary>
	std::queue<TaskDelegate*> _latetasks;
	/// <summary>
	/// mutex for access to task queues
	/// </summary>
	std::mutex _lock;

	std::unordered_set<FormID> _regeneration;
	std::deque<std::shared_ptr<Input>> _regenqueue;
	std::mutex _regenlock;
	
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

template <>
std::shared_ptr<Session> Data::CreateForm();
template <>
std::shared_ptr<TaskController> Data::CreateForm();
template <>
std::shared_ptr<Settings> Data::CreateForm();
template <>
std::shared_ptr<Oracle> Data::CreateForm();
template <>
std::shared_ptr<Generator> Data::CreateForm();
template <>
std::shared_ptr<ExclusionTree> Data::CreateForm();
template <>
std::shared_ptr<ExecutionHandler> Data::CreateForm();
template <>
std::shared_ptr<SessionData> Data::CreateForm();
