#include <thread>
#include <array>
#include <cstdio>

#include "Logging.h"

#if defined(unix) || defined(__unix__) || defined(__unix)
#	include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include <Windows.h>
#endif

int main(int argc, char** argv)
{
	Logging::InitializeLog(".");
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	char buf[1024];
	while (buf[0] != '.') {
#if defined(unix) || defined(__unix__) || defined(__unix)
		scanf("%1023s", buf);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
		scanf_s("%s", buf, std::size(buf));
#endif
		logdebug("{}", std::string(buf));
		printf("%s", buf);
	}
	std::this_thread::sleep_for(std::chrono::seconds(1));
	return 11;
}
