#include "Settings.h"
#include "Logging.h"
#include "Session.h"
#include <iostream>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include "CrashHandler.h"
#endif

int32_t main(int32_t argc, char** argv)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif

	/* command line arguments
	*	--help, -h				- prints help dialogue
	*	-conf <FILE>			- specifies path to the settings file
	*	-l <SAFE>				- load prior safepoint and resume						[Not implemented]
	*	-p <SAFE>				- print statistics from prior safepoint					[Not implemented]
	*	--dry					- just run PUT once, and display output statistics		[Not implemented]
	*	--dry-i	<INPUT>			- just run PUT once with the given input				[Not implemented]
	*/

	std::wstring settingspath = L"";
	
	for (int32_t i = 1; i < argc; i++) {
		size_t pos = 0;
		std::string option = std::string(argv[i]);
		if (option.find("--help") != std::string::npos) {
			// print help dialogue and exit
			std::cout << "--help, -h			- prints help dialogue\n"
						 "-conf <FILE>			- specifies path to the settings file\n"
						 "-l <SAFE>				- load prior safepoint and resume\n"
						 "-p <SAFE>				- print statistics from prior safepoint\n"
						 "--dry					- just run PUT once, and display output statistics\n"
						 "--dry-i	<INPUT>		- just run PUT once with the given input\n";
			exit(0);
		} else if (option.find("-conf") != std::string::npos) {
			if (i + 1 < argc) {
				settingspath = Utility::ConvertToWideString(std::string_view{ argv[i + 1] }).value_or(L"");
				i++;
			}
			else
			{
				std::cout << "missing configuration file name";
				exit(1);
			}
		}
	}
	bool error = false;
	Session* sess = new Session();
	sess->StartSession(error, false, false, settingspath);
	sess->Wait();
	sess->StopSession(false);
	delete sess;

	return 0;
}
