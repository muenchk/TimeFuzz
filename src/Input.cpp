
#include "Input.h"

Input::Input()
{

}

size_t Input::Length()
{
	return sequence.size();
}

std::string& Input::operator[](size_t index)
{
	auto itr = begin();
	size_t pos = 0;
	while (itr != end())
	{
		if (pos == index)
			return *itr;
		pos++;
		itr++;
	}
	throw std::out_of_range("");
}

std::list<std::string>::iterator Input::begin()
{
	return sequence.begin();
}

std::list<std::string>::iterator Input::end()
{
	return sequence.end();
}
