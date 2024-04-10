#include "Logging.h"


void Logging::InitializeLog(std::filesystem::path _path)
{
	log_directory = _path;
	Profile::Init(Utility::executable_name());
	Log::Init(Utility::executable_name());
}
