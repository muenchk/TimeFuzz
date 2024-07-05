#include "SessionData.h"

#include <exception>
#include <stdexcept>

SessionData::~SessionData()
{
	Clear();
}

void SessionData::Clear()
{
	throw std::runtime_error("SessionData::Clear not implemented");
}
