#include "Data.h"
#include "Utility.h"
#include "TaskController.h"
#include "ExecutionHandler.h"

#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>
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

std::string Data::GetSaveName()
{
	std::string name = uniquename + "_" + std::to_string(savenumber);
	savenumber++;
	return name;
}

void Data::Save()
{
	// saves the current state of the program to disk
	// saving requires all active operations to cease while the data is collected and written to disk
	// 

	// create new file on disc
	std::string name = GetSaveName();
	std::ofstream save = std::ofstream((savepath / name), std::ios_base::out | std::ios_base::binary);
	if (save.is_open()) {
		loginfo("Opened save-file \"{}\"", name);
		// lock access to taskcontroller and executionhandler
		std::shared_ptr<TaskController> taskcontrol = CreateForm<TaskController>();
		std::shared_ptr<ExecutionHandler> execcontrol = CreateForm<ExecutionHandler>();
		execcontrol->Freeze();
		taskcontrol->Freeze();

		// write main information about savefile: name, savenumber, nextformid, etc.
		{
			size_t len = 14;
			size_t offset = 0;
			unsigned char* buffer = new unsigned char[len];
			Buffer::Write(saveversion, buffer, offset);
			Buffer::Write(_nextformid, buffer, offset);
			Buffer::Write(_globalTasks, buffer, offset);
			Buffer::Write(_globalExec, buffer, offset);
			save.write((char*)buffer, len);
			delete[] buffer;
		}
		// write session data
		{
			std::shared_lock<std::shared_mutex> guard(_hashmaplock);
			for (auto& [formid, form] : _hashmap) {
				size_t len = 0;
				unsigned char* buffer = nullptr;
				switch (form->GetType())
				{
				case FormType::Input:
					buffer = Records::CreateRecord<Input>(dynamic_pointer_cast<Input>(form), len);
					break;
				case FormType::Grammar:
					buffer = Records::CreateRecord<Grammar>(dynamic_pointer_cast<Grammar>(form), len);
					break;
				case FormType::DevTree:
					buffer = Records::CreateRecord<DerivationTree>(dynamic_pointer_cast<DerivationTree>(form), len);
					break;
				case FormType::ExclTree:
					buffer = Records::CreateRecord<ExclusionTree>(dynamic_pointer_cast<ExclusionTree>(form), len);
					break;
				case FormType::Generator:
					buffer = Records::CreateRecord<Generator>(dynamic_pointer_cast<Generator>(form), len);
					break;
				case FormType::Session:
					buffer = Records::CreateRecord<Session>(dynamic_pointer_cast<Session>(form), len);
					break;
				case FormType::Settings:
					buffer = Records::CreateRecord<Settings>(dynamic_pointer_cast<Settings>(form), len);
					break;
				case FormType::Test:
					buffer = Records::CreateRecord<Test>(dynamic_pointer_cast<Test>(form), len);
					break;
				case FormType::TaskController:
					buffer = Records::CreateRecord<TaskController>(dynamic_pointer_cast<TaskController>(form), len);
					break;
				case FormType::ExecutionHandler:
					buffer = Records::CreateRecord<ExecutionHandler>(dynamic_pointer_cast<ExecutionHandler>(form), len);
					break;
				default:
					logcritical("Trying to save unknown formtype");
					break;
				}
				if (buffer != nullptr)
				{
					save.write((char*)buffer, len);
				}else
					logcritical("record buffer could not be created")
			}
		}

		// unlock taskcontroller and executionhandler
		taskcontrol->Thaw();
		execcontrol->Thaw();
	} else
		logcritical("Cannot open new savefile");
	throw std::runtime_error("Data::Save is not yet implemented");
}

void Data::Load(std::string name)
{
	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::directory_iterator(savepath)) {
		if (entry.exists() && !entry.path().empty() && entry.path().extension() == extension.c_str()) {
			if (auto path = entry.path().string(); path.rfind(name) != std::string::npos) {
				files.push_back(entry.path());
			}
		}
	}
	if (files.empty()) {
		logcritical("No save files were found with the unique name {}", name);
		return;
	}
	int32_t number = 0;
	int32_t num = 0;
	std::filesystem::path fullname = "";
	for (int32_t i = 0; i < (int32_t)files.size(); i++)
	{
		auto rec = Utility::SplitString(files[i].filename().string(), '_', false);
		if (rec.size() == 2)
		{
			// rec[0] == name
			num = 0;
			try {
				num = std::stoi(rec[1]);
			} catch (std::exception) {}
			if (num > number)
			{
				number = num;
				fullname = files[i];
			}
		}
	}
	if (number != 0)
	{
		uniquename = name;
		savenumber = number + 1;
		LoadIntern(fullname);
	}
	else {
		loginfo("No save files were found with the unique name {}", name);
		return;
	}
}

