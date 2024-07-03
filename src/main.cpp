#include "Settings.h"
#include "Logging.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include "CrashHandler.h"
#endif

int main(/*int argc, char** argv*/)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif
	return 0;
}
