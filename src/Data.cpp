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
	std::string name = uniquename + "_" + std::to_string(savenumber) + extension;
	savenumber++;
	return name;
}

void Data::SetSaveName(std::string name)
{
	uniquename = name;
}

void Data::SetSavePath(std::filesystem::path path)
{
	savepath = path;
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
			size_t len = 30;
			size_t offset = 0;
			unsigned char* buffer = new unsigned char[len];
			Buffer::Write(saveversion, buffer, offset);
			Buffer::Write(guid1, buffer, offset);
			Buffer::Write(guid2, buffer, offset);
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
				case FormType::Oracle:
					buffer = Records::CreateRecord<Oracle>(dynamic_pointer_cast<Oracle>(form), len);
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
		save.flush();
		save.close();
		// unlock taskcontroller and executionhandler
		taskcontrol->Thaw();
		execcontrol->Thaw();
		loginfo("Saved session");
	} else
		logcritical("Cannot open new savefile");
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
		size_t BUFSIZE = 4096;
		unsigned char* buffer = new unsigned char[BUFSIZE];
		unsigned char* cbuffer = nullptr;
		unsigned char* buf = nullptr;
		size_t flen = std::filesystem::file_size(path);
		size_t pos = 0;
		size_t offset = 0;
		// get save file version
		int32_t version = 0;
		uint64_t ident1 = 0;
		uint64_t ident2 = 0;
		if (flen - pos >= 4) {
			save.read((char*)buffer, 4);
			if (save.gcount() == 4)
				version = Buffer::ReadInt32(buffer, offset);
			pos += 4;
		}
		if (flen - pos >= 16) {
			save.read((char*)buffer, 16);
			offset = 0;
			if (save.gcount() == 16) {
				ident1 = Buffer::ReadUInt64(buffer, offset);
				ident2 = Buffer::ReadUInt64(buffer, offset);
			}
			pos += 16;
		}
		if (guid1 == ident1 && guid2 == ident2) {
			bool abort = false;
			// this is our savefile
			// read data stuff
			if (flen - pos >= 10)
			{
				save.read((char*)buffer, 10);
				offset = 0;
				if (save.gcount() == 10) {
					_nextformid = Buffer::ReadUInt64(buffer, offset);
					_globalTasks = Buffer::ReadBool(buffer, offset);
					_globalExec = Buffer::ReadBool(buffer, offset);
				}
				else
				{
					logcritical("Save file does not appear to have the proper format: fail data fields");
					abort = true;
				}
				pos += 10;
			}

			if (!abort) {
				switch (version) {
				case 0:  // couldn't get saveversion
					logcritical("Save file does not appear to have the proper format: fail version");
					break;
				case 0x1:  // save file version 1
					{
						size_t rlen = 0;
						int32_t rtype = 0;
						bool cbuf = false;
						while (pos < flen) {
							rlen = 0;
							rtype = 0;
							// read length of record, type of record
							if (flen - pos >= 12) {
								save.read((char*)buffer, 12);
								offset = 0;
								if (save.gcount() == 12) {
									rlen = Buffer::ReadSize(buffer, offset);
									rtype = Buffer::ReadInt32(buffer, offset);
								}
								pos += 12;
							}
							if (rlen > 0) {
								// if the record is small enough to fit into our regular buffer, use that one, else use a new custom buffer we have to delete later
								if (rlen <= BUFSIZE) {
									cbuf = false;
									save.read((char*)buffer, rlen);
									pos += rlen;
									if (save.gcount() != (std::streamsize)rlen) {
										// we haven't read as much as we want, probs end-of-file, so end iteration and continue
										logwarn("Found unexpected end-of-file");
										continue;
									}
									buf = buffer;
								} else {
									cbuf = true;
									cbuffer = new unsigned char[rlen];
									save.read((char*)cbuffer, rlen);
									pos += rlen;
									if (save.gcount() != (std::streamsize)rlen) {
										// we haven't read as much as we want, probs end-of-file, so end iteration and continue
										logwarn("Found unexpected end-of-file");
										delete[] cbuffer;
										cbuffer = nullptr;
										continue;
									}
									buf = cbuffer;
								}
								offset = 0;
								// create the correct record type
								switch (rtype) {
								case FormType::Input:
									{
										bool res = RegisterForm(Records::ReadRecord<Input>(buf, offset, rlen, _lresolve));
										if (res) {
											loginfo("Read Record:      Input");
										} else {
											loginfo("Failed Record:    Input");
										}
									}
									break;
								case FormType::Grammar:
									{
										bool res = RegisterForm(Records::ReadRecord<Grammar>(buf, offset, rlen, _lresolve));
										if (res) {
											loginfo("Read Record:      Grammar");
										} else {
											loginfo("Failed Record:    Grammar");
										}
									}
									break;
								case FormType::DevTree:
									{
										bool res = RegisterForm(Records::ReadRecord<DerivationTree>(buf, offset, rlen, _lresolve));
										if (res) {
											loginfo("Read Record:      DerivationTree");
										} else {
											loginfo("Failed Record:    DerivationTree");
										}
									}
									break;
								case FormType::ExclTree:
									{
										auto excl = CreateForm<ExclusionTree>();
										bool res = excl->ReadData(buf, offset, rlen, _lresolve);
										if (offset > rlen)
											res = false;
										if (res) {
											loginfo("Read Record:      ExclusionTree");
										} else {
											loginfo("Failed Record:    ExclusionTree");
										}
									}
									break;
								case FormType::Generator:
									{
										auto gen = CreateForm<Generator>();
										bool res = gen->ReadData(buf, offset, rlen, _lresolve);
										if (offset > rlen)
											res = false;
										if (res) {
											loginfo("Read Record:      Generator");
										} else {
											loginfo("Failed Record:    Generator");
										}
									}
									break;
								case FormType::Session:
									{
										auto session = CreateForm<Session>();
										bool res = session->ReadData(buf, offset, rlen, _lresolve);
										if (offset > rlen)
											res = false;
										if (res) {
											loginfo("Read Record:      Session");
										} else {
											loginfo("Failed Record:    Session");
										}
									}
									break;
								case FormType::Settings:
									{
										auto sett = CreateForm<Settings>();
										bool res = sett->ReadData(buf, offset, rlen, _lresolve);
										if (offset > rlen)
											res = false;
										if (res) {
											loginfo("Read Record:      Settings");
										} else {
											loginfo("Failed Record:    Settings");
										}
									}
									break;
								case FormType::Test:
									{
										bool res = RegisterForm(Records::ReadRecord<Test>(buf, offset, rlen, _lresolve));
										if (res) {
											loginfo("Read Record:      Test");
										} else {
											loginfo("Failed Record:    Test");
										}
									}
									break;
								case FormType::TaskController:
									{
										auto tcontrol = CreateForm<TaskController>();
										bool res = tcontrol->ReadData(buf, offset, rlen, _lresolve);
										if (offset > rlen)
											res = false;
										if (res) {
											loginfo("Read Record:      TaskController");
										} else {
											loginfo("Failed Record:    TaskController");
										}
									}
									break;
								case FormType::ExecutionHandler:
									{
										auto exec = CreateForm<ExecutionHandler>();
										bool res = exec->ReadData(buf, offset, rlen, _lresolve);
										if (offset > rlen)
											res = false;
										if (res) {
											loginfo("Read Record:      ExecutionHandler");
										} else {
											loginfo("Failed Record:    ExecutionHandler");
										}
									}
									break;
								case FormType::Oracle:
									{
										auto oracle = CreateForm<Oracle>();
										bool res = oracle->ReadData(buf, offset, rlen, _lresolve);
										if (offset > rlen)
											res = false;
										if (res) {
											loginfo("Read Record:      ExecutionHandler");
										} else {
											loginfo("Failed Record:    ExecutionHandler");
										}
									}
									break;
								}
								if (cbuf)
									delete[] cbuffer;
							}
						}
						_loaded = true;
						loginfo("Loaded save");
					}
					break;
				default:  // unknown save file version
					logcritical("Save file version is unknown");
					break;
				}
			}
		} else {
			// this cannot be our savefile
			logcritical("Save file does not have the proper format: fail guid");
		}
		delete[] buffer;
		save.close();
		_lresolve->Resolve();
		loginfo("Loaded session");
	} else
		logcritical("Cannot open savefile");
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
		FormID formid = StaticFormIDs::Session;
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
		FormID formid = StaticFormIDs::TaskController;
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
		FormID formid = StaticFormIDs::Settings;
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
		FormID formid = StaticFormIDs::Oracle;
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
		FormID formid = StaticFormIDs::Generator;
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
		FormID formid = StaticFormIDs::ExclusionTree;
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
		FormID formid = StaticFormIDs::ExecutionHandler;
		ptr->SetFormID(formid);
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ formid, dynamic_pointer_cast<Form>(ptr) });
		}
		return ptr;
	}
}
