#include "Logging.h"
#include "Oracle.h"
#include "Utility.h"
#include "BufferOperations.h"
#include "Input.h"
#include "Data.h"


std::string Oracle::TypeString(PUTType type)
{
	return type == Oracle::PUTType::CMD                 ? "CMD" :
	       type == Oracle::PUTType::Script              ? "Script" :
	       type == Oracle::PUTType::STDIN_Responsive    ? "StdinResponsive" :
	       type == Oracle::PUTType::STDIN_Responsive ? "StdinDump" :
	                                                      "undefined";
}

Oracle::PUTType Oracle::ParseType(std::string str)
{
	if (Utility::ToLower(str).find("cmd") != std::string::npos)
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
			_valid = true;
			return;
		}
		_valid = false;
		return;
		#else
		// linux file not found, etc... just exit
		logcritical("Cannot check for Oracle: {}", e.what());
		_valid = false;
		return;
		#endif
	}
	_valid = true;
}

bool Oracle::Validate()
{
	try {
		if (!std::filesystem::exists(_path)) {
			logwarn("Oracle cannot be initialised with a non-existent path: {}", _path.string());
			_valid = false;
			return _valid;
		}
		if (std::filesystem::is_directory(_path)) {
			logwarn("Oracle cannot be initialised with a directory: {}", _path.string());
			_valid = false;
			return _valid;
		}
		if (std::filesystem::is_symlink(_path)) {
			logwarn("Oracle cannot be initialsed with a symlink: {}", _path.string());
			_valid = false;
			return _valid;
		}
		if (std::filesystem::is_regular_file(_path) == false) {
			logwarn("Oracle cannot be initialised on an unsupported filetype: {}", _path.string());
			_valid = false;
			return _valid;
		}
		if (std::filesystem::is_empty(_path)) {
			logwarn("Oracle cannot be initialised on an empty file: {}", _path.string());
			_valid = false;
			return _valid;
		}
	} catch (std::filesystem::filesystem_error& e) {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
		// windows when checking certain pathes the files cannot be accessed and throw error, for instance python installation from the store
		// so we just continue if we get that specific error without outputting any errors and settings us to invalid
		if (e.code().value() == ERROR_CANT_ACCESS_FILE) {
			_valid = true;
			return _valid;
		}
		_valid = false;
		return _valid;
#else
		// linux file not found, etc... just exit
		logcritical("Cannot check for Oracle: {}", e.what());
		_valid = false;
		return _valid;
#endif
	}
	return _valid;
}

void Oracle::SetLuaCmdArgs(std::string script)
{
	_luaCmdArgsStr = script;
}

void Oracle::SetLuaCmdArgs(std::filesystem::path scriptpath)
{
	_luaCmdArgsPath = scriptpath;
}

void Oracle::SetLuaCmdArgsReplay(std::filesystem::path scriptpath)
{
	_luaCmdArgsPathReplay = scriptpath;
}

void Oracle::SetLuaOracle(std::string script)
{
	_luaOracleStr = script;
}

void Oracle::SetLuaOracle(std::filesystem::path scriptpath)
{
	_luaOraclePath = scriptpath;
}

void Oracle::SetLuaScriptArgs(std::string script)
{
	_luaScriptArgsStr = script;
}

void Oracle::SetLuaScriptArgs(std::filesystem::path scriptpath)
{
	_luaScriptArgsPath = scriptpath;
}

