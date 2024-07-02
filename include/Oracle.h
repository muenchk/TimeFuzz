#pragma once

#include <filesystem>
#include <tuple>

#include "ExecutionHandler.h"

class Oracle
{
public:
	enum OracleResult : EnumType
	{
		/// <summary>
		/// Program has not finished
		/// </summary>
		Unfinished = 0b0,
		/// <summary>
		/// The result is passing
		/// </summary>
		Passing = 0b1,
		/// <summary>
		/// The result is failing
		/// </summary>
		Failing = 0b10,
		/// <summary>
		/// The result cannot be identified
		/// </summary>
		Undefined = 0b100,
		/// <summary>
		/// Any input that has this input as prefix, will produce the same result
		/// </summary>
		Prefix = 0b1000000,
	};

	enum class OracleType
	{
		Undefined,
		/// <summary>
		/// executes a python script that execute the PUT and collects information about the result and the last executed element of the input sequence
		/// </summary>
		PythonScript,
		CommandlineProgramResponsive,
		CommandlineProgramDump,
	};

private:
	std::filesystem::path _path;
	OracleType _type;
	bool valid = false;

public:

	Oracle(OracleType type, std::filesystem::path oraclepath);
	bool Validate();

	OracleResult Evaluate(Test* test);

	static std::string TypeString(OracleType type);
	static OracleType ParseType(std::string str);
	inline std::filesystem::path path()
	{
		return _path;
	}

};
