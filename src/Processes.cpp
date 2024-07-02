#include "Processes.h"
#include "ExecutionHandler.h"
#include "Utility.h"
#include <optional>
#include <string_view>
#include "Logging.h"



#if defined(unix) || defined(__unix__) || defined(__unix)
#		include <cstring>
#endif

namespace Processes
{

#if defined(unix) || defined(__unix__) || defined(__unix)


	void execvp_cpp(std::string app, std::vector<std::string> args)
	{
		std::vector<const char*> pargs;
		pargs.reserve(args.size() + 1);
		pargs.push_back(app.c_str());
		for (auto const& a : args) {
			pargs.push_back(a.c_str() + '\0');
		}
		pargs.push_back(NULL);

		// Signature: int execvp(const char *file, char *const argv[]);

		// execvp(app.c_str(), execvp(app.c_str(), (char* const *) pargs.data() )
		int status = execvp(app.c_str(), (char* const*)pargs.data());

		if (status == -1) {
			std::fprintf(stderr, " Error: unable to launch process\n");
			throw std::runtime_error("Error: failed to launch process");
		}
	}
	std::pair<bool, int> fork_exec(std::string app, std::vector<std::string> args, int timelimitsec, std::string outfile)
	{
		std::printf(" [TRACE] <BEFORE FORK> PID of parent process = %d \n", getpid());

		// PID of child process (copy of this process)
		pid_t pid = fork();

		if (pid == -1) {
			std::fprintf(stderr, "Error: unable to launch process\n");
			throw std::runtime_error("Error: unable to launch process");
		}
		if (pid == 0) {
			//std::printf(" [TRACE] Running on child process => PID_CHILD = %d \n", getpid());

			/*
		// Close file descriptors, in order to disconnect the process from the terminal.
		// This procedure allows the process to be launched as a daemon (aka service).
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		close(STDIN_FILENO);
		*/

			if (outfile != "") {
				// redirect stdout to file
				auto fd = fopen(outfile.c_str(), "w");
				//auto fd = open(outfile.c_str(), O_CREAT | O_TRUNC, "w");
				dup2(fileno(fd), fileno(stdout));
				dup2(fileno(fd), fileno(stderr));
				//auto fd = open(outfile.c_str(), O_CREAT | O_TRUNC, "w");
				//dup2(fd, STDOUT_FILENO);
				//dup2(fd, STDERR_FILENO);
			}
			// Execvp system call, replace the image of this process with a new one
			// from an executable.
			execvp_cpp(app, args);
			return { false, -1 };
		}

		std::printf(" [TRACE] <AFTER FORK> PID of parent process = %d \n", getpid());

		// pid_t waitpid(pid_t pid, int *wstatus, int options);
		int status;

		std::printf(" [TRACE] Waiting for child process to finish.\n");

		std::chrono::system_clock::time_point start = std::chrono::system_clock::now();  // = std::chrono::system_clock::now();
		bool finished = false;

		// wait until timelimit runs out and check whether the child finished
		while (timelimitsec == 0 || std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() < timelimitsec * 1000) {
			// Wait for child process termination.
			// From header: #include <sys/wait.h>
			int ret = waitpid(pid, &status, WNOHANG);
			if (ret == -1) {
				//logger::error("Processes", "Fork_exec", "Error: cannot wait for child process");
				throw std::runtime_error("Error: cannot wait for child process");
			} else if (ret == pid) {
				finished = true;
				break;
			} else {
				if ((std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count()) % 60 == 0)
					//logger::info("Processes", "Fork_exec", "Still waiting for child to finish. Running for: " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count()) + "s");
				std::this_thread::sleep_for(1s);
			}
		}
		int exitcode = 1;
		if (finished && WIFEXITED(status)) {
			exitcode = WEXITSTATUS(status);
		} else {
			// kill child since it exceeded its time limit
			kill(pid, SIGTERM);
		}

		//logger::info("Processes", "Fork_exec", "Child has finished after: " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count()) + " seconds");

		std::printf(" [TRACE] Child process has been terminated with exitcode: %d\n", exitcode);
		// -------- Parent process ----------------//
		return { finished, exitcode };
	}

