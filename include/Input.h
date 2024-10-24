#pragma once

#include <list>
#include <filesystem>
#include <chrono>
#include <lua.hpp>

#include "DerivationTree.h"
#include "TaskController.h"
#include "Utility.h"
#include "Form.h"

class ExecutionHandler;
class Test;
class SessionFunctions;

struct lua_State;

class Input : public Form
{
	friend class SessionFunctions;

	const int32_t classversion = 0x1;
	#pragma region InheritedForm
public:
	size_t GetStaticSize(int32_t version = 0x1) override;
	size_t GetDynamicSize() override;
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	int32_t GetType() override {
		return FormType::Input;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::Input;
	}
	void Delete(Data* data);
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();

	#pragma endregion

private:
	bool hasfinished = false;
	bool trimmed = false;
	std::chrono::nanoseconds executiontime;
	int32_t exitcode = 0;

	friend ExecutionHandler;
	friend Test;

	std::string pythonstring;
	bool pythonconverted = false;

public:
	std::shared_ptr<Test> test;
	Input();

	~Input();

	static int lua_ConvertToPython(lua_State* L)
	{
		Input* input = (Input*)lua_touserdata(L, 1);
		input->ConvertToPython();
		lua_pushstring(L, input->pythonstring.c_str());
		return 1;
	}

	/// <summary>
	/// converts the input to python code
	/// </summary>
	std::string ConvertToPython(bool update = false);
	/// <summary>
	/// converts the input to a stream
	/// </summary>
	void ConvertToStream();
	/// <summary>
	/// returns the number of entries in the input
	/// </summary>
	/// <returns></returns>
	size_t Length();
	/// <summary>
	/// converts the input to a string
	/// </summary>
	/// <param name="index"></param>
	/// <returns></returns>
	std::string& operator[](size_t index);
	/// <summary>
	/// Adds an entry at the end of the input
	/// </summary>
	/// <param name="entry"></param>
	void AddEntry(std::string entry);
	/// <summary>
	/// converts the input to a string
	/// </summary>
	/// <returns></returns>
	std::string ToString();
	/// <summary>
	/// returns the hash of the input
	/// </summary>
	/// <returns></returns>
	std::size_t Hash();
	/// <summary>
	/// iterator pointing to the first element of the input
	/// </summary>
	/// <returns></returns>
	[[nodiscard]] std::list<std::string>::iterator begin();
	/// <summary>
	/// iterator pointing beyond the last element of the input
	/// </summary>
	/// <returns></returns>
	[[nodiscard]] std::list<std::string>::iterator end();

	/// <summary>
	/// Returns the execution time of the test if it has already finished, otherwise -1
	/// </summary>
	/// <returns></returns>
	inline std::chrono::nanoseconds GetExecutionTime()
	{
		if (hasfinished)
			return executiontime;
		else
			return std::chrono::nanoseconds(-1);
	}

	/// <summary>
	/// Returns the exitcode of the test if it has already finished, otherwise -1
	/// </summary>
	/// <returns></returns>
	inline int32_t GetExitCode()
	{
		if (hasfinished)
			return exitcode;
		else
			return -1;
	}

	/// <summary>
	/// Returns whether the corresponding test has finished
	/// </summary>
	/// <returns></returns>
	inline bool Finished()
	{
		return hasfinished;
	}

	/// <summary>
	/// Returns the oracle result
	/// </summary>
	/// <returns></returns>
	inline EnumType GetOracleResult()
	{
		return oracleResult;
	}

	/// <summary>
	/// Parses inputs from a python file.
	/// [The file should contain a variable name [inputs = ...]
	/// </summary>
	/// <param name="path"></param>
	/// <returns></returns>
	static std::vector<std::shared_ptr<Input>> ParseInputs(std::filesystem::path path);

private:
	/// <summary>
	/// derivation/parse tree of the input
	/// </summary>
	std::shared_ptr<DerivationTree> derive;
	/// <summary>
	/// the string representation of the input
	/// </summary>
	std::string stringrep;
	/// <summary>
	/// the oracle result of the input
	/// </summary>
	EnumType oracleResult;
	/// <summary>
	/// the underlying representation of the input sequence
	/// </summary>
	std::list<std::string> sequence;

	/// <summary>
	/// originally generated sequence [stores sequence after trimming]
	/// </summary>
	std::list<std::string> orig_sequence;
};
