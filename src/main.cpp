#include "Settings.h"
#include "Logging.h"
#include "Session.h"
#include "ExitCodes.h"
#include "UIClasses.h"
#include "shader/learnopengl_shader_s.h"
#include <iostream>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include "CrashHandler.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// The openGL texture and drawing stuff (the background) are adapted from code published on
// learnopengl.com by JOEY DE VRIES and is available under
// the following license: https://creativecommons.org/licenses/by-nc/4.0/legalcode

// The imgui stuff is adapted from the official examples of the repository https://github.com/ocornut/imgui which is available under MIT license 
// date: [2024/10/23]

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "glad/glad.h"
//#include "GL/GL.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
//#if defined(IMGUI_IMPL_OPENGL_ES2)
//#include <GLES2/gl2.h>
//#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void endCallback()
{
	fprintf(stdin, "exitinternal");
}


// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

std::shared_ptr<Session> session = nullptr;

SessionStatus status;

void StartSession()
{
	// -----Start the session or do whatever-----

	loginfo("Main: Start Session.");

	char buffer[128];

	LoadSessionArgs args;
	args.reloadSettings = CmdArgs::_reloadConfig;
	args.settingsPath = CmdArgs::_settingspath;

	if (CmdArgs::_load) {
		args.startSession = true;
		// load session
		if (CmdArgs::_num)
			session = Session::LoadSession(CmdArgs::_loadname, CmdArgs::_number, args, &status);
		else
			session = Session::LoadSession(CmdArgs::_loadname, args, &status);
		if (!session) {
			logcritical("Session cannot be loaded from savefile:\t{}", CmdArgs::_loadname);
			scanf("%s", buffer);
			exit(ExitCodes::StartupError);
		}
	} else if (CmdArgs::_print) {
		args.startSession = false;
		// load session
		if (CmdArgs::_num)
			session = Session::LoadSession(CmdArgs::_loadname, CmdArgs::_number, args, &status);
		else
			session = Session::LoadSession(CmdArgs::_loadname, args, &status);
		if (!session) {
			logcritical("Session cannot be loaded from savefile:\t{}", CmdArgs::_loadname);
			scanf("%s", buffer);
			exit(ExitCodes::StartupError);
		}
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

	loginfo("Main: Started Session.");
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
		"    --reloadconfig             - reloads the configuration from config file instead of save\n"
		"    --print <NAME>             - print statistics from prior safepoint\n"
		"    --print-num <NAME> <NUM>   - print statistics from specific prior safepoint\n"
		"    --dry                      - just run PUT once, and display output statistics\n"
		"    --dry-i <INPUT>            - just run PUT once with the given input\n"
		"    --responsive               - Enables resposive console mode accepting inputs from use\n"
		"    --ui				        - Starts the program in GUI mode\n"
		"    --logtoconsole             - Writes all logging output to the console\n"
		"    --separatelogfiles         - Writes logfiles to \"/logs\" and uses timestamps in the logname\n"
		"    --create-conf <PATH>       - Writes a default configuration file to the current folder\n"
		"    --debug                    - Enable debug logging\n";

	std::string logpath = "";
	bool logtimestamps = false;

	std::filesystem::path execpath = std::filesystem::path(argv[0]).parent_path();

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
				std::cout << "Parameter: --load-num\n";
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
		} else if (option.find("--reloadconfig") != std::string::npos) {
			std::cout << "Parameter: --reloadconfig\n";
			CmdArgs::_reloadConfig = true;
		} else if (option.find("--logtoconsole") != std::string::npos) {
			std::cout << "Parameter: --logtoconsole\n";
			Logging::StdOutError = true;
			Logging::StdOutLogging = true;
			Logging::StdOutWarn = true;
			Logging::StdOutDebug = true;
		} else if (option.find("--separatelogfiles") != std::string::npos) {
			std::cout << "Parameter: --separatelogfiles\n";
			logtimestamps = true;
			logpath = "logs";
		} else if (option.find("--load") != std::string::npos) {
			if (i + 1 < argc) {
				std::cout << "Parameter: --load\n";
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
				std::cout << "Parameter: --print-num\n";
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
				std::cout << "Parameter: --print\n";
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
				std::cout << "Parameter: --dry-i\n";
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
			std::cout << "Parameter: --dry\n";
			CmdArgs::_dry = true;
		} else if (option.find("--responsive") != std::string::npos) {
			std::cout << "Parameter: --responsive\n";
			CmdArgs::_responsive = true;
		} else if (option.substr(0, 4).find("--ui") != std::string::npos) {
			std::cout << "Parameter: --ui\n";
			CmdArgs::_ui = true;
		} else if (option.substr(0, 4).find("--debug") != std::string::npos) {
			std::cout << "Parameter: --debug\n";
			CmdArgs::_debug = true;
		} else if (option.find("--create-conf") != std::string::npos) {
			if (i + 1 < argc) {
				std::cout << "Parameter: --create-conf\n";
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
				std::cout << "Parameter: --conf\n";
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

	// set debugging
	Logging::EnableDebug = CmdArgs::_debug;

	// -----go into responsive loop-----
	if (CmdArgs::_ui) {
		Logging::StdOutDebug = false;
		// start session and don't wait for completion
		StartSession();
		loginfo("Start UI");

		std::string windowtitle = "TimeFuzz: " + status.sessionname;
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
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
																		//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
		GLFWwindow* window = glfwCreateWindow(1280, 1280, windowtitle.c_str(), nullptr, nullptr);
		if (window == nullptr)
			return 1;
		glfwMakeContextCurrent(window);
		glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			std::cout << "Failed to initialize GLAD" << std::endl;
			return -1;
		}
		//glfwSetWindowAspectRatio(window, 16, 9);

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
		//ImGui_ImplOpenGL3_Init(glsl_version);
		ImGui_ImplOpenGL3_Init("#version 330");

		// build and compile our shader zprogram
		// ------------------------------------
		Shader ourShader((execpath / "shader" / "4.1.texture.vs").string().c_str(), (execpath / "shader" / "4.1.texture.fs").string().c_str());

		float vertices[] = {
			// positions          // colors           // texture coords
			1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,    // top right
			1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom right
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // bottom left
			-1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f    // top left
		};
		unsigned int indices[] = {
			0, 1, 3,  // first triangle
			1, 2, 3   // second triangle
		};
		unsigned int VBO, VAO, EBO;
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		// position attribute
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// color attribute
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// texture coord attribute
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		unsigned int texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);  // set texture wrapping to GL_REPEAT (default wrapping method)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		int iwidth, iheight, inrChannels;
		stbi_set_flip_vertically_on_load(true);
		unsigned char* data = stbi_load((execpath / "background.png").string().c_str(), &iwidth, &iheight, &inrChannels, 0);
		if (data) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, iwidth, iheight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
		} else
			logcritical("Cannot open texture.");
		stbi_image_free(data);

		// Our state
		bool show_demo_window = true;
		bool show_another_window = false;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		// runtime vars
		bool displayadd;

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

			// get session status
			session->GetStatus(status);
			displayadd = true;

			// show simple window with stats
			{
				static bool wopen = true;
				ImGui::Begin("Status", &wopen);
				if (!wopen) {
					loginfo("UI window has been closed, exiting application.");
					if (session->Finished() == false && session->Loaded() == true)
						session->StopSession(false);
					exit(ExitCodes::Success);
				}
				if (session->Loaded() == false && session->GetLastError() == ExitCodes::StartupError) {
					displayadd = false;
					ImGui::Text("The Session couldn't be started");
					ImGui::NewLine();
					if (ImGui::Button("Close")) {
						loginfo("UI window has been closed, exiting application.");
						exit(ExitCodes::Success);
					}
				} else {
					ImGui::Text("Executed Tests:    %llu", status.overallTests);
					ImGui::Text("Positive Tests:    %llu", status.positiveTests);
					ImGui::Text("Negative Tests:    %llu", status.negativeTests);
					ImGui::Text("Unfinished Tests:  %llu", status.unfinishedTests);
					ImGui::Text("Pruned Tests:      %llu", status.prunedTests);
					ImGui::Text("Runtime:           %llu s", std::chrono::duration_cast<std::chrono::seconds>(status.runtime).count());

					ImGui::NewLine();
					ImGui::Text("Status:    %s", status.status.c_str());
					ImGui::NewLine();
					if (status.saveload) {
						ImGui::Text("Save / Load Progress:                            %llu/%llu", status.saveload_current, status.saveload_max);
						ImGui::ProgressBar((float)status.gsaveload, ImVec2(0.0f, 0.0f));
						ImGui::Text("Record: %20s                 %llu/%llu", FormType::ToString(status.record).c_str(), status.saveload_record_current, status.saveload_record_len);
						ImGui::ProgressBar((float)status.grecord, ImVec2(0.0f, 0.0f));
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

						ImGui::NewLine();
						ImGui::NewLine();

						if (session->Finished()) {
							ImGui::Text("The Session has ended");
							ImGui::NewLine();
							if (ImGui::Button("Close")) {
								loginfo("UI window has been closed, exiting application.");
								exit(ExitCodes::Success);
							}
						} else if (session->Loaded() && session->Running() == false && session->Finished() == false) {
							if (ImGui::Button("Close")) {
								loginfo("UI window has been closed, exiting application.");
								exit(ExitCodes::Success);
							}
							ImGui::SameLine();
							if (ImGui::Button("Start")) {
								bool error = false;
								session->StartLoadedSession(error, CmdArgs::_reloadConfig, CmdArgs::_settingspath, nullptr);
							}
						} else {
							if (ImGui::Button("Save")) {
								std::thread th(std::bind(&Session::Save, session));
								th.detach();
							}
							ImGui::SameLine();
							if (ImGui::Button("Stop")) {
								std::thread th(std::bind(&Session::StopSession, session, std::placeholders::_1), true);
								th.detach();
							}
							ImGui::SameLine();
							if (ImGui::Button("Abort")) {
								std::thread th(std::bind(&Session::StopSession, session, std::placeholders::_1), false);
								th.detach();
							}
						}
					}
				}
				ImGui::End();
			}

			// show window with advanced stats
			{
				static bool wopen = true;
				ImGui::Begin("Advanced", &wopen);
				if (wopen) {
					if (displayadd) {
						ImGui::SeparatorText("Exclusion Tree");
						ImGui::Text("Depth:                %lld", status.excl_depth);
						ImGui::Text("Nodes:                %llu", status.excl_nodecount);
						ImGui::Text("Leaves:               %llu", status.excl_leafcount);

						ImGui::SeparatorText("Task Controller");
						ImGui::Text("Waiting:              %d", status.task_waiting);
						ImGui::Text("Completed:            %llu", status.task_completed);

						ImGui::SeparatorText("Execution Handler");
						ImGui::Text("Waiting:              %d", status.exec_waiting);
						ImGui::Text("Running:              %d", status.exec_running);
						ImGui::Text("Stopping:             %d", status.exec_stopping);

						ImGui::SeparatorText("Generation");
						ImGui::Text("Generated Inputs:     %lld", status.gen_generatedInputs);
						ImGui::Text("Excluded with Prefix: %lld", status.gen_generatedWithPrefix);
						ImGui::Text("Generation Fails:     %lld", status.gen_generationFails);
						ImGui::Text("Add Test Fails:       %lld", status.gen_addtestfails);
						ImGui::Text("Failure Rate:         %llf", status.gen_failureRate);

						ImGui::SeparatorText("Test exit stats");
						ImGui::Text("Natural:              %llu", status.exitstats.natural);
						ImGui::Text("Last Input:           %llu", status.exitstats.lastinput);
						ImGui::Text("Terminated:           %llu", status.exitstats.terminated);
						ImGui::Text("Timeout:              %llu", status.exitstats.timeout);
						ImGui::Text("Fragment Timeout:     %llu", status.exitstats.fragmenttimeout);
						ImGui::Text("Memory:               %llu", status.exitstats.memory);
						ImGui::Text("Init error:           %llu", status.exitstats.initerror);
					} else {
						ImGui::Text("Not available");
					}
				}
				ImGui::End();
			}

			// show window with information about the best generated inputs
			{
				static bool wopen = true;
				ImGui::Begin("Inputs", &wopen);
				if (wopen) {
					static int32_t MAX_ITEMS = 100;
					if (displayadd) {
						static ImVector<UI::UIInput> imElements;
						static std::vector<UI::UIInput> elements;
						if (imElements.Size == 0)
							imElements.resize(MAX_ITEMS);
						// GET ITEMS FROM SESSION
						session->UI_GetTopK(elements, MAX_ITEMS);

						for (int i = 0; i < MAX_ITEMS; i++)
							imElements[i] = elements[i];

						// construct table
						static ImGuiTableFlags flags =
							ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;
						//PushStyleCompact
						//...
						//PopStyleCompact
						if (ImGui::BeginTable("itemtable", 6, flags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 15), 0.0f))
						{
							ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputID);
							ImGui::TableSetupColumn("Length",  ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputLength);
							ImGui::TableSetupColumn("Primary Score",  ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputPrimaryScore);
							ImGui::TableSetupColumn("Secondary Score", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputSecondaryScore);
							ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputResult);
							ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputAction);
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableHeadersRow();

							// DO SOME SORTING

							// use clipper for large vertical lists
							ImGuiListClipper clipper;
							clipper.Begin(imElements.size());
							while (clipper.Step())
							{
								for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
								{
									auto item = &imElements[i];
									ImGui::PushID((uint32_t)item->id);
									ImGui::TableNextRow();
									ImGui::TableNextColumn();
									ImGui::Text("%8llx", item->id);
									ImGui::TableNextColumn();
									ImGui::Text("%10lu", item->length);
									ImGui::TableNextColumn();
									ImGui::Text("%8.2f", item->primaryScore);
									ImGui::TableNextColumn();
									ImGui::Text("%8.2f", item->secondaryScore);
									ImGui::TableNextColumn();
									if (item->result == UI::Result::Failing)
										ImGui::Text("Failing");
									else if (item->result == UI::Result::Passing)
										ImGui::Text("Passing");
									else
										ImGui::Text("Unfinished");
									ImGui::TableNextColumn();
									if (ImGui::SmallButton("Replay"))
									{
										std::thread th(std::bind(&Session::Replay, session, std::placeholders::_1), item->id);
										th.detach();
									}
									ImGui::PopID();
								}
							}
							ImGui::EndTable();
						}

					} else {
						ImGui::Text("Not available");
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
			glViewport(0, 0, display_w, display_h);
			glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
			glClear(GL_COLOR_BUFFER_BIT);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture);
			ourShader.use();
			glBindVertexArray(VAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			glfwSwapBuffers(window);
		}

		// Cleanup
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);

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
