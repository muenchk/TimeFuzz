#pragma once

#include <list>
#include <string>

class IInput
{
public:
	virtual void ConvertToPython() = 0;
	virtual void ConvertToStream() = 0;
	virtual size_t Length() = 0;
	virtual std::string& operator[](size_t index) = 0;
	virtual std::list<std::string>::iterator begin() = 0;
	virtual std::list<std::string>::iterator end() = 0;

private:
};
