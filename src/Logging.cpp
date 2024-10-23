#include "Logging.h"
#include <string>
#include <chrono>
#include <format>


void Logging::InitializeLog(std::filesystem::path _path, bool append, bool timestamp)
{
	log_directory = _path;
	if (timestamp) {
		std::stringstream ss;
		auto now = std::chrono::system_clock::now();
		ss << now;
		std::cout << ss.str() << "\t" << std::format("{:%Y-%m-%d_%H-%M-%S}", std::chrono::floor<std::chrono::seconds>(now)) << "\n";
		Profile::Init(Utility::executable_name() + "_" + ss.str());
		Log::Init(Utility::executable_name() + "_" + ss.str());
	} else {
		Profile::Init(Utility::executable_name(), append);
		Log::Init(Utility::executable_name(), append);
	}
}
