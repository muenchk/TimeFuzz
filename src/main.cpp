#include "Settings.h"
#include "Logging.h"
#include "Session.h"
#include "ExitCodes.h"
#include <iostream>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include "CrashHandler.h"
#endif

// The imgui stuff is adapted from the official examples of the repository https://github.com/ocornut/imgui which is available under MIT license 
// date: [2024/10/23]

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "glad/glad.h"
//#include "GL/GL.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void endCallback()
{
	fprintf(stdin, "exitinternal");
}

std::shared_ptr<Session> session = nullptr;

SessionStatus status;

void StartSession()
{
	// -----Start the session or do whatever-----

	char buffer[128];

	if (CmdArgs::_load) {
		// load session
		if (CmdArgs::_num)
			session = Session::LoadSession(CmdArgs::_loadname, CmdArgs::_number, CmdArgs::_settingspath, &status, true);
		else
			session = Session::LoadSession(CmdArgs::_loadname, CmdArgs::_settingspath, &status, true);
		if (!session) {
			logcritical("Session cannot be loaded from savefile:\t{}", CmdArgs::_loadname);
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
		exit(ExitCodes::Success);
	} else if (CmdArgs::_dry) {
		throw new std::runtime_error("Dry runs aren't implemented yet");
	} else {
		// create sessions and start it
		session = Session::CreateSession();
		bool error = false;
		session->StartSession(error, false, false, CmdArgs::_settingspath);
		session->InitStatus(status);
		if (error == true) {
			logcritical("Couldn't start the session, exiting...");
			scanf("%s", buffer);
			exit(ExitCodes::StartupError);
		}
	}
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
		"    --ui				        - Starts the program in GUI mode\n"
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
		} else if (option.substr(0, 4).find("--ui") != std::string::npos) {
			CmdArgs::_ui = true;
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
		CmdArgs::_settingspath = std::filesystem::absolute(CmdArgs::_settingspath).wstring();
	} else
		CmdArgs::_settingspath = std::filesystem::current_path().wstring();

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

	bool stop = false;

	// -----go into responsive loop-----
	if (CmdArgs::_ui) {
		Logging::StdOutDebug = false;
		// start session and don't wait for completion
		StartSession();

		glfwSetErrorCallback(glfw_error_callback);
		if (!glfwInit()) {  // if we cannot open GUI, go into responsive mode as a fallback
			logcritical("Cannot open GUI: Falling back into responsive mode.");
			goto Responsive;
		}
		// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
		// GL ES 2.0 + GLSL 100
		const char* glsl_version = "#version 100";
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
		// GL 3.2 + GLSL 150
		const char* glsl_version = "#version 150";
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
		// GL 3.0 + GLSL 130
		const char* glsl_version = "#version 130";
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
		//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
		//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
		GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
		if (window == nullptr)
			return 1;
		glfwMakeContextCurrent(window);
		glfwSwapInterval(1);  // Enable vsync

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsLight();

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init(glsl_version);

		// Our state
		bool show_demo_window = true;
		bool show_another_window = false;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		while (!glfwWindowShouldClose(window) && !stop) {

			// Poll and handle events (inputs, window resize, etc.)
			// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
			// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
			// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
			// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
			glfwPollEvents();
			if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));

			// Start the Dear ImGui frame
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			// show simple window with stats
			{
				static bool wopen = true;
				ImGui::Begin("Status", &wopen);
				if (!wopen)
				{
					loginfo("UI window has been closed, exiting application.");
					if (session->Finished() == false && session->Loaded() == true)
						session->StopSession(false);
					exit(ExitCodes::Success);
				}
				if (session->Loaded() == false && session->GetLastError() == ExitCodes::StartupError) {
					ImGui::Text("The Session couldn't be started");
					ImGui::NewLine();
					if (ImGui::Button("Close"))
					{
						loginfo("UI window has been closed, exiting application.");
						exit(ExitCodes::Success);
					}
				} else {
					session->GetStatus(status);

					ImGui::Text("Executed Tests:    %llu", status.overallTests);
					ImGui::Text("Positive Tests:    %llu", status.positiveTests);
					ImGui::Text("Negative Tests:    %llu", status.negativeTests);
					ImGui::Text("Pruned Tests:      %llu", status.prunedTests);
					ImGui::Text("Runtime:           %llu s", std::chrono::duration_cast<std::chrono::seconds>(status.runtime).count());

					ImGui::NewLine();
					ImGui::Text("Status:    %s", status.status.c_str());
					ImGui::NewLine();
					if (status.saveload) {
						ImGui::Text("Save / Load Progress:                            %.3f", status.gsaveload * 100);
						ImGui::ProgressBar((float)status.gsaveload, ImVec2(0.0f, 0.0f));
					} else {
						if (status.goverall >= 0.f) {
							ImGui::Text("Progress towards overall generated tests:    %.3f%%", status.goverall * 100);
							ImGui::ProgressBar((float)status.goverall, ImVec2(0.0f, 0.0f));
						}
						if (status.gpositive >= 0.f) {
							ImGui::Text("Progress towards positive generated tests:   %.3f%%", status.gpositive * 100);
							ImGui::ProgressBar((float)status.gpositive, ImVec2(0.0f, 0.0f));
						}
						if (status.gnegative >= 0.f) {
							ImGui::Text("Progress towards negative generated tests:   %.3f%%", status.gnegative * 100);
							ImGui::ProgressBar((float)status.gnegative, ImVec2(0.0f, 0.0f));
						}
						if (status.gtime >= 0.f) {
							ImGui::Text("Progress towards timeout:                    %.3f%%", status.gtime * 100);
							ImGui::ProgressBar((float)status.gtime, ImVec2(0.0f, 0.0f));
						}

						

						if (session->Finished()) {
							ImGui::NewLine();
							ImGui::NewLine();
							ImGui::Text("The Session has ended");
							ImGui::NewLine();
							if (ImGui::Button("Close")) {
								loginfo("UI window has been closed, exiting application.");
								exit(ExitCodes::Success);
							}
						} else {
							ImGui::NewLine();
							ImGui::NewLine();
							if (ImGui::Button("Save")) {
								std::thread th(std::bind(&Session::Save, session));
								th.detach();
							}
							ImGui::SameLine();
							if (ImGui::Button("Stop")) {
								std::thread th(std::bind(&Session::StopSession, session, std::placeholders::_1), true);
								th.detach();
							}
						}
					}
				}
				ImGui::End();
			}

			// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
			//if (show_demo_window)
			//	ImGui::ShowDemoWindow(&show_demo_window);

			// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
			/* {
				static float f = 0.0f;
				static int counter = 0;

				ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!" and append into it.

				ImGui::Text("This is some useful text.");           // Display some text (you can use a format strings too)
				ImGui::Checkbox("Demo Window", &show_demo_window);  // Edit bools storing our window open/close state
				ImGui::Checkbox("Another Window", &show_another_window);

				ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
				ImGui::ColorEdit3("clear color", (float*)&clear_color);  // Edit 3 floats representing a color

				if (ImGui::Button("Button"))  // Buttons return true when clicked (most widgets return true when edited/activated)
					counter++;
				ImGui::SameLine();
				ImGui::Text("counter = %d", counter);

				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
				ImGui::End();
			}*/

			// 3. Show another simple window.
			/* if (show_another_window) {
				ImGui::Begin("Another Window", &show_another_window);  // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
				ImGui::Text("Hello from another window!");
				if (ImGui::Button("Close Me"))
					show_another_window = false;
				ImGui::End();
			}*/

			// Rendering
			ImGui::Render();
			int display_w, display_h;
			glfwGetFramebufferSize(window, &display_w, &display_h);
			//glViewport(0, 0, display_w, display_h);
			//glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
			//glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			glfwSwapBuffers(window);
		}

		// Cleanup
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(window);
		glfwTerminate();

	} else if (CmdArgs::_responsive) {
Responsive:
		Logging::StdOutDebug = false;
		Logging::StdOutLogging = false;
		// wait for session to load
		StartSession();
		while (session->Loaded() == false)
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		session->SetSessionEndCallback(endCallback);
		// stops loop
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
		// wait for session to load
		StartSession();
		while (session->Loaded() == false)
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		std::cout << "Beginning infinite wait for session end";
		session->Wait();
		session.reset();
	}
	std::cout << "Exiting program";
	scanf("%s", buffer);
	return 0;
}
