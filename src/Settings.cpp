#include <memory>

#include "Settings.h"

Settings* Settings::GetSingleton()
{
	static Settings session;
	return std::addressof(session);
}
