#include "Logging.h"
#include "Oracle.h"
#include "Utility.h"
#include "BufferOperations.h"


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
	if (Utility::ToLower(str) == "CommandLine")
		return Oracle::PUTType::CMD;
	else if (Utility::ToLower(str) == "Script")
		return Oracle::PUTType::Script;
	else if (Utility::ToLower(str) == "StdinResponsive")
		return Oracle::PUTType::STDIN_Responsive;
	else if (Utility::ToLower(str) == "StdinDump")
		return Oracle::PUTType::STDIN_Dump;
	else
		return Oracle::PUTType::Undefined;
}

Oracle::Oracle()
{
	luas = luaL_newstate();
	luaL_openlibs(luas);
}

Oracle::~Oracle()
{
	lua_close(luas);
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
	return valid;
}

void Oracle::SetLuaCmdArgs(std::string script)
{
	LcmdargsStr = script;
	// the lua command is saved in a string
	if (luaL_dostring(luas, LcmdargsStr.c_str()) == LUA_OK) {
		lua_pop(luas, lua_gettop(luas));
	}
}

void Oracle::SetLuaCmdArgs(std::filesystem::path scriptpath)
{
	LcmdargsPath = scriptpath;

	// run lua on script
	if (luaL_dofile(luas, scriptpath.string().c_str()) == LUA_OK) {
		lua_pop(luas, lua_gettop(luas));
	}
}

Oracle::OracleResult Oracle::Evaluate(std::shared_ptr<Test> test)
{
	test->input;
	return Oracle::OracleResult::Passing;
}

std::string Oracle::GetCmdArgs(std::shared_ptr<Test> test)
{
	std::unique_lock<std::mutex> guard(cmdlock);
	std::string args = "";
	auto input = test->input.lock();
	if (input) {
		lua_getglobal(luas, "GetCmdArgs");
		//lua_push .....
		// set function call args

		if (lua_isfunction(luas, -1)) {
			if (lua_pcall(luas, 0, 1, 0) == LUA_OK)  // execute function with 0 Arguments, 1 return value
			{
				if (lua_isstring(luas, -1)) {
					const char* cmd = lua_tostring(luas, -1);
					lua_pop(luas, 1);
					args = std::string(cmd);
				}
				lua_pop(luas, lua_gettop(luas));
			}
		}
	}
	return args;
}

size_t Oracle::GetStaticSize(int32_t version)
{
	static size_t size0x1 = Form::GetDynamicSize()  // form base size
	                        + 4                     // version
	                        + 4                     // type
	                        + 1;                    // valid
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
	return GetStaticSize(classversion) + Buffer::CalcStringLength(_path.string());
}

bool Oracle::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write((int32_t)_type, buffer, offset);
	Buffer::Write(valid, buffer, offset);
	Buffer::Write(_path.string(), buffer, offset);
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
			Set(_type, _path);
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