void Oracle::ApplyLuaCommands(lua_State* L)
{
	if (_luaCmdArgsStr.empty() == false) {
		// the lua command is saved in a string
		if (luaL_dostring(L, _luaCmdArgsStr.c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		}
	} else {
		// run lua on script
		if (luaL_dofile(L, _luaCmdArgsPath.string().c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		} else {
			std::string err = lua_tostring(L, -1);
			lua_pop(L, 1);
			logcritical("Lua Error in CmdArgs: {}", err);
			logwarn("Cmd Args lua Script could not be read successfully")
		}
	}

	if (_luaOracleStr.empty() == false) {
		// the lua command is saved in a string
		if (luaL_dostring(L, _luaOracleStr.c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		}
	} else {
		// run lua on script
		if (luaL_dofile(L, _luaOraclePath.string().c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		} else {
			std::string err = lua_tostring(L, -1);
			lua_pop(L, 1);
			logcritical("Lua Error in Oracle: {}", err);
			logwarn("Oracle lua Script could not be read successfully")
		}
	}

	if (std::filesystem::exists(_luaCmdArgsPathReplay))
		if (luaL_dofile(L, _luaCmdArgsPathReplay.string().c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		}

	if (_luaScriptArgsStr.empty() == false) {
		// the lua command is saved in a string
		if (luaL_dostring(L, _luaScriptArgsStr.c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		}
	} else {
		// run lua on script
		if (luaL_dofile(L, _luaScriptArgsPath.string().c_str()) == LUA_OK) {
			lua_pop(L, lua_gettop(L));
		} else {
			std::string err = lua_tostring(L, -1);
			lua_pop(L, 1);
			logcritical("Lua Error in ScriptArgs: {}", err);
			logwarn("Script Args lua Script could not be read successfully")
		}
	}
}

OracleResult Oracle::Evaluate(lua_State* L, std::shared_ptr<Test> test)
{
	StartProfiling;
	profile(TimeProfiling, "{}: begin oracle", Utility::PrintForm(test));
	auto input = test->_input.lock();
	OracleResult result = OracleResult::None;
	if (input)
	{
		lua_pushlightuserdata(L, (void*)(input.get()));
		lua_setglobal(L, "input");

		lua_getglobal(L, "Oracle");

		if (lua_isfunction(L, -1)) {
			if (lua_pcall(L, 0, 1, 0) == LUA_OK)  // execute function with 0 Arguments, 1 return value
			{
				if (lua_isnumber(L, -1)) {
					result = (OracleResult)luaL_checkinteger(L, -1);
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
	profile(TimeProfiling, "{}: calcualted oracle", Utility::PrintForm(test));

	return result;
}

std::string Oracle::GetCmdArgs(lua_State* L, Test* test, bool replay)
{
	//std::unique_lock<std::mutex> guard(cmdlock);
	std::string args = "";
	auto input = test->_input.lock();
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

std::string Oracle::GetScriptArgs(lua_State* L, Test* test)
{
	//std::unique_lock<std::mutex> guard(cmdlock);
	std::string args = "";
	auto input = test->_input.lock();
	if (input) {
		lua_pushlightuserdata(L, (void*)(input.get()));
		lua_setglobal(L, "input");
		lua_getglobal(L, "GetScriptArgs");
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
			} else {
				std::string err = lua_tostring(L, -1);
				lua_pop(L, 1);
				logcritical("Lua Error in GetScriptArgs: {}", err);
			}
		}
		while (lua_gettop(L) != 0)
			lua_pop(L, 1);
	}
	return args;
}

size_t Oracle::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4                     // version
	                        + 4                     // type
	                        + 1                     // valid
	                        + 8                     // LoracleStr size
	                        + 8                     // LoraclePath size
	                        + 8                     // LcmdargsStr size
	                        + 8                     // LcmdargsPath size
	                        + 8                     // LscriptargsStr size
	                        + 8;                    // LscriptargsPath size
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
	return Form::GetDynamicSize()  // form stuff
	       + GetStaticSize(classversion) + Buffer::CalcStringLength(_path.string()) + Buffer::CalcStringLength(_luaOracleStr) + Buffer::CalcStringLength(_luaOraclePath.string()) + Buffer::CalcStringLength(_luaCmdArgsStr) + Buffer::CalcStringLength(_luaCmdArgsPath.string()) + Buffer::CalcStringLength(_luaScriptArgsStr) + Buffer::CalcStringLength(_luaScriptArgsPath.string());
}

bool Oracle::WriteData(std::ostream* buffer, size_t& offset, size_t length)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset, length);
	Buffer::Write((int32_t)_type, buffer, offset);
	Buffer::Write(_valid, buffer, offset);
	Buffer::Write(_path.string(), buffer, offset);
	Buffer::Write(_luaOracleStr, buffer, offset);
	if (std::filesystem::exists(_luaOraclePath))
		Buffer::Write(_luaOraclePath.string(), buffer, offset);
	else
		Buffer::Write(std::string(""), buffer, offset);
	Buffer::Write(_luaCmdArgsStr, buffer, offset);
	if (std::filesystem::exists(_luaCmdArgsPath))
		Buffer::Write(_luaCmdArgsPath.string(), buffer, offset);
	else
		Buffer::Write(std::string(""), buffer, offset);
	Buffer::Write(_luaScriptArgsStr, buffer, offset);
	if (std::filesystem::exists(_luaScriptArgsPath))
		Buffer::Write(_luaScriptArgsPath.string(), buffer, offset);
	else
		Buffer::Write(std::string(""), buffer, offset);
	return true;
}

bool Oracle::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			_type = (PUTType)Buffer::ReadInt32(buffer, offset);
			_valid = Buffer::ReadBool(buffer, offset);
			std::string path = Buffer::ReadString(buffer, offset);
			_path = std::filesystem::path(path);
			_luaOracleStr = Buffer::ReadString(buffer, offset);
			if (_luaOracleStr.empty() == false)
				SetLuaOracle(_luaOracleStr);
			path = Buffer::ReadString(buffer, offset);
			if (path.empty() == false)
				SetLuaOracle(std::filesystem::path(path));
			_luaCmdArgsStr = Buffer::ReadString(buffer, offset);
			if (_luaCmdArgsStr.empty() == false)
				SetLuaCmdArgs(_luaCmdArgsStr);
			path = Buffer::ReadString(buffer, offset);
			if (path.empty() == false)
				SetLuaCmdArgs(std::filesystem::path(path));
			_luaScriptArgsStr = Buffer::ReadString(buffer, offset);
			if (_luaScriptArgsStr.empty() == false)
				SetLuaScriptArgs(_luaScriptArgsStr);
			path = Buffer::ReadString(buffer, offset);
			if (path.empty() == false)
				SetLuaScriptArgs(std::filesystem::path(path));
			// init lua from settings to support cross-platform stuff out of the boc
			resolver->AddTask([this, resolver]() {
				auto sett = resolver->ResolveFormID<Settings>(Data::StaticFormIDs::Settings);
				Set(sett->oracle.oracle, sett->GetOraclePath());
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
				path = std::filesystem::path(sett->oracle.lua_path_script);
				if (!std::filesystem::exists(path)) {
					logcritical("Lua ScriptArgs script cannot be found.");
				}
				SetLuaScriptArgs(path);
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
	Form::ClearForm();
	_luaOracleStr = "";
	_luaOraclePath = "";
	_luaCmdArgsStr = "";
	_luaCmdArgsPath = "";
	_valid = false;
	_path = "";
}

void Oracle::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}

size_t Oracle::MemorySize()
{
	return sizeof(Oracle) + sizeof(_luaScriptArgsPath) + sizeof(_luaCmdArgsPath) + sizeof(_luaOraclePath) + sizeof(_path);
}
