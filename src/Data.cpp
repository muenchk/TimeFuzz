#include "Data.h"
#include "Utility.h"
#include "TaskController.h"
#include "ExecutionHandler.h"
#include "LuaEngine.h"
#include "LZMAStreamBuf.h"
#include "SessionData.h"
#include "DeltaDebugging.h"
#include "Generation.h"
#include "BufferOperations.h"

#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <exception>

Data::Data()
{
	_lresolve = new LoadResolver();
	_lresolve->SetData(this);
	RegisterForms();
}

Data* Data::GetSingleton()
{
	static Data data;
	return std::addressof(data);
}

void Data::RegisterForms()
{
	// this is a bit brute force, and needs to be updated when a new class is introduced,
	// but then again the save and load methods need to be updated as well
	Input::RegisterFactories();
	_objectRecycler.insert_or_assign(Input::GetTypeStatic(), new ObjStorage());
	Grammar::RegisterFactories();
	_objectRecycler.insert_or_assign(Grammar::GetTypeStatic(), new ObjStorage());
	DerivationTree::RegisterFactories();
	_objectRecycler.insert_or_assign(DerivationTree::GetTypeStatic(), new ObjStorage());
	ExclusionTree::RegisterFactories();
	_objectRecycler.insert_or_assign(ExclusionTree::GetTypeStatic(), new ObjStorage());
	Generator::RegisterFactories();
	_objectRecycler.insert_or_assign(Generator::GetTypeStatic(), new ObjStorage());
	Generation::RegisterFactories();
	_objectRecycler.insert_or_assign(Generation::GetTypeStatic(), new ObjStorage());
	Session::RegisterFactories();
	_objectRecycler.insert_or_assign(Session::GetTypeStatic(), new ObjStorage());
	Settings::RegisterFactories();
	_objectRecycler.insert_or_assign(Settings::GetTypeStatic(), new ObjStorage());
	Test::RegisterFactories();
	_objectRecycler.insert_or_assign(Test::GetTypeStatic(), new ObjStorage());
	TaskController::RegisterFactories();
	_objectRecycler.insert_or_assign(TaskController::GetTypeStatic(), new ObjStorage());
	ExecutionHandler::RegisterFactories();
	_objectRecycler.insert_or_assign(ExecutionHandler::GetTypeStatic(), new ObjStorage());
	Oracle::RegisterFactories();
	_objectRecycler.insert_or_assign(Oracle::GetTypeStatic(), new ObjStorage());
	SessionData::RegisterFactories();
	_objectRecycler.insert_or_assign(SessionData::GetTypeStatic(), new ObjStorage());
	DeltaDebugging::DeltaController::RegisterFactories();
	_objectRecycler.insert_or_assign(DeltaDebugging::DeltaController::GetTypeStatic(), new ObjStorage());
}

std::string Data::GetSaveName()
{
	std::string name = _uniquename + "_" + std::to_string(_savenumber) + _extension;
	_savenumber++;
	return name;
}

void Data::SetSaveName(std::string name)
{
	_uniquename = name;
}

void Data::SetSavePath(std::filesystem::path path)
{
	_savepath = path;
}

