#pragma once

#include <list>
#include <filesystem>
#include <chrono>

#include "DerivationTree.h"
#include "TaskController.h"
#include "Utility.h"

class ExecutionHandler;
class Test;

class Input
{
private:
	bool hasfinished = false;
	bool trimmed = false;
	std::chrono::nanoseconds executiontime;
	int exitcode = 0;

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
	inline int GetExitCode()
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
