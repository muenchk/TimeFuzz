#pragma once

#include <list>

#include "DerivationTree.h"
#include "Oracle.h"
#include "Interfaces/IInput.h"
#include "Session.h"
#include "TaskController.h"

class Input : IInput
{
public:
	Input();
	void ConvertToPython();
	void ConvertToStream();
	size_t Length();
	std::string& operator[](size_t index);

private:
	DerivationTree* derive;
	std::string stringrep;
	OracleResult result;
	std::list<std::string> sequence;
	[[nodiscard]] std::list<std::string>::iterator begin();
	[[nodiscard]] std::list<std::string>::iterator end();
};