void Data::Load(std::string name, int32_t number)
{
	// find the appropiate save file to open
	// additionally find the one with the highest number, so that we can identify the number of the next save
	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::directory_iterator(savepath)) {
		if (entry.exists() && !entry.path().empty() && entry.path().extension() == extension.c_str()) {
			if (auto path = entry.path().string(); path.rfind(name) != std::string::npos) {
				files.push_back(entry.path());
			}
		}
	}
	if (files.empty()) {
		loginfo("No save files were found with the unique name {}", name);
		return;
	}
	bool found = false;
	int32_t highest = 0;
	int32_t num = 0;
	std::filesystem::path fullname = "";
	for (int32_t i = 0; i < (int32_t)files.size(); i++) {
		auto rec = Utility::SplitString(files[i].filename().string(), '_', false);
		if (rec.size() == 2) {
			// rec[0] == name
			num = 0;
			try {
				num = std::stoi(rec[1]);
			} catch (std::exception) {}
			if (num == number) {
				found = true;
				fullname = files[i];
			}
			if (num > highest)
				highest = num;
		}
	}
	if (found) {
		uniquename = name;
		savenumber = highest + 1;
		LoadIntern(fullname);
	} else {
		loginfo("No save file was found with the unique name {} and number {}", name, number);
		return;
	}
}

void Data::LoadIntern(std::filesystem::path path)
{
	std::ifstream save = std::ifstream(path, std::ios_base::in | std::ios_base::binary);
	if (save.is_open()) {
		loginfo("Opened save-file \"{}\"", path.filename().string());




		_loaded = true;
	} else
		logcritical("Cannot open savefile");
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

void LoadResolver::AddTask(TaskFn a_task)
{
	AddTask(new Task(std::move(a_task)));
}

void LoadResolver::AddTask(TaskDelegate* a_task)
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
		TaskDelegate* del;
		del = tasks.front();
		tasks.pop();
		del->Run();
		del->Dispose();
	}
}

LoadResolver::Task::Task(TaskFn&& a_fn) :
	_fn(std::move(a_fn))
{}

void LoadResolver::Task::Run()
{
	_fn();
}

void LoadResolver::Task::Dispose()
{
	delete this;
}


template<>
std::shared_ptr<Session> Data::CreateForm()
{
	std::shared_ptr<Session> ptr;
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		auto itr = _hashmap.find(StaticFormIDs::Session);
		if (itr != _hashmap.end())
			ptr = dynamic_pointer_cast<Session>(itr->second);
	}
	if (ptr)
		return ptr;
	else {
		ptr = std::make_shared<Session>();
		ptr->data = this;
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
		auto itr = _hashmap.find(StaticFormIDs::TaskController);
		if (itr != _hashmap.end())
			ptr = dynamic_pointer_cast<TaskController>(itr->second);
	}
	if (ptr)
		return ptr;
	else {
		if (_globalTasks)
			ptr = std::shared_ptr<TaskController>(TaskController::GetSingleton());
		else
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
		auto itr = _hashmap.find(StaticFormIDs::Settings);
		if (itr != _hashmap.end())
			ptr = dynamic_pointer_cast<Settings>(itr->second);
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
		auto itr = _hashmap.find(StaticFormIDs::Oracle);
		if (itr != _hashmap.end())
			ptr = dynamic_pointer_cast<Oracle>(itr->second);
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
		auto itr = _hashmap.find(StaticFormIDs::Generator);
		if (itr != _hashmap.end())
			ptr = dynamic_pointer_cast<Generator>(itr->second);
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
		auto itr = _hashmap.find(StaticFormIDs::ExclusionTree);
		if (itr != _hashmap.end())
			ptr = dynamic_pointer_cast<ExclusionTree>(itr->second);
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
		auto itr = _hashmap.find(StaticFormIDs::ExecutionHandler);
		if (itr != _hashmap.end())
			ptr = dynamic_pointer_cast<ExecutionHandler>(itr->second);
	}
	if (ptr)
		return ptr;
	else {
		if (_globalExec)
			ptr = std::shared_ptr<ExecutionHandler>(ExecutionHandler::GetSingleton());
		else
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
