#include "Settings.h"
#include "Logging.h"
#include "CrashHandler/CrashHandler.h"

int main(int argc, char** argv)
{
	Logging::InitializeLog(".");
	Crash::Install(".");
	return 0;
}
