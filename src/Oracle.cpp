#include "Logging.h"
#include "Oracle.h"
#include "Utility.h"
#include "BufferOperations.h"
#include "Input.h"
#include "Data.h"


std::string Oracle::TypeString(PUTType type)
{
	return type == Oracle::PUTType::CMD                 ? "CommandLine" :
	       type == Oracle::PUTType::Script              ? "Script" :
	       type == Oracle::PUTType::STDIN_Responsive    ? "StdinResponsive" :
	       type == Oracle::PUTType::STDIN_Responsive ? "StdinDump" :
	                                                      "undefined";
}

Oracle::PUTType Oracle::ParseType(std::string str)
{
	if (Utility::ToLower(str).find("commandline") != std::string::npos)
		return Oracle::PUTType::CMD;
	else if (Utility::ToLower(str).find("script") != std::string::npos)
		return Oracle::PUTType::Script;
	else if (Utility::ToLower(str).find("stdinresponsive") != std::string::npos)
		return Oracle::PUTType::STDIN_Responsive;
	else if (Utility::ToLower(str).find("stdindump") != std::string::npos)
		return Oracle::PUTType::STDIN_Dump;
	else
		return Oracle::PUTType::Undefined;
}

Oracle::Oracle()
{
}

Oracle::~Oracle()
{
}

void Oracle::Set(PUTType type, std::filesystem::path PUTpath)
{
	_type = type;
	_path = PUTpath;
	try {
		if (!std::filesystem::exists(_path)) {
			logwarn("Oracle cannot be initialised with a non-existent path: {}", _path.string());
			return;
		}
		if (std::filesystem::is_directory(_path)) {
			logwarn("Oracle cannot be initialised with a directory: {}", _path.string());
			return;
		}
		if (std::filesystem::is_symlink(_path)) {
			logwarn("Oracle cannot be initialsed with a symlink: {}", _path.string());
			return;
		}
		if (std::filesystem::is_regular_file(_path) == false) {
			logwarn("Oracle cannot be initialised on an unsupported filetype: {}", _path.string());
			return;
		}
		if (std::filesystem::is_empty(_path)) {
			logwarn("Oracle cannot be initialised on an empty file: {}", _path.string());
			return;
		}
	}
	catch (std::filesystem::filesystem_error& e) {
		#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
		// windows when checking certain pathes the files cannot be accessed and throw error, for instance python installation from the store
		// so we just continue if we get that specific error without outputting any errors and settings us to invalid
		if (e.code().value() == ERROR_CANT_ACCESS_FILE)
		{
			valid = true;
			return;
		}
		valid = false;
		return;
		#else
		// linux file not found, etc... just exit
		logcritical("Cannot check for Oracle: {}", e.what());
		valid = false;
		return;
		#endif
	}
	valid = true;
}

bool Oracle::Validate()
{
	try {
		if (!std::filesystem::exists(_path)) {
			logwarn("Oracle cannot be initialised with a non-existent path: {}", _path.string());
			valid = false;
			return valid;
		}
		if (std::filesystem::is_directory(_path)) {
			logwarn("Oracle cannot be initialised with a directory: {}", _path.string());
			valid = false;
			return valid;
		}
		if (std::filesystem::is_symlink(_path)) {
			logwarn("Oracle cannot be initialsed with a symlink: {}", _path.string());
			valid = false;
			return valid;
		}
		if (std::filesystem::is_regular_file(_path) == false) {
			logwarn("Oracle cannot be initialised on an unsupported filetype: {}", _path.string());
			valid = false;
			return valid;
		}
		if (std::filesystem::is_empty(_path)) {
			logwarn("Oracle cannot be initialised on an empty file: {}", _path.string());
			valid = false;
			return valid;
		}
	} catch (std::filesystem::filesystem_error& e) {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
		// windows when checking certain pathes the files cannot be accessed and throw error, for instance python installation from the store
		// so we just continue if we get that specific error without outputting any errors and settings us to invalid
		if (e.code().value() == ERROR_CANT_ACCESS_FILE) {
			valid = true;
			return valid;
		}
		valid = false;
		return valid;
#else
		// linux file not found, etc... just exit
		logcritical("Cannot check for Oracle: {}", e.what());
		valid = false;
		return valid;
#endif
	}
	return valid;
}

void Oracle::SetLuaCmdArgs(std::string script)
{
	LcmdargsStr = script;
}

void Oracle::SetLuaCmdArgs(std::filesystem::path scriptpath)
{
	LcmdargsPath = scriptpath;
}

void Oracle::SetLuaCmdArgsReplay(std::filesystem::path scriptpath)
{
	LcmdargsPathReplay = scriptpath;
}

void Oracle::SetLuaOracle(std::string script)
{
	LoracleStr = script;
}

void Oracle::SetLuaOracle(std::filesystem::path scriptpath)
{
	LoraclePath = scriptpath;
}

