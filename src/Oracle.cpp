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

Oracle::Oracle(PUTType type, std::filesystem::path PUTpath)
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

Oracle::OracleResult Oracle::Evaluate(Test* test)
{
	test->input;
	return Oracle::OracleResult::Passing;
}


size_t Oracle::GetDynamicSize()
{
	static size_t size0x1 = 4     // version
	                 + 4   // type
	                 + 1;  // valid
	return size0x1 + Buffer::CalcStringLength(_path.string());
}

bool Oracle::WriteData(unsigned char* buffer, size_t offset)
{
	Buffer::Write(classversion, buffer, offset);
	Buffer::Write((int32_t)_type, buffer, offset);
	Buffer::Write(valid, buffer, offset);
	Buffer::Write(_path.string(), buffer, offset);
	return true;
}

bool Oracle::ReadData(unsigned char* buffer, size_t offset, size_t length)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			_type = (PUTType)Buffer::ReadInt32(buffer, offset);
			valid = Buffer::ReadBool(buffer, offset);
			_path = std::filesystem::path(Buffer::ReadString(buffer, offset));
		}
		break;
	default:
		return false;
	}
}
