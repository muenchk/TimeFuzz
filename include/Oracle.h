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

	enum class PUTType
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
	PUTType _type;
	bool valid = false;

public:

	Oracle(PUTType type, std::filesystem::path PUTpath);
	bool Validate();

	OracleResult Evaluate(Test* test);

	static std::string TypeString(PUTType type);
	static PUTType ParseType(std::string str);
	inline std::filesystem::path path()
	{
		return _path;
	}

	static int32_t GetType()
	{
		return 'ORAC';
	}

	/// <summary>
	/// returns the total size of all fields of this instance
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize();

	const int32_t classversion = 0x1;
	/// <summary>
	/// saves all relevant information of this instance to the given buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <returns></returns>
	bool WriteData(unsigned char* buffer, size_t offset);
	/// <summary>
	/// reads all relevant information of this instance from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	bool ReadData(unsigned char* buffer, size_t offset, size_t length);
};
