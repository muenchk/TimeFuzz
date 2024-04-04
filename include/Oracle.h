#pragma once

#include <filesystem>
#include <Input.h>

class Oracle
{
public:
	enum OracleResult
	{
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
		/// executes a python script that execute the PUT and collects information about the result and the last executed element of the input sequence
		/// </summary>
		PythonScript,
		CommandlineProgramResponsive,
		CommandlineProgramDump,
	};

	Oracle(OracleType type, std::filesystem::path oraclepath);
	bool Validate();

	OracleResult Run(std::shared_ptr<Input> input);
};

using OracleResult = std::tuple<uint64_t, std::shared_ptr<Input>>;
