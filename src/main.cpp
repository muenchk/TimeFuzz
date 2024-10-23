#include "Settings.h"
#include "Logging.h"
#include "Session.h"
#include "ExitCodes.h"
#include <iostream>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include "CrashHandler.h"
#endif

void endCallback()
{
	fprintf(stdin, "exitinternal");
}

int32_t main(int32_t argc, char** argv)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(std::filesystem::current_path().string());
#endif

	char buffer[128];

	/* command line arguments
	*	--help, -h				- prints help dialogue
	*	-conf <FILE>			- specifies path to the settings file
	*	-l <SAFE>				- load prior safepoint and resume					
	*	-p <SAFE>				- print statistics from prior safepoint					
	*	--dry					- just run PUT once, and display output statistics	
	*	--dry-i	<INPUT>			- just run PUT once with the given input			
	*/

	// -----Evaluate the command line parameters-----

	std::string cmdargs =
		"    --help, -h                 - prints help dialogue\n"
		"    --conf <FILE>              - specifies path to the settings file\n"
		"    --load <NAME>              - load prior safepoint and resume\n"
		"    --load-num <NAME> <NUM>    - load specific prior safepoint and resume\n"
		"    --print <NAME>             - print statistics from prior safepoint\n"
		"    --print-num <NAME> <NUM>   - print statistics from specific prior safepoint\n"
		"    --dry                      - just run PUT once, and display output statistics\n"
		"    --dry-i <INPUT>            - just run PUT once with the given input\n"
		"    --responsive               - Enables resposive console mode accepting inputs from use\n"
		"    --logtoconsole             - Writes all logging output to the console\n"
		"    --separatelogfiles         - Writes logfiles to \"/logs\" and uses timestamps in the logname\n"
		"    --create-conf <PATH>       - Writes a default configuration file to the current folder\n";

	std::string logpath = "";
	bool logtimestamps = false;

	for (int32_t i = 1; i < argc; i++) {
		size_t pos = 0;
		std::string option = std::string(argv[i]);
		if (option.find("--help") != std::string::npos) {
			// print help dialogue and exit
			std::cout << cmdargs;
			scanf("%s", buffer);
			exit(0);
		} else if (option.find("--load-num") != std::string::npos) {
			if (i + 2 < argc) {
				CmdArgs::_num = true;
				CmdArgs::_loadname = std::string(argv[i + 1]);
				try {
					CmdArgs::_number = std::stoi(std::string(argv[i + 2]));
				} catch (std::exception&) {
					std::cerr << "missing number of save to load";
					scanf("%s", buffer);
					exit(ExitCodes::ArgumentError);
				}
				CmdArgs::_load = true;
				i++;
			} else {
				std::cerr << "missing name of save to load";
				scanf("%s", buffer);
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--logtoconsole") != std::string::npos) {
			Logging::StdOutError = true;
			Logging::StdOutLogging = true;
			Logging::StdOutWarn = true;
			Logging::StdOutDebug = true;
		} else if (option.find("separatelogfiles") != std::string::npos) {
			logtimestamps = true;
			logpath = "logs";
		} else if (option.find("--load") != std::string::npos) {
			if (i + 1 < argc) {
				CmdArgs::_loadname = std::string(argv[i + 1]);
				CmdArgs::_load = true;
				i++;
			} else {
				std::cerr << "missing name of save to load";
				scanf("%s", buffer);
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--print-num") != std::string::npos) {
			if (i + 2 < argc) {
				CmdArgs::_num = true;
				CmdArgs::_loadname = std::string(argv[i + 1]);
				try {
					CmdArgs::_number = std::stoi(std::string(argv[i + 2]));
				} catch (std::exception&) {
					std::cerr << "missing number of save to load";
					scanf("%s", buffer);
					exit(ExitCodes::ArgumentError);
				}
				CmdArgs::_print = true;
				i++;
			} else {
				std::cerr << "missing name of save to print";
				scanf("%s", buffer);
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--print") != std::string::npos) {
			if (i + 1 < argc) {
				CmdArgs::_loadname = std::string(argv[i + 1]);
				CmdArgs::_print = true;
				i++;
			} else {
				std::cerr << "missing name of save to print";
				scanf("%s", buffer);
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--dry-i") != std::string::npos) {
			if (i + 1 < argc) {
				CmdArgs::_dryinput = std::string(argv[i + 1]);
				CmdArgs::_dryi = true;
				CmdArgs::_dry = true;
				i++;
			} else {
				std::cerr << "missing input for dry run";
				scanf("%s", buffer);
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--dry") != std::string::npos) {
			CmdArgs::_dry = true;
		} else if (option.find("--responsive") != std::string::npos) {
			CmdArgs::_responsive = true;
		} else if (option.find("--create-conf") != std::string::npos) {
			if (i + 1 < argc) {
				CmdArgs::_settingspath = Utility::ConvertToWideString(std::string_view{ argv[i + 1] }).value();
				//printf("%ls\t%s\t%s\n", Utility::ConvertToWideString(std::string_view{ argv[i + 1] }).value().c_str(), argv[i + 1]);
				Settings* settings = new Settings();
				settings->Save(CmdArgs::_settingspath);
				printf("Wrote default configuration file to the specified location: %ls\n", CmdArgs::_settingspath.c_str());
				exit(ExitCodes::Success);
			} else {
				std::cerr << "missing configuration file name";
				scanf("%s", buffer);
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--conf") != std::string::npos) {
			if (i + 1 < argc) {
				CmdArgs::_settingspath = Utility::ConvertToWideString(std::string_view{ argv[i + 1] }).value_or(L"");
				CmdArgs::_settings = true;
				i++;
			} else {
				std::cerr << "missing configuration file name";
				scanf("%s", buffer);
				exit(ExitCodes::ArgumentError);
			}
		}
	}

	// -----sanity check the parameters-----

	// check out configuration file path
	if (CmdArgs::_settings && !std::filesystem::exists(CmdArgs::_settingspath)) {
		// if the settings/conf file is given but does not exist
		std::cout << "Configuration file path is invalid: " << std::filesystem::absolute(std::filesystem::path(CmdArgs::_settingspath)).string();
		scanf("%s", buffer);
		exit(ExitCodes::ArgumentError);
	} else if (CmdArgs::_settings) {
		CmdArgs::_settingspath = std::filesystem::absolute(CmdArgs::_settingspath);
	} else
		CmdArgs::_settingspath = std::filesystem::current_path();

	// determine the working directory based on the path of the configuration file
	if (CmdArgs::_settings) {
		CmdArgs::workdir = std::filesystem::absolute(std::filesystem::path(CmdArgs::_settingspath)).parent_path();

	} else {
		CmdArgs::workdir = std::filesystem::current_path();
	}

	std::filesystem::current_path(CmdArgs::workdir);

	std::filesystem::create_directories(CmdArgs::workdir / logpath);

	// init logging engine
	Logging::InitializeLog(CmdArgs::workdir / logpath, false, logtimestamps);
	Logging::StdOutWarn = true;
	Logging::StdOutError = true;

	logmessage("Working Directory:\t{}", CmdArgs::workdir.string());
	logmessage("Configuration file path:\t{}", std::filesystem::absolute(std::filesystem::path(CmdArgs::_settingspath)).string());

	// check out the load path and print
	if (CmdArgs::_load && CmdArgs::_print) {
		logcritical("Load and Print option cannot be active at the same time.");
		scanf("%s", buffer);
		exit(ExitCodes::ArgumentError);
	}

	// check out dry run options
	if ((CmdArgs::_load || CmdArgs::_print) && CmdArgs::_dry) {
		logcritical("Dry run is incompatible with load and print options.");
		scanf("%s", buffer);
		exit(ExitCodes::ArgumentError);
	}

	// -----Start the session or do whatever-----

	std::shared_ptr<Session> session = nullptr;
	if (CmdArgs::_load) {
		// load session
		if (CmdArgs::_num)
			session = Session::LoadSession(CmdArgs::_loadname, CmdArgs::_number, CmdArgs::_settingspath);
		else
			session = Session::LoadSession(CmdArgs::_loadname, CmdArgs::_settingspath);
		if (!session) {
			logcritical("Session cannot be loaded from savefile:\t{}", CmdArgs::_loadname);
			scanf("%s", buffer);
			exit(ExitCodes::StartupError);
		}
		// start loaded session
		bool error = false;
		session->StartLoadedSession(error);
		if (error == true) {
			logcritical("Couldn't start the loaded session, exiting...");
			scanf("%s", buffer);
			exit(ExitCodes::StartupError);
		}
	} else if (CmdArgs::_print) {
		// load session
		if (CmdArgs::_num)
			session = Session::LoadSession(CmdArgs::_loadname, CmdArgs::_number, CmdArgs::_settingspath);
		else
			session = Session::LoadSession(CmdArgs::_loadname, CmdArgs::_settingspath);
		if (!session) {
			logcritical("Session cannot be loaded from savefile:\t{}", CmdArgs::_loadname);
			scanf("%s", buffer);
			exit(ExitCodes::StartupError);
		}
		// print stats
		std::cout << session->PrintStats();
		// stop session, destroy it and exit
		session->StopSession();
		session.reset();
		return 0;
	} else if (CmdArgs::_dry) {
		throw new std::runtime_error("Dry runs aren't implemented yet");
	} else {
		// create sessions and start it
		session = Session::CreateSession();
		bool error = false;
		session->StartSession(error, false, false, CmdArgs::_settingspath);
		if (error == true) {
			logcritical("Couldn't start the session, exiting...");
			scanf("%s", buffer);
			exit(ExitCodes::StartupError);
		}
	}

	// -----go into responsive loop-----
	if (CmdArgs::_responsive) {
		Logging::StdOutDebug = false;
		Logging::StdOutLogging = false;
		// stops loop
		bool stop = false;
		while (!stop) {
			// read commands from cmd and act on them
			std::string line;
			getline(std::cin, line);
			line = Utility::ToLower(line);
			// we aren't using LLM so we only accept simple prefix-based commands

			// CMD: kill
			if (line.substr(0, 4).find("kill") != std::string::npos) {
			}
			// CMD: stop [save | nosave]
			else if (line.substr(0, 4).find("stop") != std::string::npos) {
				if (line.find("nosave") != std::string::npos) {
					// stop without saving and exit loop
					session->StopSession(false);
					stop = true;
				} else {
					// stop with saving wether the user wants or not :)
					session->StopSession(true);
					stop = true;
				}
			}
			// CMD: save
			else if (line.substr(0, 4).find("save") != std::string::npos) {
				session->Save();
			}
			// CMD: stats
			else if (line.substr(0, 5).find("stats") != std::string::npos) {
				std::cout << session->PrintStats();
			}
			// CMD: INTERNAL: exitinternal
			else if (line.substr(0, 12).find("exitinternal") != std::string::npos) {
				// internal command to exit main loop and program
				stop = true;
			}
		}
		session->DestroySession();
		session.reset();
	} else {
		std::cout << "Beginning infinite wait for session end";
		session->Wait();
		session.reset();
	}
	std::cout << "Exiting program";
	scanf("%s", buffer);
	return 0;
}
