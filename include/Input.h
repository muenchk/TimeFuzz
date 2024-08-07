#pragma once

#include <list>
#include <filesystem>
#include <chrono>

#include "DerivationTree.h"
#include "TaskController.h"
#include "Utility.h"
#include "Data.h"

class ExecutionHandler;
class Test;

class Input
{
	const int32_t classversion = 0x1;
	uint32_t _formid = 0;
	#pragma region SaveLoad
public:
	/// <summary>
	/// returns the total size of the fields with static size
	/// </summary>
	/// <returns></returns>
	size_t GetStaticSize(int32_t version = 0x1);
	/// <summary>
	/// returns the total size of all fields of this instance
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize();
	/// <summary>
	/// returns the class version
	/// </summary>
	/// <returns></returns>
	int32_t GetClassVersion();
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
	bool ReadData(unsigned char* buffer, size_t offset, size_t length, LoadResolver* resolver);
	static int32_t GetType()
	{
		return 'INPU';
	}

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
	Test* test = nullptr;
	Input();

	~Input();

	/// <summary>
	/// resets all internal variables
	/// </summary>
	void Clear();

	/// <summary>
	/// returns the formid of this instance
	/// </summary>
	/// <returns></returns>
	uint32_t GetFormID();
	/// <summary>
	/// sets the formid
	/// </summary>
	/// <param name="formid"></param>
	void SetFormID(uint32_t formid);

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
	DerivationTree* derive = nullptr;
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
