#pragma once

#include <filesystem>
#include <tuple>
#include <lua.hpp>

#include "ExecutionHandler.h"
#include "Form.h"

class Oracle : public Form
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

		None = 0x8000000000000000,
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
	const int32_t classversion = 0x1;
	std::string LoracleStr = "";
	std::filesystem::path LoraclePath;

	std::string LcmdargsStr = "";
	std::filesystem::path LcmdargsPath;
	std::mutex cmdlock;

public:
	Oracle();
	~Oracle();
	void Set(PUTType type, std::filesystem::path PUTpath);
	bool Validate();
	void SetLuaCmdArgs(std::string script);
	void SetLuaCmdArgs(std::filesystem::path scrippath);
	void SetLuaOracle(std::string script);
	void SetLuaOracle(std::filesystem::path scriptpath);

	void ApplyLuaCommands(lua_State* L);

	/// <summary>
	/// Evaluates the result of a test
	/// </summary>
	/// <param name="test"></param>
	/// <returns></returns>
	OracleResult Evaluate(lua_State* L, std::shared_ptr<Test> test);
	/// <summary>
	/// computes the command line args for a test
	/// </summary>
	/// <param name="test"></param>
	/// <returns></returns>
	std::string GetCmdArgs(lua_State* L, Test* test);

	/// <summary>
	/// Returns the type of the PUT as a string
	/// </summary>
	/// <param name="type"></param>
	/// <returns></returns>
	static std::string TypeString(PUTType type);
	/// <summary>
	/// Parses the type of the put from a string
	/// </summary>
	/// <param name="str"></param>
	/// <returns></returns>
	static PUTType ParseType(std::string str);

	PUTType GetOracletype()
	{
		return _type;
	}
	/// <summary>
	/// returns the path of the PUT
	/// </summary>
	/// <returns></returns>
	inline std::filesystem::path path()
	{
		return _path;
	}

	size_t GetStaticSize(int32_t version = 0x1) override;
	size_t GetDynamicSize() override;
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	int32_t GetType() override
	{
		return FormType::Oracle;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::Oracle;
	}
	void Delete(Data* data);
	void Clear();
	inline static bool _registeredFactories = false;
	static void RegisterFactories();
};
