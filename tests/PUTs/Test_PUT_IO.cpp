#include <thread>
#include <array>
#include <cstdio>

#include "Logging.h"

#if defined(unix) || defined(__unix__) || defined(__unix)
#	include <unistd.h>
#	include <poll.h>
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include <Windows.h>
#endif


int main(/*int argc, char** argv*/)
{
	Logging::InitializeLog(".");
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

#if defined(unix) || defined(__unix__) || defined(__unix)
	std::string ret = "";
	struct pollfd fds;
	int events = 0;
	fds.fd = STDIN_FILENO;  // stdin
	fds.events = POLLIN;
	events = poll(&fds, 1, 0);
	logdebug("0: {}", events);
	while (ret != "What a wonderful day..") {  // input to read available
		if ((fds.revents & POLLIN) == POLLIN) {
			logdebug("1");
			char buf[512];
			logdebug("2");
			int _read = read(STDIN_FILENO, buf, 511);
			logdebug("4: {}", _read);
			if (_read <= 0) {
				logdebug("5");
				break;
			} else {
				logdebug("6");
				// convert to string
				std::string str(buf, _read);
				logdebug("{}", str);
				ret += str;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		fds.revents = 0;
		events = poll(&fds, 1, 0);
	}

#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::string ret = "";
	static HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
	DWORD dwRead;
	CHAR chBuf[512];
	BOOL bSuccess = FALSE;
	while (ret != "What a wonderful day..") {
		bSuccess = ReadFile(handle, chBuf, 512, &dwRead, NULL);
		if (!bSuccess || dwRead == 0)
			break;
		else {
			// convert read to string
			std::string str(chBuf, dwRead);
			logdebug("{}", str);
			ret += str;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
#endif

	/*
	char buf[1024] = { 0 };
	while (buf[0] != '.') {
#if defined(unix) || defined(__unix__) || defined(__unix)
		scanf("%1023s", buf);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
		scanf_s("%s", buf, (int)std::size(buf));
#endif
		logdebug("{}", std::string(buf, strlen(buf)));
		printf("%s", buf);
	}*/
	std::this_thread::sleep_for(std::chrono::seconds(1));
	exit(11);
}
