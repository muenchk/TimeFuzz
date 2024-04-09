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