	std::vector<std::string> SplitArguments(std::string par)
	{
		std::vector<std::string> argus;
		size_t pos = std::string::npos;
		std::string arg = "";
		while ((pos = par.find(' ')) != std::string::npos) {
			// we have found a blank. If the first sign is a quotation mark, extract the parameter and add it
			if (par[0] == '\"') {
				pos = par.substr(1, par.length() - 1).find('\"');
				if (pos != std::string::npos) {
					// found second quotation mark
					arg = par.substr(1, pos);  // pos is 1-based
					par.erase(0, pos + 2);
					argus.push_back(arg);
					arg = "";
				} else {
					return std::vector<std::string>{};
				}
			} else if (par[0] == '\'') {
				pos = par.substr(1, par.length() - 1).find('\'');
				if (pos != std::string::npos) {
					// found second quotation mark
					arg = par.substr(1, pos);  // pos is 1-based
					par.erase(0, pos + 2);
					argus.push_back(arg);
					arg = "";
				} else {
					return std::vector<std::string>{};
				}
			} else if (par.length() > 0) {
				argus.push_back(par.substr(0, pos));
				par.erase(0, pos + 1);
			}
		}
		// handle last argument if existing
		if (par != "") {
			if (par[0] == '\"') {
				if (par[par.length() - 1] == '\"')
					argus.push_back(par.substr(1, par.length() - 2));
				else
					return std::vector<std::string>{};
			} else if (par[0] == '\'') {
				if (par[par.length() - 1] == '\'')
					argus.push_back(par.substr(1, par.length() - 2));
				else
					return std::vector<std::string>{};
			} else
				argus.push_back(par);
		}
		// delete possible empty arguments
		auto itr = argus.begin();
		while (itr != argus.end()) {
			if (*itr == "") {
				argus.erase(itr);
				continue;
			}
			itr++;
		}
		return argus;
	}

	bool StartPUTProcess(Test* test, std::string app, std::string args)
	{
		pid_t pid = fork();
		if (pid == -1) {
			return false;
		}
		if (pid == 0) {
			// running on child process

			// redirect input and output
			close(test->red_input[1]);
			close(test->red_output[0]);

			dup2(test->red_input[0], STDIN_FILENO);
			dup2(test->red_output[1], STDOUT_FILENO);
			dup2(test->red_output[1], STDERR_FILENO);

			// split the arguments
			std::vector<std::string> command = SplitArguments(args);

			// change program
			Processes::execvp_cpp(app, command);
			return false;
		}

		test->processid = pid;
		close(test->red_output[1]);

		return true;
	}

	long GetProcessMemory(pid_t pid)
	{
		throw std::runtime_error("not implemented");
	}

	bool KillProcess(pid_t pid)
	{
		throw std::runtime_error("not implemented");
	}

	bool GetProcessRunning(pid_t pid)
	{
		throw std::runtime_error("not implemented");
	}

	int GetExitCode(pid_t)
	{
		throw std::runtime_error("not implemented");
	}

#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

#	include <Windows.h>
#	include <Psapi.h>
#	include <cstdlib>
#	include <tchar.h>

	bool StartPUTProcess(Test* test, std::string app, std::string args)
	{
		BOOL success = false;

		// process info
		ZeroMemory(&(test->pi), sizeof(PROCESS_INFORMATION));

		// start info
		ZeroMemory(&(test->si), sizeof(STARTUPINFO));
		test->si.cb = sizeof(STARTUPINFO);
		test->si.hStdError = test->red_output[1];   // set to write end
		test->si.hStdOutput = test->red_output[1];  // set to wrte end
		test->si.hStdInput = test->red_input[0];    // set to read end
		test->si.dwFlags |= STARTF_USESTDHANDLES;

		auto wstr = Utility::ConvertToWideString(std::string_view{ app });
		std::wstring _app;
		if (wstr.has_value())
			_app = wstr.value();

		wstr = Utility::ConvertToWideString(std::string_view{ args });
		std::wstring _args;
		if (wstr.has_value())
			_args = wstr.value();

		std::wstring input = _app + L" " + _args;
		wchar_t* w_args = const_cast<wchar_t*>(input.c_str());
		const wchar_t* w_app = _app.c_str();

		success = CreateProcessW(
			w_app,
			w_args,
			NULL,
			NULL,
			TRUE,
			0,
			NULL,
			NULL,
			&(test->si),
			&(test->pi));

		if (!success)
			return false;
		else {
			// close input pipe of redirected output
			CloseHandle(test->red_output[1]);
			// don't close input pipe handle, as we will be using that
			return true;
		}
	}

	long GetProcessMemory(HANDLE pid)
	{
		BOOL success;
		PROCESS_MEMORY_COUNTERS mc;
		success = GetProcessMemoryInfo(
			pid,
			&mc,
			sizeof(_PROCESS_MEMORY_COUNTERS_EX));
		if (success)
			return mc.PagefileUsage;
		else
			return -1;
	}

	bool KillProcess(HANDLE pid)
	{
		return TerminateProcess(pid, EXIT_FAILURE) > 0;
	}

	bool GetProcessRunning(HANDLE pid)
	{
		DWORD exitcode;
		BOOL success = GetExitCodeProcess(
			pid,
			&exitcode);
		if (success)
		{
			return exitcode == STILL_ACTIVE;
		} else
			return false; // probs don't have permissions to retrieve exit code
	}

	int GetExitCode(HANDLE pid)
	{
		DWORD exitcode;
		BOOL success = GetExitCodeProcess(
			pid,
			&exitcode);
		if (success)
		{
			return exitcode;
		} else {
			logcritical("Cannot get exitcode: {}", GetLastError());
			return -1;
		}
	}

#endif
}
