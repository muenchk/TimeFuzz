
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
	return sequence[index];
}

