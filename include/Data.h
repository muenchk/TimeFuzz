#pragma once

#include <string>
#include <queue>
#include <unordered_map>
#include <mutex>

#include "TaskController.h"
#include "PtrHolder.h"

class LoadResolver;

class Data
{
private:
	std::string uniquename;
	std::string savename;
	int32_t savenumber;
	LoadResolver* lresolve;

	uint32_t nextformid = 1;
	std::mutex lockformid;
	std::unordered_map<uint32_t, PtrHolder*> hashmap;

	friend LoadResolver;

public:

	Data();

	static Data* GetSingleton();

	template<class T>
	bool RegisterExistingFormID(std::shared_ptr<T> ptr)
	{
		if (ptr) {
			if (auto itr = hashmap.find(ptr->GetFormID()); itr == hashmap.end()) {
				PtrHolderClass<T>* holder = new PtrHolderClass<T>();
				holder->type = T::GetType();
				holder->formid = ptr->GetFormID();
				holder->ptr = ptr;
				hashmap.insert(ptr->GetFormID(), holder);
			}
		}
		return false;
	}

	template<class T>
	bool RegisterNewFormID(std::shared_ptr<T> ptr)
	{
		if (ptr)
			;
		else
			return false;
		uint32_t formid = 0;
		{
			std::lock_guard<std::mutex> guard(lockformid);
			formid = nextformid++;
		}
		PtrHolderClass<T>* holder = new PtrHolderClass<T>();
		holder->type = T::GetType();
		holder->formid = formid;
		holder->ptr = ptr;
		ptr->SetFormID(formid);
		hashmap.insert(formid, holder);
	}

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
	std::shared_ptr<T> ResolveFormID(uint32_t formid)
	{
		auto itr = data->hashmap.find(formid);
		if (itr != data->hashmap.end())
		{
			PtrHolderClass<T>* holder = std::get<1>(*itr)->As<T>();
			if (auto ptr = holder->ptr.lock(); ptr)
				return ptr;
			else
				return {};
		}
		return {};
	}

	void Resolve();
};
