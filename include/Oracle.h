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
		/// <summary>
		/// Invalid value
		/// </summary>
		Undefined,
		/// <summary>
		/// Arguments are passed on the command line [no fragment evaluation]
		/// </summary>
		CMD,
		/// <summary>
		/// Arguments are passed on the command line
		/// [STDOUT returns formatted execution information]
		/// </summary>
		Script,
		/// <summary>
		/// Arguments are passed as fragments on STDIN
		/// </summary>
		STDIN_Responsive,
		/// <summary>
		/// Arguments are dumped on STDIN [no fragment evaluation
		/// </summary>
		STDIN_Dump,
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
