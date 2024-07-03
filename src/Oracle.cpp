#include "Logging.h"
#include "Oracle.h"
#include "Utility.h"

std::string Oracle::TypeString(OracleType type)
{
	return type == Oracle::OracleType::PythonScript                 ? "PythonScript" :
	       type == Oracle::OracleType::CommandlineProgramResponsive ? "cmdResponsive" :
	       type == Oracle::OracleType::CommandlineProgramDump       ? "cmdDump" :
	                                                                  "undefined";
}

Oracle::OracleType Oracle::ParseType(std::string str)
{
	if (Utility::ToLower(str) == "pythonscript")
		return Oracle::OracleType::PythonScript;
	else if (Utility::ToLower(str) == "cmdresponsive")
		return Oracle::OracleType::CommandlineProgramResponsive;
	else if (Utility::ToLower(str) == "cmddump")
		return Oracle::OracleType::CommandlineProgramDump;
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
