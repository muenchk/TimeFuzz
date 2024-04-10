#pragma once

#include <list>
#include <filesystem>

#include "DerivationTree.h"
#include "TaskController.h"
#include "Utility.h"

class Input
{
public:
	Input();
	void ConvertToPython();
	void ConvertToStream();
	size_t Length();
	std::string& operator[](size_t index);
	void AddEntry(std::string entry);
	std::string ToString();

	/// <summary>
	/// Parses inputs from a python file.
	/// [The file should contain a variable name [inputs = ...]
	/// </summary>
	/// <param name="path"></param>
	/// <returns></returns>
	static std::vector<std::shared_ptr<Input>> ParseInputs(std::filesystem::path path);

private:
	DerivationTree* derive;
	std::string stringrep;
	EnumType oracleResult;
	std::list<std::string> sequence;
	[[nodiscard]] std::list<std::string>::iterator begin();
	[[nodiscard]] std::list<std::string>::iterator end();
};