void Data::Save(std::shared_ptr<Functions::BaseFunction> callback)
{
	_runtime += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - _sessionBegin);
	_actionloadsave = true;
	_actionloadsave_max = 0;
	_actionrecord_len = 0;
	_actionrecord_offset = 0;
	_status = "Beginning save...";
	StartProfiling;
	// saves the current state of the program to disk
	// saving requires all active operations to cease while the data is collected and written to disk
	
	SaveStats stats;

	auto settings = CreateForm<Settings>();

	auto sessiondata = CreateForm<SessionData>();

	std::cout << "hashtable size: " << _hashmap.size() << "\n";
	// create new file on disc
	std::string name = GetSaveName();
	if (!std::filesystem::exists(_savepath))
		std::filesystem::create_directories(_savepath);
	logdebug("{}", (_savepath / name).string());
	std::cout << "path: " << (_savepath / name).string() << "\n";
	std::ofstream fsave = std::ofstream((_savepath / name), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
	if (fsave.is_open()) {
		// lock access to taskcontroller and executionhandler
		_status = "Freezing controllers...";
		std::shared_ptr<TaskController> taskcontrol = CreateForm<TaskController>();
		std::shared_ptr<ExecutionHandler> execcontrol = CreateForm<ExecutionHandler>();
		taskcontrol->RequestFreeze();
		//execcontrol->Freeze(true);
		execcontrol->Freeze(false);
		taskcontrol->Freeze();

		// write main information about savefile: name, _savenumber, nextformid, _runtime etc.
		{
			size_t len = 38;
			size_t offset = 0;
			unsigned char* buffer = new unsigned char[len];
			Buffer::Write(saveversion, buffer, offset);
			Buffer::Write(guid1, buffer, offset);
			Buffer::Write(guid2, buffer, offset);
			Buffer::Write(_nextformid, buffer, offset);
			Buffer::Write(_globalTasks, buffer, offset);
			Buffer::Write(_globalExec, buffer, offset);
			Buffer::Write(_runtime, buffer, offset);
			fsave.write((char*)buffer, len);
			delete[] buffer;
		}
		// also write information about the file itself, including compression used and compression level
		{
			size_t len = 5;
			size_t offset = 0;
			unsigned char* buffer = new unsigned char[len];
			Buffer::Write(settings->saves.compressionLevel, buffer, offset);
			Buffer::Write(settings->saves.compressionExtreme, buffer, offset);
			fsave.write((char*)buffer, len);
			delete[] buffer;
		}

		Streambuf* sbuf = nullptr;
		if (settings->saves.compressionLevel != -1) {
			sbuf = new LZMAStreambuf(&fsave, settings->saves.compressionLevel, settings->saves.compressionExtreme, settings->general.numthreads);
		} else
			sbuf = new Streambuf(&fsave);
		std::ostream save(sbuf);
		loginfo("Opened save-file \"{}\"", name);

		// here we are going to save the callbac we are supposed to call, so that we can call it even if the 
		// program end after this save
		// we are allocating a flat 256 bytes for this, the callback shouldn't be larger than this
		{
			size_t len = 256;
			size_t offset = 0;
			unsigned char* buffer = new unsigned char[len];
			if (callback) {
				Buffer::Write(true, &save, offset);
				callback->WriteData(&save, offset);
				save.write((char*)buffer, len - callback->GetLength() - 1);
			} else {
				Buffer::Write(false, buffer, offset);
				save.write((char*)buffer, len);
			}
			delete[] buffer;
		}

		_status = "Writing save...";

		// write session data
		{
			std::shared_lock<std::shared_mutex> guard(_hashmaplock);
			loginfo("Saving {} records...", _hashmap.size());
			_actionloadsave_max = _hashmap.size() + 1;
			_actionloadsave_current = 0;
			{
				size_t len = 8;
				size_t offset = 0;
				unsigned char* buffer = new unsigned char[len];
				Buffer::WriteSize(_hashmap.size() + 1 /*string Hashmap*/, buffer, offset);
				save.write((char*)buffer, len);
				delete[] buffer;
			}
			// write string hashmap [its coded as a type of record]
			{
				size_t sz = GetStringHashmapSize();  // record length
				_actionrecord_offset = 0;
				_actionrecord_len = sz;
				Records::CreateRecordHeaderStringHashmap(&save, _actionrecord_len, _actionrecord_offset);
				WriteStringHashmap(&save, _actionrecord_offset, _actionrecord_len);
				loginfo("Wrote string hashmap. {} entries.", _stringHashmap.left.size());
				_actionloadsave_current++;
				if (fsave.bad())
					logcritical("critical error in underlying savefile");
			}
			for (auto& [formid, form] : _hashmap) {
				_actionrecord_len = 0;
				_actionrecord_offset = 0;
				//unsigned char* buffer = nullptr;
				_record = form->GetType();
				switch (_record) {
				case FormType::Input:
					Records::CreateRecord<Input>(dynamic_pointer_cast<Input>(form),&save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: Input");
					}
					stats._Input++;
					//logdebug("Write Record:      Input");
					break;
				case FormType::Grammar:
					Records::CreateRecord<Grammar>(dynamic_pointer_cast<Grammar>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: Grammar");
					}
					stats._Grammar++;
					//Logdebug("Write Record:      Grammar");
					break;
				case FormType::DevTree:
					Records::CreateRecord<DerivationTree>(dynamic_pointer_cast<DerivationTree>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: DerivationTree");
					}
					stats._DevTree++;
					//logdebug("Write Record:      DerivationTree");
					break;
				case FormType::ExclTree:
					Records::CreateRecord<ExclusionTree>(dynamic_pointer_cast<ExclusionTree>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: ExclusionTree");
					}
					stats._ExclTree++;
					//logdebug("Write Record:      ExclusionTree");
					break;
				case FormType::Generator:
					Records::CreateRecord<Generator>(dynamic_pointer_cast<Generator>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: Generator");
					}
					stats._Generator++;
					//logdebug("Write Record:      Generator");
					break;
				case FormType::Session:
					Records::CreateRecord<Session>(dynamic_pointer_cast<Session>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: Session");
					}
					stats._Session++;
					//logdebug("Write Record:      Session");
					break;
				case FormType::Settings:
					Records::CreateRecord<Settings>(dynamic_pointer_cast<Settings>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: Settings");
					}
					stats._Settings++;
					//logdebug("Write Record:      Settings");
					break;
				case FormType::Test:
					Records::CreateRecord<Test>(dynamic_pointer_cast<Test>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: Test");
						auto sz = dynamic_pointer_cast<Test>(form)->GetDynamicSize();
					}
					stats._Test++;
					//logdebug("Write Record:      Test");
					break;
				case FormType::TaskController:
					Records::CreateRecord<TaskController>(dynamic_pointer_cast<TaskController>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: TaskController");
					}
					stats._TaskController++;
					//logdebug("Write Record:      TaskController");
					break;
				case FormType::ExecutionHandler:
					Records::CreateRecord<ExecutionHandler>(dynamic_pointer_cast<ExecutionHandler>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: ExecutionHandler");
					}
					stats._ExecutionHandler++;
					//logdebug("Write Record:      ExecutionHandler");
					break;
				case FormType::Oracle:
					Records::CreateRecord<Oracle>(dynamic_pointer_cast<Oracle>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: Oracle");
					}
					stats._Oracle++;
					//logdebug("Write Record:      Oracle");
					break;
				case FormType::SessionData:
					Records::CreateRecord<SessionData>(dynamic_pointer_cast<SessionData>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: SessionData");
					}
					stats._SessionData++;
					//logdebug("Write Record:      SessionData");
					break;
				case FormType::DeltaController:
					Records::CreateRecord<DeltaDebugging::DeltaController>(dynamic_pointer_cast<DeltaDebugging::DeltaController>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: DeltaController");
					}
					stats._DeltaController++;
					//logdebug("Write Record:      DeltaController");
					break;
				case FormType::Generation:
					Records::CreateRecord<Generation>(dynamic_pointer_cast<Generation>(form), &save, _actionrecord_offset, _actionrecord_len);
					if (_actionrecord_offset > _actionrecord_len) {
						logcritical("Buffer overflow in record: Generation");
					}
					stats._Generation++;
					//logdebug("Write Record:      Generation");
					break;
				default:
					stats._Fail++;
					logcritical("Trying to save unknown formtype");
					break;
				}
				//if (buffer != nullptr) {
				//	save.write((char*)buffer, _actionrecord_len);
				//	if (fsave.bad())
				//		logcritical("critical error in underlying savefile")
				//} else {
				//	stats._Fail++;
				//	logcritical("record buffer could not be created");
				//}
				_actionloadsave_current++;
			}
		}
		sbuf->flush();
		save.flush();
		fsave.flush();
		fsave.close();
		if (sbuf != nullptr)
		{
			delete sbuf;
			sbuf = nullptr;
		}
		// set proper
		_sessionBegin = std::chrono::steady_clock::now();
		// unlock taskcontroller and executionhandler
		taskcontrol->Thaw();
		execcontrol->Thaw();
		loginfo("Saved session");
	} else {
		logcritical("Cannot open new savefile");
	}

	if (callback) {
		auto sessdata = CreateForm<SessionData>();
		sessdata->_controller->AddTask(callback);
	}

	loginfo("Saved Records:");
	loginfo("Input: {}", stats._Input);
	loginfo("Grammar: {}", stats._Grammar);
	loginfo("DerivationTree: {}", stats._DevTree);
	loginfo("ExclusionTree: {}", stats._ExclTree);
	loginfo("Generator: {}", stats._Generator);
	loginfo("Session: {}", stats._Session);
	loginfo("Settings: {}", stats._Settings);
	loginfo("Test: {}", stats._Test);
	loginfo("TaskController: {}", stats._TaskController);
	loginfo("ExecutionHandler: {}", stats._ExecutionHandler);
	loginfo("Oracle: {}", stats._Oracle);
	loginfo("Fails: {}", stats._Fail);

	_actionloadsave = false;
	_status = "Running...";
	profile(TimeProfiling, "Saving session");
}

