#pragma once 

#include <chrono>
#include <thread>
#include <string>

#ifndef H_PROCESSES
#define H_PROCESSES

using namespace std::chrono_literals;

class ExecutionHandler;
class Test;

#if defined(unix) || defined(__unix__) || defined(__unix)
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include "Windows.h"
#endif

namespace Processes
{
#	if defined(unix) || defined(__unix__) || defined(__unix)

#		include <unistd.h>
#		include <fcntl.h>
#		include <sys/stat.h>
#		include <sys/types.h>
#		include <sys/wait.h>

	////////////////// Original Author : CAIORSS
	// C++ wrapper for the exevp() library-call
	// It replaces the current process image with a new one
	// from other executable.
	void execvp_cpp(std::string app, std::vector<std::string> args);

	std::pair<bool, int> fork_exec(std::string app, std::vector<std::string> args, int timelimitsec, std::string outfile);

	bool StartPUTProcess(Test* test, std::string app, std::string args);

	long GetProcessMemory(pid_t pid);

	bool KillProcess(pid_t pid);

	bool GetProcessRunning(pid_t pid, int* exitcode);

	int GetExitCode(pid_t);

#	elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

	bool StartPUTProcess(Test* test, std::string app, std::string args);

	long GetProcessMemory(HANDLE pid);

	bool KillProcess(HANDLE pid);

	bool GetProcessRunning(HANDLE pid);

	int GetExitCode(HANDLE pid);

#	endif

#endif
}
