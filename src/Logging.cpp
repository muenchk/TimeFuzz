#include "Logging.h"


void Logging::InitializeLog(std::filesystem::path _path, bool append)
{
	log_directory = _path;
	Profile::Init(Utility::executable_name(), append);
	Log::Init(Utility::executable_name(), append);
}
