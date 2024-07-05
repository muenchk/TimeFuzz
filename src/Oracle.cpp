#include "Logging.h"
#include "Oracle.h"
#include "Utility.h"

std::string Oracle::TypeString(OracleType type)
{
	return type == Oracle::OracleType::CMD              ? "CommandLine" :
	       type == Oracle::OracleType::Script           ? "Script" :
	       type == Oracle::OracleType::STDIN_Responsive ? "StdinResponsive" :
	       type == Oracle::OracleType::STDIN_Responsive ? "StdinDump" :
	                                                      "undefined";
}

Oracle::OracleType Oracle::ParseType(std::string str)
{
	if (Utility::ToLower(str) == "CommandLine")
		return Oracle::OracleType::CMD;
	else if (Utility::ToLower(str) == "Script")
		return Oracle::OracleType::Script;
	else if (Utility::ToLower(str) == "StdinResponsive")
		return Oracle::OracleType::STDIN_Responsive;
	else if (Utility::ToLower(str) == "StdinDump")
		return Oracle::OracleType::STDIN_Dump;
	else
		return Oracle::OracleType::Undefined;
}

Oracle::Oracle(OracleType type, std::filesystem::path oraclepath)
{
	_type = type;
	_path = oraclepath;
	if (!std::filesystem::exists(_path))
	{
		logwarn("Oracle cannot be initialised with a non-existent path: {}", _path.string());
		return;
	}
	if (std::filesystem::is_directory(_path))
	{
		logwarn("Oracle cannot be initialised with a directory: {}", _path.string());
		return;
	}
	if (std::filesystem::is_symlink(_path))
	{
		logwarn("Oracle cannot be initialsed with a symlink: {}", _path.string());
		return;
	}
	if (std::filesystem::is_regular_file(_path) == false)
	{
		logwarn("Oracle cannot be initialised on an unsupported filetype: {}", _path.string());
		return;
	}
	if (std::filesystem::is_empty(_path))
	{
		logwarn("Oracle cannot be initialised on an empty file: {}", _path.string());
		return;
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
