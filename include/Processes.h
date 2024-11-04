#pragma once 

#include <chrono>
#include <thread>
#include <string>
#include <memory>

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

	std::pair<bool, int32_t> fork_exec(std::string app, std::vector<std::string> args, int32_t timelimitsec, std::string outfile);

	bool StartPUTProcess(std::shared_ptr<Test> test, std::string app, std::string args);

	uint64_t GetProcessMemory(pid_t pid);

	bool KillProcess(pid_t pid);

	bool GetProcessRunning(pid_t pid, int32_t* exitcode);

	int32_t GetExitCode(pid_t);

#	elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

	bool StartPUTProcess(std::shared_ptr<Test> test, std::string app, std::string args);

	uint64_t GetProcessMemory(HANDLE pid);

	bool KillProcess(HANDLE pid);

	bool GetProcessRunning(HANDLE pid);

	int32_t GetExitCode(HANDLE pid);

#	endif

#endif
}
