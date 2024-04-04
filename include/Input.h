#pragma once

#include <list>

#include "DerivationTree.h"
#include "Oracle.h"
#include "Session.h"
#include "TaskController.h"

class Input
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
	Oracle result;
	std::list<std::string> sequence;
};