void Oracle::ApplyLuaCommands(lua_State* L)
{
	if (LcmdargsStr.empty() == false) {
		// the lua command is saved in a string
		if (luaL_dostring(L, LcmdargsStr.c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		}
	} else {
		// run lua on script
		if (luaL_dofile(L, LcmdargsPath.string().c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		}
	}

	if (LoracleStr.empty() == false) {
		// the lua command is saved in a string
		if (luaL_dostring(L, LoracleStr.c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		}
	} else {
		// run lua on script
		if (luaL_dofile(L, LoraclePath.string().c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		}
	}

	if (std::filesystem::exists(LcmdargsPathReplay))
		if (luaL_dofile(L, LcmdargsPathReplay.string().c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		}
}

Oracle::OracleResult Oracle::Evaluate(lua_State* L, std::shared_ptr<Test> test)
{
	auto input = test->input.lock();
	Oracle::OracleResult result = Oracle::OracleResult::None;
	if (input)
	{
		lua_pushlightuserdata(L, (void*)(input.get()));
		lua_setglobal(L, "input");

		lua_getglobal(L, "Oracle");

		if (lua_isfunction(L, -1)) {
			if (lua_pcall(L, 0, 1, 0) == LUA_OK)  // execute function with 0 Arguments, 1 return value
			{
				if (lua_isinteger(L, -1)) {
					result = (Oracle::OracleResult)luaL_checkinteger(L, -1);
					lua_pop(L, 1);
				}
				lua_pop(L, lua_gettop(L));
			} else {
				std::string err = lua_tostring(L, -1);
				lua_pop(L, 1);
				logcritical("Lua Error in Oracle: {}", err);
			}
		}
		while (lua_gettop(L) != 0)
			lua_pop(L, 1);
	}

	return result;
}

std::string Oracle::GetCmdArgs(lua_State* L, Test* test, bool replay)
{
	//std::unique_lock<std::mutex> guard(cmdlock);
	std::string args = "";
	auto input = test->input.lock();
	if (input) {
		lua_pushlightuserdata(L, (void*)(input.get()));
		lua_setglobal(L, "input");
		if (replay)
			lua_getglobal(L, "GetCmdArgsReplay");
		else
			lua_getglobal(L, "GetCmdArgs");
		//lua_push .....
		// set function call args

		if (lua_isfunction(L, -1)) {
			if (lua_pcall(L, 0, 1, 0) == LUA_OK)  // execute function with 0 Arguments, 1 return value
			{
				if (lua_isstring(L, -1)) {
					const char* cmd = lua_tostring(L, -1);
					lua_pop(L, 1);
					args = std::string(cmd);
				}
				lua_pop(L, lua_gettop(L));
			}
			else
			{
				std::string err = lua_tostring(L, -1);
				lua_pop(L, 1);
				logcritical("Lua Error in GetCmdArgs: {}", err);
			}
		}
		while (lua_gettop(L) != 0)
			lua_pop(L, 1);
	}
	return args;
}

size_t Oracle::GetStaticSize(int32_t version)
{
	static size_t size0x1 = Form::GetDynamicSize()  // form base size
	                        + 4                     // version
	                        + 4                     // type
	                        + 1                     // valid
	                        + 8                     // LoracleStr size
	                        + 8                     // LoraclePath size
	                        + 8                     // LcmdargsStr size
	                        + 8;                    // LcmdargsPath size
	switch (version)
	{
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t Oracle::GetDynamicSize()
{
	return GetStaticSize(classversion) + Buffer::CalcStringLength(_path.string()) + Buffer::CalcStringLength(LoracleStr) + Buffer::CalcStringLength(LoraclePath.string()) + Buffer::CalcStringLength(LcmdargsStr) + Buffer::CalcStringLength(LcmdargsPath.string());
}

bool Oracle::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write((int32_t)_type, buffer, offset);
	Buffer::Write(valid, buffer, offset);
	Buffer::Write(_path.string(), buffer, offset);
	Buffer::Write(LoracleStr, buffer, offset);
	if (std::filesystem::exists(LoraclePath))
		Buffer::Write(LoraclePath.string(), buffer, offset);
	else
		Buffer::Write(std::string(""), buffer, offset);
	Buffer::Write(LcmdargsStr, buffer, offset);
	if (std::filesystem::exists(LcmdargsPath))
		Buffer::Write(LcmdargsPath.string(), buffer, offset);
	else
		Buffer::Write(std::string(""), buffer, offset);
	return true;
}

bool Oracle::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			_type = (PUTType)Buffer::ReadInt32(buffer, offset);
			valid = Buffer::ReadBool(buffer, offset);
			std::string path = Buffer::ReadString(buffer, offset);
			_path = std::filesystem::path(path);
			LoracleStr = Buffer::ReadString(buffer, offset);
			if (LoracleStr.empty() == false)
				SetLuaOracle(LoracleStr);
			path = Buffer::ReadString(buffer, offset);
			if (path.empty() == false)
				SetLuaOracle(std::filesystem::path(path));
			LcmdargsStr = Buffer::ReadString(buffer, offset);
			if (LcmdargsStr.empty() == false)
				SetLuaCmdArgs(LcmdargsStr);
			path = Buffer::ReadString(buffer, offset);
			if (path.empty() == false)
				SetLuaCmdArgs(std::filesystem::path(path));
			// init lua from settings to support cross-platform stuff out of the boc
			resolver->AddTask([this, resolver]() {
				auto sett = resolver->ResolveFormID<Settings>(Data::StaticFormIDs::Settings);
				auto path = std::filesystem::path(sett->oracle.lua_path_cmd);
				if (!std::filesystem::exists(path)) {
					logcritical("Lua CmdArgs script cannot be found.");
				}
				SetLuaCmdArgs(path);
				path = std::filesystem::path(sett->oracle.lua_path_oracle);
				if (!std::filesystem::exists(path)) {
					logcritical("Lua Oracle script cannot be found.");
				}
				SetLuaOracle(path);
			});
			return true;
		}
		break;
	default:
		return false;
	}
}

void Oracle::Delete(Data*)
{
	Clear();
}

void Oracle::Clear()
{
	LoracleStr = "";
	LoraclePath = "";
	LcmdargsStr = "";
	LcmdargsPath = "";
	valid = false;
	_path = "";
}

void Oracle::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}