void Data::Load(std::string name, LoadSaveArgs& loadArgs)
{
	StartProfiling;
	_status = "Finding save files...";
	_actionloadsave = true;
	_actionloadsave_max = 0;
	std::vector<std::filesystem::path> files;
	if (!std::filesystem::exists(_savepath))
		std::filesystem::create_directories(_savepath);
	for (const auto& entry : std::filesystem::directory_iterator(_savepath)) {
		if (entry.exists() && !entry.path().empty() && entry.path().extension() == _extension.c_str()) {
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
	profile(TimeProfiling, "Pre-Load");
	if (number != 0)
	{
		_uniquename = name;
		_savenumber = number + 1;
		LoadIntern(fullname, loadArgs);
	} else {
		_actionloadsave = false;
		_status = "No savefiles found.";
		loginfo("No save files were found with the unique name {}", name);
		return;
	}
}

void Data::Load(std::string name, int32_t number, LoadSaveArgs& loadArgs)
{
	StartProfiling;
	_status = "Finding save files...";
	_actionloadsave = true;
	_actionloadsave_max = 0;
	// find the appropiate save file to open
	// additionally find the one with the highest number, so that we can identify the number of the next save
	if (!std::filesystem::exists(_savepath))
		std::filesystem::create_directories(_savepath);
	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::directory_iterator(_savepath)) {
		if (entry.exists() && !entry.path().empty() && entry.path().extension() == _extension.c_str()) {
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
	profile(TimeProfiling, "Pre-Load");
	if (found) {
		_uniquename = name;
		_savenumber = highest + 1;
		LoadIntern(fullname, loadArgs);
	} else {
		_actionloadsave = false;
		_status = "No savefiles found.";
		loginfo("No save file was found with the unique name {} and number {}", name, number);
		return;
	}
}

void Data::LoadIntern(std::filesystem::path path, LoadSaveArgs& loadArgs)
{
	_status = "Load save file....";
	_actionloadsave = true;
	_actionloadsave_max = 0;
	_actionrecord_len = 0;
	_actionrecord_offset = 0;
	// callback after load
	std::shared_ptr<Functions::BaseFunction> callback;
	StartProfiling;
	SaveStats stats;
	std::ifstream fsave = std::ifstream(path, std::ios_base::in | std::ios_base::binary);
	if (fsave.is_open()) {
		loginfo("Opened save-file \"{}\"", path.filename().string());
		size_t BUFSIZE = 4096;
		unsigned char* buffer = new unsigned char[BUFSIZE];
		unsigned char* cbuffer = nullptr;
		unsigned char* buf = nullptr;
		size_t flen = std::filesystem::file_size(path);
		_actionloadsave_max = (uint64_t)flen;
		size_t pos = 0;
		size_t offset = 0;
		// get save file version
		int32_t version = 0;
		uint64_t ident1 = 0;
		uint64_t ident2 = 0;
		// compression
		int32_t compressionLevel = -1;
		bool compressionExtreme = false;
		if (flen - pos >= 4) {
			fsave.read((char*)buffer, 4);
			if (fsave.gcount() == 4)
				version = Buffer::ReadInt32(buffer, offset);
			pos += 4;
		}
		if (flen - pos >= 16) {
			fsave.read((char*)buffer, 16);
			offset = 0;
			if (fsave.gcount() == 16) {
				ident1 = Buffer::ReadUInt64(buffer, offset);
				ident2 = Buffer::ReadUInt64(buffer, offset);
			}
			pos += 16;
		}
		if (guid1 == ident1 && guid2 == ident2) {
			logdebug("GUID matches");
			bool abort = false;
			// this is our savefile
			// read data stuff
			if (flen - pos >= 10)
			{
				fsave.read((char*)buffer, 18);
				offset = 0;
				if (fsave.gcount() == 18) {
					_nextformid = Buffer::ReadUInt64(buffer, offset);
					_globalTasks = Buffer::ReadBool(buffer, offset);
					_globalExec = Buffer::ReadBool(buffer, offset);
					_runtime = Buffer::ReadNanoSeconds(buffer, offset);
				}
				else
				{
					logcritical("Save file does not appear to have the proper format: fail data fields");
					abort = true;
				}
				pos += 18;
			}

			// read information about the file itself, including compression used and compression level
			{
				fsave.read((char*)buffer, 5);
				offset = 0;
				if (fsave.gcount() == 5) {
					compressionLevel = Buffer::ReadInt32(buffer, offset);
					compressionExtreme = Buffer::ReadBool(buffer, offset);
				}
				else
				{
					logcritical("Save file does not appear to have the proper format: failed to read compression information");
					abort = true;
				}
			}

			_actionloadsave_current = pos;
			if (!abort) {
				logdebug("decide compression");
				// init compression etc.
				Streambuf* sbuf = nullptr;
				if (compressionLevel != -1)
					sbuf = new LZMAStreambuf(&fsave);
				else
					sbuf = new Streambuf(&fsave);
				std::istream save(sbuf);

				bool fileerror = false;
				// load callback if it exists
				{
					size_t length = 256;
					offset = 0;
					//save.read((char*)buffer, 1);
					//if (save.gcount() == (std::streamsize)length) {
						if (Buffer::ReadBool(&save, offset)) {
							callback = Functions::BaseFunction::Create(&save, offset, length, _lresolve);
							save.read((char*)buffer, length - 1 - callback->GetLength());
						} else
							save.read((char*)buffer, length - 1);
					//} else {
						if (save.bad()) {
						logcritical("Save file does not appear to have the proper format: failed to read callback information");
						fileerror = true;
					}
				}

				// read hashmap and progress information
				_actionloadsave_max = 0;
				_actionloadsave_current = 0;
				{
					save.read(reinterpret_cast<char*>(buffer), 8);
					if (save.bad())
					{
						logcritical("io error");
						fileerror = true;
					}
					offset = 0;
					if (save.gcount() == 8) {
						_actionloadsave_max = Buffer::ReadSize(buffer, offset);
					} else {
						logcritical("Save file does not appear to have the proper format: failed to read hashmap size");
						fileerror = true;
					}
				}

				logdebug("load {} records.", _actionloadsave_max);

				switch (version) {
				case 0:  // couldn't get saveversion
					logcritical("Save file does not appear to have the proper format: fail version");
					break;
				case 0x1:
					logcritical("SAVE VERSION 0x1 IS NOT SUPPORTED. Go back to commit 892de1ed2da1d6d05497f6ab64adfcb3ce2d2b2c");
					_actionloadsave = false;
					_status = "Failed...";
					return;
				case 0x2:  // save file version 1
					{
						size_t rlen = 0;
						int32_t rtype = 0;
						bool cbuf = false;
						while (fileerror == false && _actionloadsave_current < _actionloadsave_max) {
							rlen = 0;
							rtype = 0;
							// read length of record, type of record
							if (flen - pos >= 12) {
								rlen = Buffer::ReadSize(&save, offset);
								rtype = Buffer::ReadInt32(&save, offset);
								if (save.bad())
								{
									// we haven't read as much as we want, probs end-of-file, so end iteration and continue
									logwarn("Found unexpected end-of-file");
									fileerror = true;
									continue;
								}
								pos += 12;
							}
							else
							{
								fileerror = true;
								continue;
							}
							if (rlen > 0) {
								/*// if the record is small enough to fit into our regular buffer, use that one, else use a new custom buffer we have to delete later
								if (rlen <= BUFSIZE) {
									cbuf = false;
									save.read((char*)buffer, rlen);
									pos += rlen;
									if (save.eof() || save.fail()) {
										// we haven't read as much as we want, probs end-of-file, so end iteration and continue
										logwarn("Found unexpected end-of-file");
										fileerror = true;
										continue;
									}
									buf = buffer;
								} else {
									cbuf = true;
									cbuffer = new unsigned char[rlen];
									save.read((char*)cbuffer, rlen);
									pos += rlen;
									if (save.eof() || save.fail()) {
										// we haven't read as much as we want, probs end-of-file, so end iteration and continue
										logwarn("Found unexpected end-of-file");
										fileerror = true;
										delete[] cbuffer;
										cbuffer = nullptr;
										continue;
									}
									buf = cbuffer;
								}*/
								//logdebug("read record data.");
								offset = 0;
								_actionrecord_len = rlen;
								_actionrecord_offset = 0;
								_record = rtype;
								// create the correct record type
								switch (rtype) {
								case 'STRH':
									{
										bool res = ReadStringHashmap(&save, _actionrecord_offset, rlen);
										if (_actionrecord_offset > rlen)
											res = false;
										if (res) {
											loginfo("Read String Hashmap. {} entries.", _stringHashmap.left.size());
										} else {
											logcritical("Failed to read String Hashmap");
										}
									}
								break;
								case FormType::Input:
									{
										//logdebug("Read Record:      Input");
										auto record = Records::ReadRecord<Input>(&save, offset, _actionrecord_offset, rlen, _lresolve);
										if (record && record->HasFlag(Form::FormFlags::Deleted) == false) {
											bool res = RegisterForm(record);
											if (res) {
												stats._Input++;
											} else {
												stats._Fail++;
												logcritical("Failed Record:    Input");
											}
										} else {
											stats._Fail++;
											logcritical("Deleted Record:    Input");
										}
									}
									break;
								case FormType::Grammar:
									{
										//logdebug("Read Record:      Grammar");
										auto record = Records::ReadRecord<Grammar>(&save, offset, _actionrecord_offset, rlen, _lresolve);
										if (record && record->HasFlag(Form::FormFlags::Deleted) == false) {
											bool res = RegisterForm(record);
											if (res) {
												stats._Grammar++;
											} else {
												stats._Fail++;
												logcritical("Failed Record:    Grammar");
											}
										} else {
											stats._Fail++;
											logcritical("Deleted Record:    Grammar");
										}
									}
									break;
								case FormType::DevTree:
									{
										//logdebug("Read Record:      DerivationTree");
										auto record = Records::ReadRecord<DerivationTree>(&save, offset, _actionrecord_offset, rlen, _lresolve);
										if (record && record->HasFlag(Form::FormFlags::Deleted) == false) {
											bool res = RegisterForm(record);
											if (res) {
												stats._DevTree++;
											} else {
												stats._Fail++;
												logcritical("Failed Record:    DerivationTree");
											}
										} else {
											stats._Fail++;
											logcritical("Deleted Record:    DerivationTree");
										}
									}
									break;
								case FormType::ExclTree:
									{
										//logdebug("Read Record:      ExclusionTree");
										auto excl = CreateForm<ExclusionTree>();
										bool res = excl->ReadData(&save, _actionrecord_offset, rlen, _lresolve, loadArgs.skipExlusionTree);
										if (_actionrecord_offset > rlen)
											res = false;
										if (res) {
											stats._ExclTree++;
										} else {
											stats._Fail++;
											logcritical("Failed Record:    ExclusionTree");
										}
										if (loadArgs.skipExlusionTree)
											loginfo("Skipped Reading Record:	ExclusionTree");
									}
									break;
								case FormType::Generator:
									{
										//logdebug("Read Record:      Generator");
										auto gen = CreateForm<Generator>();
										bool res = gen->ReadData(&save, _actionrecord_offset, rlen, _lresolve);
										if (_actionrecord_offset > rlen)
											res = false;
										if (res) {
											stats._Generator++;
										} else {
											stats._Fail++;
											logcritical("Failed Record:    Generator");
										}
									}
									break;
								case FormType::Session:
									{
										//logdebug("Read Record:      Session");
										auto session = CreateForm<Session>();
										bool res = session->ReadData(&save, _actionrecord_offset, rlen, _lresolve);
										if (_actionrecord_offset > rlen)
											res = false;
										if (res) {
											stats._Session++;
										} else {
											stats._Fail++;
											logcritical("Failed Record:    Session");
										}
									}
									break;
								case FormType::Settings:
									{
										//logdebug("Read Record:      Settings");
										auto sett = CreateForm<Settings>();
										bool res = sett->ReadData(&save, _actionrecord_offset, rlen, _lresolve);
										if (_actionrecord_offset > rlen)
											res = false;
										if (res) {
											stats._Settings++;
										} else {
											stats._Fail++;
											logcritical("Failed Record:    Settings");
										}
									}
									break;
								case FormType::Test:
									{
										//logdebug("Read Record:      Test");
										auto record = Records::ReadRecord<Test>(&save, offset, _actionrecord_offset, rlen, _lresolve);
										if (record && record->HasFlag(Form::FormFlags::Deleted) == false) {
											bool res = RegisterForm(record);
											if (res) {
												stats._Test++;
											} else {
												stats._Fail++;
												logcritical("Failed Record:    Test");
											}
										} else {
											stats._Fail++;
											logcritical("Deleted Record:    Test");
										}
									}
									break;
								case FormType::TaskController:
									{
										//logdebug("Read Record:      TaskController");
										auto tcontrol = CreateForm<TaskController>();
										bool res = tcontrol->ReadData(&save, _actionrecord_offset, rlen, _lresolve);
										if (_actionrecord_offset > rlen)
											res = false;
										if (res) {
											stats._TaskController++;
										} else {
											stats._Fail++;
											logcritical("Failed Record:    TaskController");
										}
									}
									break;
								case FormType::ExecutionHandler:
									{
										//logdebug("Read Record:      ExecutionHandler");
										auto exec = CreateForm<ExecutionHandler>();
										bool res = exec->ReadData(&save, _actionrecord_offset, rlen, _lresolve);
										if (_actionrecord_offset > rlen)
											res = false;
										if (res) {
											stats._ExecutionHandler++;
										} else {
											stats._Fail++;
											logcritical("Failed Record:    ExecutionHandler");
										}
									}
									break;
								case FormType::Oracle:
									{
										//logdebug("Read Record:      Oracle");
										auto oracle = CreateForm<Oracle>();
										bool res = oracle->ReadData(&save, _actionrecord_offset, rlen, _lresolve);
										if (_actionrecord_offset > rlen)
											res = false;
										if (res) {
											stats._Oracle++;
											_lresolve->_oracle = oracle;
										} else {
											stats._Fail++;
											logcritical("Failed Record:    Oracle");
										}
									}
									break;
								case FormType::SessionData:
									{
										//logdebug("Read Record:      SessionData");
										auto sessdata = CreateForm<SessionData>();
										bool res = sessdata->ReadData(&save, _actionrecord_offset, rlen, _lresolve);
										if (_actionrecord_offset > rlen)
											res = false;
										if (res) {
											stats._SessionData++;
										} else {
											stats._Fail++;
											logcritical("Failed Record:    SessionData");
										}
									}
									break;
								case FormType::DeltaController:
									{
										//logdebug("Read Record:      Test");
										auto record = Records::ReadRecord<DeltaDebugging::DeltaController>(&save, offset, _actionrecord_offset, rlen, _lresolve);
										if (record && record->HasFlag(Form::FormFlags::Deleted) == false) {
											bool res = RegisterForm(record);
											if (res) {
												stats._DeltaController++;
											} else {
												stats._Fail++;
												logcritical("Failed Record:    DeltaController");
											}
										} else {
											stats._Fail++;
											logcritical("Deleted Record:    DeltaController");
										}
									}
									break;
								case FormType::Generation:
									{
										//logdebug("Read Record:      Generation");
										auto record = Records::ReadRecord<Generation>(&save, offset, _actionrecord_offset, rlen, _lresolve);
										if (record && record->HasFlag(Form::FormFlags::Deleted) == false) {
											bool res = RegisterForm(record);
											if (res) {
												stats._Generation++;
											} else {
												stats._Fail++;
												logcritical("Failed Record:    Generation");
											}
										} else {
											stats._Fail++;
											logcritical("Deleted Record:    Generation");
										}
									}
									break;
								default:
									stats._Fail++;
									logcritical("Trying to read unknown formtype");
								}
								//if (cbuf)
								//	delete[] cbuffer;

							}
							// update progress
							_actionloadsave_current++;
						}
						_loaded = true;
						loginfo("Loaded save");
					}
					break;
				default:  // unknown save file version
					logcritical("Save file version is unknown");
					break;
				}
				if (sbuf != nullptr) {
					delete sbuf;
					sbuf = nullptr;
				}
			}
		} else {
			// this cannot be our savefile
			logcritical("Save file does not have the proper format: fail guid");
		}
		delete[] buffer;
		fsave.close();
		std::cout << "hashtable size: " << _hashmap.size() << "\n";
		if (!_lresolve->_oracle)
		{
			logcritical("Didn't read oracle, catastrpic fail.");
			_lresolve->_oracle = CreateForm<Oracle>();
		}

		_status = "Resolving Records...";
		loginfo("Resolving records.");
		_actionloadsave_max = _lresolve->TaskCount();
		_actionloadsave_current = 0;
		_lresolve->Resolve(_actionloadsave_current);

		loginfo("Resolved records.");
		loginfo("Resolving late records.");

		// before we resolve late tasks, we need to register ourselves to the lua wrapper, if some of the lambda
		// functions want to use lua scripts
		auto sessdata = CreateForm<SessionData>();
		sessdata->_oracle = CreateForm<Oracle>();
		bool registeredLua = Lua::RegisterThread(sessdata);  // session is already fully loaded for the most part, so we can use the command
		_lresolve->ResolveLate(_actionloadsave_current);
		// unregister ourselves from the lua wrapper if we registered ourselves above
		if (registeredLua)
			Lua::UnregisterThread();
		// add savecallback to taskcontroller
		if (callback) {
			sessdata->_controller->AddTask(callback);
		}

		loginfo("Resolved late records.");
		loginfo("Loaded session");
	} else
		logcritical("Cannot open savefile");

	loginfo("Loaded Records:");
	loginfo("Input: {}", stats._Input);
	loginfo("Grammar: {}", stats._Grammar);
	loginfo("DerivationTree: {}", stats._DevTree);
	loginfo("ExclusionTree: {}", stats._ExclTree);
	loginfo("Generator: {}", stats._Generator);
	loginfo("Session: {}", stats._Session);
	loginfo("Settings: {}", stats._Settings);
	loginfo("Test: {}", stats._Test);
	loginfo("TaskController: {}", stats._TaskController);
	loginfo("ExecutionHandler: {}", stats._ExecutionHandler);
	loginfo("Oracle: {}", stats._Oracle);
	loginfo("Fails: {}", stats._Fail);

	_actionloadsave = false;
	_status = "Running...";
	profile(TimeProfiling, "Loading Save");
}


LoadResolver* LoadResolver::GetSingleton()
{
	static LoadResolver resolver;
	return std::addressof(resolver);
}

void Data::StartClock()
{
	_sessionBegin = std::chrono::steady_clock::now();
}

void LoadResolver::AddTask(TaskFn a_task)
{
	AddTask(new Task(std::move(a_task)));
}

void LoadResolver::AddTask(TaskDelegate* a_task)
{
	{
		std::unique_lock<std::mutex> guard(_lock);
		_tasks.push(a_task);
	}
}

void LoadResolver::AddLateTask(TaskFn a_task)
{
	AddLateTask(new Task(std::move(a_task)));
}

void LoadResolver::AddLateTask(TaskDelegate* a_task)
{
	{
		std::unique_lock<std::mutex> guard(_lock);
		_latetasks.push(a_task);
	}
}

void LoadResolver::SetData(Data* dat)
{
	_data = dat;
}

void LoadResolver::Resolve(uint64_t& progress)
{
	StartProfiling;
	while (!_tasks.empty()) {
		TaskDelegate* del;
		del = _tasks.front();
		_tasks.pop();
		del->Run();
		del->Dispose();
		progress++;
	}
	profile(TimeProfiling, "Performing Post-load operations");
}

void LoadResolver::ResolveLate(uint64_t& progress)
{
	StartProfiling;
	while (!_latetasks.empty()) {
		TaskDelegate* del;
		del = _latetasks.front();
		_latetasks.pop();
		del->Run();
		del->Dispose();
		progress++;
	}
	profile(TimeProfiling, "Performing Post-load operations");
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

template <>
std::shared_ptr<SessionData> Data::CreateForm()
{
	std::shared_ptr<SessionData> ptr;
	{
		std::unique_lock<std::shared_mutex> guard(_hashmaplock);
		auto itr = _hashmap.find(StaticFormIDs::SessionData);
		if (itr != _hashmap.end())
			ptr = dynamic_pointer_cast<SessionData>(itr->second);
	}
	if (ptr)
		return ptr;
	else {
		ptr = std::make_shared<SessionData>();
		ptr->data = this;
		FormID formid = StaticFormIDs::SessionData;
		ptr->SetFormID(formid);
		{
			std::unique_lock<std::shared_mutex> guard(_hashmaplock);
			_hashmap.insert({ formid, dynamic_pointer_cast<Form>(ptr) });
		}
		return ptr;
	}
}

std::unordered_map<FormID, std::weak_ptr<Form>> Data::GetWeakHashmapCopy()
{
	std::unordered_map<FormID, std::weak_ptr<Form>> hashweak;
	{
		std::shared_lock<std::shared_mutex> guard(_hashmaplock);
		for (auto& [id, form] : _hashmap)
		{
			hashweak.insert({ id, std::weak_ptr<Form>(form) });
		}
	}
	return hashweak;
}
size_t Data::GetHashmapSize()
{
	return _hashmap.size();
}

FormID Data::GetIDFromString(std::string str)
{
	//_stringHashmapLock.lock_shared();
	//boost::upgrade_lock<boost::upgrade_mutex> guard(_stringHashmapLock);
	auto itr = _stringHashmap.right.find(str);
	if (itr != _stringHashmap.right.end()) {
		return itr->second;
	} else {
		//boost::upgrade_to_unique_lock<boost::upgrade_mutex> guardunique(guard);
		std::unique_lock<std::mutex> guard(_stringHashmapLock);
		FormID formid = 0;
		{
			formid = _stringNextFormID++;
		}
		_stringHashmap.insert(StringHashmap::value_type(formid, str));
		return formid;
	}
}
std::pair<std::string, bool> Data::GetStringFromID(FormID id)
{
	//std::shared_lock<std::shared_mutex> guard(_stringHashmapLock);
	auto itr = _stringHashmap.left.find(id);
	if (itr != _stringHashmap.left.end()) {
		return { itr->second, true };
	} else
		return { "", false };
}

size_t Data::GetStringHashmapSize()
{
	size_t size = 0;
	size += 4; // version
	size += 8; // number of entries
	for (auto [id, str] : _stringHashmap.left)
	{
		size += 8, // id
		size += Buffer::CalcStringLength(str); // string
	}
	return size;
}

bool Data::WriteStringHashmap(std::ostream* buffer, size_t& offset, size_t)
{
	static int32_t version = 0x1;
	Buffer::Write(version, buffer, offset);
	Buffer::WriteSize(_stringHashmap.left.size(), buffer, offset);
	for (auto [id, str] : _stringHashmap.left)
	{
		Buffer::Write(id, buffer, offset);
		Buffer::Write(str, buffer, offset);
	}
	return true;
}

bool Data::ReadStringHashmap(std::istream* buffer, size_t& offset, size_t length)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version)
	{
	case 0x1:
		size_t num = Buffer::ReadSize(buffer, offset);
		for (size_t i = 0; i < num; i++)
		{
			// do this as the resolve direction of functions in another function call is from right to left
			// which would seriously mess up the buffer
			FormID id = Buffer::ReadUInt64(buffer, offset);
			std::string str = Buffer::ReadString(buffer, offset);
			_stringHashmap.insert(StringHashmap::value_type(id, str));
			if (offset >= length)
				break;
		}
		return true;
	}
	return false;
}

void Data::Visit(std::function<VisitAction(std::shared_ptr<Form>)> visitor)
{
	bool writelock = false;
	_hashmaplock.lock_shared();
	for (auto& [id, form] : _hashmap)
	{
		switch (visitor(form))
		{
		case VisitAction::None:
			break;
		case VisitAction::DeleteForm:
			if (!writelock)
			{
				_hashmaplock.unlock_shared();
				_hashmaplock.lock();
				writelock = true;
			}
			_hashmap.erase(form->GetFormID());
			form->Delete(this);
		}
	}
	if (writelock)
		_hashmaplock.unlock();
	else
		_hashmaplock.unlock_shared();
}

void Data::Clear()
{
	_actionloadsave = true;
	_status = "Clearing hashmap...";
	//if (_lresolve != nullptr) {
	//	delete _lresolve;
	//}
	delete _lresolve;
	std::unique_lock<std::shared_mutex> guard(_hashmaplock);
	_actionloadsave_max = _hashmap.size();
	_actionloadsave_current = 0;
	for (auto& [id, form] : _hashmap) {
		form->SetFormID(0);
		if (form->GetType() != FormType::Session)
			form->Clear();
		_actionloadsave_current++;
	}
	_hashmap.clear();
	for (auto& [id, stor] : _objectRecycler)
		delete stor;
	_objectRecycler.clear();
	_stringHashmap.clear();
	_actionloadsave = false;
}
