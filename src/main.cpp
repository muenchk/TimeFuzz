#include "ExitCodes.h"
#include "Logging.h"
#include "Session.h"
#include "Settings.h"
#include "UIClasses.h"
#include "ansi_escapes.h"
#include <filesystem>
#include <iostream>
#include "DeltaDebugging.h"
//#include "Processes.h"

namespace Processes
{
#if defined(unix) || defined(__unix__) || defined(__unix)
	extern uint64_t GetProcessMemory(pid_t pid);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	extern uint64_t GetProcessMemory(HANDLE pid);
#endif
}

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include "ChrashHandlerINCL.h"
#endif

#ifdef EnableUI
	#include "shader/learnopengl_shader_s.h"

	#define STB_IMAGE_IMPLEMENTATION
	#include <stb_image.h>

	// The openGL texture and drawing stuff (the background) are adapted from code published on
	// learnopengl.com by JOEY DE VRIES and is available under
	// the following license: https://creativecommons.org/licenses/by-nc/4.0/legalcode

	// The imgui stuff is adapted from the official examples of the repository https://github.com/ocornut/imgui which is available under MIT license
	// date: [2024/10/23]

	#include "glad/glad.h"
	#include "imgui.h"
	#include "imgui_impl_glfw.h"
	#include "imgui_impl_opengl3.h"
	//#include "GL/GL.h"
	#include <stdio.h>
	#define GL_SILENCE_DEPRECATION
	//#if defined(IMGUI_IMPL_OPENGL_ES2)
	//#include <GLES2/gl2.h>
	//#endif
	#include <GLFW/glfw3.h>  // Will drag system OpenGL headers

	static void glfw_error_callback(int error, const char* description)
	{
		fprintf(stderr, "GLFW Error %d: %s\n", error, description);
	}

	// glfw: whenever the window size changed (by OS or user resize) this _callback function executes
	// ---------------------------------------------------------------------------------------------
	void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
	{
		// make sure the viewport matches the new window dimensions; note that width and
		// height will be significantly larger than specified on retina displays.
		glViewport(0, 0, width, height);
	}

#endif

void endCallback()
{
	fprintf(stdin, "exitinternal");
}


std::shared_ptr<Session> session = nullptr;

SessionStatus status;

std::string statuspath;

std::string Snapshot(bool full)
{
	std::stringstream snap;

	static UI::UIGeneration ActiveGeneration;

	static std::vector<UI::UIDeltaDebugging> ddcontrollers;
	static size_t numddcontrollers = 0;
	static UI::UIInputInformation inputinfo;
	bool updatedddcontrollers = false;

	StartProfiling;

	// get session status
	session->GetStatus(status);

	// general
	snap << "### GENERAL\n";
	{ 
#if defined(unix) || defined(__unix__) || defined(__unix)
		auto memory_mem = Processes::GetProcessMemory(getpid());
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
		auto memory_mem = Processes::GetProcessMemory(GetCurrentProcess());
#endif
		memory_mem = memory_mem / 1048576;

		snap << fmt::format("Memory Usage:    {}MB", memory_mem)
			 << "\n";
	}
	snap << "\n\n";
	// status
	snap << "### STATUS\n";
	{
		snap << fmt::format("Executed Tests:    {}", status.overallTests) << "\n";
		snap << fmt::format("Positive Tests:    {}", status.positiveTests) << "\n";
		snap << fmt::format("Negative Tests:    {}", status.negativeTests) << "\n";
		snap << fmt::format("Unfinished Tests:  {}", status.unfinishedTests) << "\n";
		snap << fmt::format("Undefined Tests:   {}", status.undefinedTests) << "\n";
		snap << fmt::format("Pruned Tests:      {}", status.prunedTests) << "\n";
		snap << fmt::format("Runtime:           {} s", Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(status.runtime).count())) << "\n";
		snap << "\n";
		snap << fmt::format("Status:    {}", status.status.c_str());
		snap << "\n";

		if (status.saveload) {
			snap << fmt::format("Save / Load Progress:                            {}/{}", status.saveload_current, status.saveload_max) << "\n";
			snap << fmt::format("Record: {:20}                 {}/{}", FormType::ToString(status.record).c_str(), status.saveload_record_current, status.saveload_record_len) << "\n";
		} else {
			if (status.goverall >= 0.f) {
				snap << fmt::format("Progress towards overall generated tests:    {}%", status.goverall * 100) << "\n";
			}
			if (status.gpositive >= 0.f) {
				snap << fmt::format("Progress towards positive generated tests:   {}%", status.gpositive * 100) << "\n";
			}
			if (status.gnegative >= 0.f) {
				snap << fmt::format("Progress towards negative generated tests:   {}%", status.gnegative * 100) << "\n";
			}
			if (status.gtime >= 0.f) {
				snap << fmt::format("Progress towards timeout:                    {}%", status.gtime * 100) << "\n";
			}

		}
	}
	snap << "\n\n";

	// advanced stats
	snap << "### ADVANCED STATS\n";
	{
		static size_t hashmapSize;
		session->UI_GetHashmapInformation(hashmapSize);

		snap << ("Exclusion Tree") << "\n";
		snap << fmt::format("Depth:                   {}", status.excl_depth) << "\n";
		snap << fmt::format("Nodes:                   {}", status.excl_nodecount) << "\n";
		snap << fmt::format("Leaves:                  {}", status.excl_leafcount) << "\n";

		snap << ("Task Controller") << "\n";
		snap << fmt::format("Waiting [All]:           {}", status.task_waiting) << "\n";
		snap << fmt::format("Waiting [Medium]:        {}", status.task_waiting_medium) << "\n";
		snap << fmt::format("Waiting [Light]:         {}", status.task_waiting_light) << "\n";
		snap << fmt::format("Completed:               {}", status.task_completed) << "\n";

		snap << ("Execution Handler") << "\n";
		snap << fmt::format("Waiting:                 {}", status.exec_waiting) << "\n";
		snap << fmt::format("Initialized:             {}", status.exec_initialized) << "\n";
		snap << fmt::format("Running:                 {}", status.exec_running) << "\n";
		snap << fmt::format("Stopping:                {}", status.exec_stopping) << "\n";

		snap << ("Generation") << "\n";
		snap << fmt::format("Generated Inputs:        {}", status.gen_generatedInputs) << "\n";
		snap << fmt::format("Excluded with Prefix:    {}", status.gen_generatedWithPrefix) << "\n";
		snap << fmt::format("Excluded Approximation:  {}", status.gen_excludedApprox) << "\n";
		snap << fmt::format("Generation Fails:        {}", status.gen_generationFails) << "\n";
		snap << fmt::format("Add Test Fails:          {}", status.gen_addtestfails) << "\n";
		snap << fmt::format("Failure Rate:            {}", status.gen_failureRate) << "\n";

		snap << ("Test exit stats") << "\n";
		snap << fmt::format("Natural:                 {}", status.exitstats.natural) << "\n";
		snap << fmt::format("Last Input:              {}", status.exitstats.lastinput) << "\n";
		snap << fmt::format("Terminated:              {}", status.exitstats.terminated) << "\n";
		snap << fmt::format("Timeout:                 {}", status.exitstats.timeout) << "\n";
		snap << fmt::format("Fragment Timeout:        {}", status.exitstats.fragmenttimeout) << "\n";
		snap << fmt::format("Memory:                  {}", status.exitstats.memory) << "\n";
		snap << fmt::format("Pipe:                    {}", status.exitstats.pipe) << "\n";
		snap << fmt::format("Init error:              {}", status.exitstats.initerror) << "\n";
		snap << fmt::format("Repeated Inputs:         {}", status.exitstats.repeat) << "\n";

		snap << ("Internals") << "\n";
		snap << fmt::format("Hashmap size:            {}", hashmapSize) << "\n";
	}
	snap << "\n\n";

	// tasks
	snap << "### TASKS\n";
	{
		static UI::UITaskController controller;
		if (controller.Initialized() == false)
			session->UI_GetTaskController(controller);
		if (controller.Initialized()) {
			controller.LockExecutedTasks();
			auto itr = controller.beginExecutedTasks();
			while (itr != controller.endExecutedTasks()) {
				snap << fmt::format("{}: {}", itr->first.c_str(), itr->second) << "\n";
				itr++;
			}
			controller.UnlockExecutedTasks();
		}
	}
	snap << "\n\n";

	// inputs
	snap << "### INPUT INFO\n";
	{
		if (inputinfo.Initialized()) {
			static bool listview = false;

			snap << ("General Information") << "\n";
			snap << fmt::format("Input ID:                   {}", Utility::GetHexFill(inputinfo._inputID).c_str()) << "\n";
			snap << fmt::format("Parent ID:                  {}", Utility::GetHexFill(inputinfo._parentInput).c_str()) << "\n";
			snap << fmt::format("Generation ID:              {}", Utility::GetHexFill(inputinfo._generationID).c_str()) << "\n";
			snap << fmt::format("Derived Inputs:             {}", inputinfo._derivedInputs) << "\n";
			snap << fmt::format("Flags:                      {}", Utility::GetHexFill(inputinfo._flags).c_str()) << "\n";
			snap << fmt::format("Has Finished:               {}", inputinfo._hasfinished ? "true" : "false") << "\n";
			snap << "\n";

			snap << ("Generation Information") << "\n";
			snap << fmt::format("Generation Time:            {} ns", inputinfo._generationTime.count()) << "\n";
			snap << fmt::format("Generation Length:          {}", inputinfo._naturallength) << "\n";
			snap << fmt::format("Trimmed Length:             {}", inputinfo._trimmedlength) << "\n";
			snap << "\n";

			snap << ("Test Information") << "\n";
			snap << fmt::format("Execution Time:             {} ns", inputinfo._executionTime.count()) << "\n";
			snap << fmt::format("Primary Score:              {}", inputinfo._primaryScore) << "\n";
			snap << fmt::format("Secondary Score:            {}", inputinfo._secondaryScore) << "\n";
			snap << fmt::format("Exitcode:                   {}", inputinfo._exitcode) << "\n";
			snap << fmt::format("ExitReason:                 {}", inputinfo._exitreason) << "\n";

			snap << ("Command Line Args:") << "\n";
			snap << fmt::format("{}", inputinfo._cmdArgs.c_str()) << "\n";
			snap << ("Script Args:") << "\n";
			snap << fmt::format("{}", inputinfo._cmdArgs.c_str()) << "\n";
			snap << "\n";

			snap << ("Input") << "\n";
			snap << fmt::format("{}", inputinfo._inputlist.c_str()) << "\n";
			snap << "\n";
			snap << ("Output") << "\n";
			snap << fmt::format("{}", inputinfo._output.c_str()) << "\n";
		} else {
			snap << fmt::format("Not available") << "\n";
		}
	}
	snap << "\n\n";

	// thread status
	snap << "### THREAD STATUS\n";
	{
		auto toStringExec = [](ExecHandlerStatus stat) {
			switch (stat) {
			case ExecHandlerStatus::None:
				return "None";
			case ExecHandlerStatus::Exitted:
				return "Exitted";
			case ExecHandlerStatus::HandlingTests:
				return "HandlingTests";
			case ExecHandlerStatus::MainLoop:
				return "MainLoop";
			case ExecHandlerStatus::Sleeping:
				return "Sleeping";
			case ExecHandlerStatus::StartingTests:
				return "StartingTests";
			case ExecHandlerStatus::Waiting:
				return "Waiting";
			case ExecHandlerStatus::KillingProcessMemory:
				return "KillingProcessMemory";
			case ExecHandlerStatus::WriteFragment:
				return "WriteFragment";
			case ExecHandlerStatus::KillingProcessTimeout:
				return "KillingProcessTimeout";
			case ExecHandlerStatus::StoppingTest:
				return "StoppingTest";
			}
			return "None";
		};

		static std::vector<TaskController::ThreadStatus> stati;
		static std::vector<const char*> statiName;
		static std::vector<std::string> statiTime;
		static ExecHandlerStatus execstatus;
		session->UI_GetThreadStatus(stati, statiName, statiTime);
		snap << ("TaskController") << "\n";
		for (size_t i = 0; i < stati.size(); i++) {
			switch (stati[i]) {
			case TaskController::ThreadStatus::Initializing:
				snap << fmt::format("Status: Initializing") << "\n";
				break;
			case TaskController::ThreadStatus::Running:
				snap << fmt::format("Status: Running | {} | {}", statiName[i], statiTime[i]) << "\n";
				break;
			case TaskController::ThreadStatus::Waiting:
				snap << fmt::format("Status: Waiting") << "\n";
				break;
			default:
				snap << fmt::format("Status: None") << "\n";
				break;
			}
		}
		snap << ("ExecutionHandler") << "\n";
		execstatus = session->UI_GetExecHandlerStatus();
		snap << fmt::format("Status: {}", toStringExec(execstatus)) << "\n";
	}
	snap << "\n\n";

	auto res = [](UI::Result result) {
		if (result == UI::Result::Failing)
			return std::string("Failing");
		else if (result == UI::Result::Passing)
			return std::string("Passing");
		else if (result == UI::Result::Running)
			return std::string("Running");
		else
			return std::string("Unfinished");
	};

	// generation
	snap << "### GENERATION\n";
	{
		session->UI_GetCurrentGeneration(ActiveGeneration);
		if (session->Loaded()) {
			static std::vector<std::pair<FormID, int32_t>> generations;
			static size_t numgenerations;
			static std::pair<FormID, FormID> selection = { 0, 0 };
			static bool changed = false;
			if (!ActiveGeneration.Initialized()) {
				changed = true;
				session->UI_GetGenerations(generations, numgenerations);
				session->UI_GetCurrentGeneration(ActiveGeneration);
				if (ActiveGeneration.Initialized()) {
					selection = { ActiveGeneration.GetFormID(), ActiveGeneration.GetGenerationNumber() };
				}
			}

			static std::string preview;
			if (changed) {
				changed = false;
				if (ActiveGeneration.Initialized())
					preview = std::to_string(ActiveGeneration.GetGenerationNumber());
				else
					preview = "None";
			}

			snap << "Generation: " << preview << "\n";

			if (ActiveGeneration.Initialized()) {
				// update stuff
				static std::vector<UI::UIInput> sources;
				ActiveGeneration.GetSources(sources);
				static int64_t numsources;
				numsources = ActiveGeneration.GetNumberOfSources();
				ActiveGeneration.GetDDControllers(ddcontrollers, numddcontrollers);

				// build ui
				snap << fmt::format("Generation Number:          {}", ActiveGeneration.GetGenerationNumber()) << "\n";
				snap << fmt::format("Total Size:                 {}", ActiveGeneration.GetSize()) << "\n";
				snap << fmt::format("Generated Size:             {}", ActiveGeneration.GetGeneratedSize())
					 << "\t\t"
					 << fmt::format("Target Size:                {}", ActiveGeneration.GetTargetSize()) << "\n";
				snap << fmt::format("DD Size:                    {}", ActiveGeneration.GetDDSize()) << "\n";
				snap << fmt::format("Active Inputs:              {}", ActiveGeneration.GetActiveInputs()) << "\n";
				int activedd = 0;
				for (auto& dd : ddcontrollers)
				{
					if (!dd.Finished())
						activedd++;
				}
				snap << fmt::format("Number of DD controllers:   {}", numddcontrollers) 
					<< "\t\t"
					<< fmt::format("Active:    {}", activedd) <<"\n";
				snap << "\n";
				snap << fmt::format("Source Inputs: {}", numsources) << "\n";

				snap << "\n";
				// do something with sources

				snap << "Sources:\n";
				snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15}", "ID", "Length", "Primary Score", "Secondary Score", "Result", "Flags", "Generation", "Derived Inputs", "ExecTime") << "\n";

				int32_t max = 5;
				if (full || sources.size() < max)
					max = (int32_t)sources.size();
				for (int32_t i = 0; i < max; i++) {
					auto item = &sources[i];
					snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:>15} ns", Utility::GetHex(item->id), item->length, item->primaryScore, item->secondaryScore, res(item->result), Utility::GetHexFill(item->flags), item->generationNumber, item->derivedInputs, item->exectime.count()) << "\n";
				}
			}
		}
	}
	snap << "\n\n";

	// top k
	snap << "### TOP K\n";
	{
		int32_t max = 5;
		if (full)
			max = 100;
		static std::vector<UI::UIInput> elements;

		// GET ITEMS FROM SESSION
		session->UI_GetTopK(elements, max);

		snap << "Inputs:\n";
		snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15}", "ID", "Length", "Primary Score", "Secondary Score", "Result", "Flags", "Generation", "Derived Inputs", "ExecTime") << "\n";

		if (elements.size() < max)
			max = (int32_t)elements.size();
		for (int32_t i = 0; i < max; i++) {
			auto item = &elements[i];
			if (item->id != 0) {
				snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:>15} ns", Utility::GetHex(item->id), item->length, item->primaryScore, item->secondaryScore, res(item->result), Utility::GetHexFill(item->flags), item->generationNumber, item->derivedInputs, item->exectime.count()) << "\n";
			}
		}
	}
	snap << "\n\n";

	// profiling
	snap << "### PROFILING\n";
	{
		static int32_t MAX_ITEMS = 100;
		static std::vector<UI::UIExecTime> dimElements(MAX_ITEMS);
		int32_t count = 0;
		auto itr = Profile::exectimes.begin();
		while (count < 100 && itr != Profile::exectimes.end()) {
			Profile::ExecTime* exec = itr->second;
			dimElements[count].ns = exec->exectime;
			dimElements[count].executions = exec->executions;
			dimElements[count].average = exec->average;
			dimElements[count].time = exec->lastexec;
			dimElements[count].func = exec->functionName;
			dimElements[count].file = exec->fileName;
			dimElements[count].usermes = exec->usermessage;
			dimElements[count].tmpid = count;
			count++;
			itr++;
		}

		snap << "Profile Times:\n";
		snap << fmt::format("{:<20} {:<30} {:<15} {:<15} {:<15} {:<15} {}", "File", "Function", "Exec Time", "Average ExecT", "Last executed", "Executions", "Message") << "\n";

		for (int i = 0; i < count; i++) {
			auto item = &dimElements[i];
			snap << fmt::format("{:<20} {:<30} {:<15} {:<15} {:<15} {:<15} {}", item->file, item->func, Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(item->ns).count()), Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(item->average).count()) , Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - item->time).count()), item->executions, item->usermes) << "\n";
		}
	}
	snap << "\n\n";

	// last generated
	snap << "### LAST GENERATED\n";
	{
		static std::vector<UI::UIInput> elements;

		// GET ITEMS FROM SESSION
		session->UI_GetLastRunInputs(elements, 0);

		snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15}", "ID", "Length", "Primary Score", "Secondary Score", "Result", "Flags", "Generation", "Derived Inputs", "ExecTime") << "\n";

		int32_t max = 10;
		if (full || elements.size() < max)
			max = (int32_t)elements.size();
		for (int32_t i = 0; i < max; i++)
		{
			auto item = &elements[i];
			if (item->id != 0) {
				snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:>15} ns", Utility::GetHex(item->id), item->length, item->primaryScore, item->secondaryScore, res(item->result), Utility::GetHexFill(item->flags), item->generationNumber, item->derivedInputs, item->exectime.count()) << "\n";
			}
		}
	}

	snap << "\n\n";

	// delta debugging
	snap << "### DELTA DEBUGGING\n";
	{
		static int32_t ITEMS = 100;
		static std::vector<UI::UIInput> inputs(ITEMS);
		static size_t numinputs;
		static std::vector<UI::UIDDResult> results(ITEMS);
		static size_t numresults;
		static UI::UIInput input;
		static UI::UIInput origInput;
		static UI::UIDeltaDebugging dd;
		static bool changed = false;
		if (ActiveGeneration.Initialized() && (dd.Initialized() == false || dd.Finished() == true)) {
			if (!updatedddcontrollers) {
				ActiveGeneration.GetDDControllers(ddcontrollers, numddcontrollers);
				updatedddcontrollers = true;
			}
			changed = true;
			if (dd.Initialized() && dd.Finished() == true)
			{
				bool f = false;
				for (size_t i = 0; i < ddcontrollers.size(); i++)
					if (ddcontrollers[i].Initialized() && ddcontrollers[i].Finished() == false) {
						dd = ddcontrollers[i];
						f = true;
					}
				if (!f)
					dd = ddcontrollers[0];
			}
			else if (ddcontrollers.size() > 0)
				dd = ddcontrollers[0];
		}
		static std::string preview;
		if (changed) {
			changed = false;
			if (dd.Initialized())
				preview = Utility::GetHexFill(dd.GetFormID());
			else
				preview = "None";
		}

		snap << "DeltaController: " << preview << "\n";
		if (dd.Initialized() && ActiveGeneration.Initialized()) {
			// update stuff
			dd.GetInput(input);
			dd.GetOriginalInput(origInput);
			dd.GetActiveInputs(inputs, numinputs);
			dd.GetResults(results, numresults);

			// build ui
			snap << fmt::format("Finished: {}", dd.Finished() ? "True" : "False") << "\n";
			snap << fmt::format("Goal:  {}", dd.GetGoal().c_str()) << "\t\t";
			snap << fmt::format("Mode:  {}", dd.GetMode().c_str()) << "\n";
			snap << fmt::format("Level: {}", dd.GetLevel()) << "\t\t";
			snap << fmt::format("Total Tests: {}", dd.GetTotalTests()) << "\n";
			snap << fmt::format("Skipped Tests: {}", dd.GetSkippedTests()) << "\n";
			snap << fmt::format("Prefix Tests: {}", dd.GetPrefixTests()) << "\n";
			snap << fmt::format("Invalid Tests: {}", dd.GetInvalidTests()) << "\n";
			snap << fmt::format("Tests done: {}", dd.GetTests()) << "\t\t";
			snap << fmt::format("Tests Remaining: {}", dd.GetTestsRemaining()) << "\n";
			snap << fmt::format("Current BatchIdent: {}", dd.GetBatchIdent()) << "\n";
			snap << fmt::format("Runtime: {}", Logging::FormatTime(dd.GetRuntime().count() / 1000)) << "\n";
			snap << "\n";

			
			snap << "Original Input:\n";
			snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15}", "ID", "Length", "Primary Score", "Secondary Score", "Result", "Flags", "Generation", "Derived Inputs", "ExecTime") << "\n";
			{
				auto item = &origInput;
				snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:>15} ns", Utility::GetHex(item->id), item->length, item->primaryScore, item->secondaryScore, res(item->result), Utility::GetHexFill(item->flags), item->generationNumber, item->derivedInputs, item->exectime.count()) << "\n";
			}
			
			snap << "Input:\n";
			snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15}", "ID", "Length", "Primary Score", "Secondary Score", "Result", "Flags", "Generation", "Derived Inputs", "ExecTime") << "\n";
			{
				auto item = &input;
				snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:>15} ns", Utility::GetHex(item->id), item->length, item->primaryScore, item->secondaryScore, res(item->result), Utility::GetHexFill(item->flags), item->generationNumber, item->derivedInputs, item->exectime.count()) << "\n";
			}

			snap << "Results:\n";
			snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<10} {:<16} {:<17} {:<15}", "ID", "Length", "Primary Score", "Secondary Score", "Result", "Flags", "Level", "Loss (Primary)", "Loss (Secondary)", "ExecTime") << "\n";

			int32_t max = 5;
			if (full || results.size() < max)
				max = (int32_t)results.size();
			for (int i = 0; i < max; i++) {
				auto item = &results[i];
				snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<10} {:<16} {:<17} {:<15}ns", Utility::GetHex(item->id), item->length, item->primaryScore, item->secondaryScore, res(item->result), Utility::GetHexFill(item->flags), item->level, item->primaryLoss, item->secondaryLoss, item->exectime.count()) << "\n";
			}


			snap << "Active Inputs:\n";
			snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15}", "ID", "Length", "Primary Score", "Secondary Score", "Result", "Flags", "Generation", "Derived Inputs", "ExecTime") << "\n";

			max = 5;
			if (full || inputs.size() < max)
				max = (int32_t)inputs.size();
			for (int i = 0; i < max; i++) {
				auto item = &inputs[i];
				snap << fmt::format("{:<10} {:<10} {:<15} {:<15} {:<15} {:<15} {:<15} {:<15} {:>15} ns", Utility::GetHex(item->id), item->length, item->primaryScore, item->secondaryScore, res(item->result), Utility::GetHexFill(item->flags), item->generationNumber, item->derivedInputs, item->exectime.count()) << "\n";
			}

		} else {
			snap << ("No delta debugging active.") << "\n";
		}
	}
	snap << "\n\n";

	profile(TimeProfiling, "Snap");

	return snap.str();
}

void SaveStatus()
{
	static auto time = std::chrono::system_clock::now();
	if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - time).count() >= (long long)CmdArgs::_saveStatusSeconds) {
		time = std::chrono::system_clock::now();
		std::string snap = Snapshot(true);
		if (!std::filesystem::exists(std::filesystem::path(CmdArgs::workdir / statuspath)))
			std::filesystem::create_directories(std::filesystem::path(CmdArgs::workdir / statuspath));
		auto days = std::chrono::floor<std::chrono::days>(time);
		std::chrono::year_month_day ymd { days};
		std::chrono::hh_mm_ss td{std::chrono::floor<std::chrono::seconds>(time - days) };
		auto path = CmdArgs::workdir / statuspath /
		            (status.sessionname + "_" + std::to_string(static_cast<int>(ymd.year())) + "_" + std::to_string(static_cast<unsigned>(ymd.month())) + "_" + std::to_string(static_cast<unsigned>(ymd.day())) + "_" + std::to_string(td.hours().count()) + "_" + std::to_string(td.minutes().count()) + "_" + std::to_string(td.seconds().count()) + ".txt");
		std::ofstream out(path);
		if (out.is_open()) {
			out.write(snap.c_str(), snap.size());
			out.flush();
		} else
			logcritical("Cannot open file for status.");
		out.close();
	}
}

void StartSession()
{
	// -----Start the session or do whatever-----

	loginfo("Main: Start Session.");

	char buffer[128];

	LoadSessionArgs args;
	args.reloadSettings = CmdArgs::_reloadConfig;
	args.settingsPath = CmdArgs::_settingspath;
	args.loadNewGrammar = CmdArgs::_updateGrammar;
	args.skipExclusionTree = CmdArgs::_doNotLoadExclusionTree;
	args.customsavepath = CmdArgs::_customsavepath;
	args.savepath = CmdArgs::_savepath;

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
		session->StartSession(error, args.skipExclusionTree, false, false, CmdArgs::_settingspath, {}, CmdArgs::_customsavepath, CmdArgs::_savepath);
		session->InitStatus(status);
		if (error == true) {
			logcritical("Couldn't start the session, exiting...");
			scanf("%s", buffer);
			exit(ExitCodes::StartupError);
		}
	}

	loginfo("Main: Started Session.");
}

#include <csignal>
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#else
//#	include <signal.h>
extern "C" void signal_callback_handler(int signum)
{
	printf("Caught signal SIGPIPE %d\n", signum);
}
#endif

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
	*	--dry					- just run PUT once, and display _output statistics	
	*	--dry-i	<INPUT>			- just run PUT once with the given _input			
	*/

	// -----Evaluate the command line parameters-----

	std::string cmdargs =
		"    --help, -h                           - prints help dialogue\n"
		"    --conf <FILE>                        - specifies path to the settings file\n"
		"    --load <NAME>                        - load prior safepoint and resume\n"
		"    --load-num <NAME> <NUM>              - load specific prior safepoint and resume\n"
		"    --reloadconfig                       - reloads the configuration from config file instead of save\n"
		"    --print <NAME>                       - print statistics from prior safepoint\n"
		"    --print-num <NAME> <NUM>             - print statistics from specific prior safepoint\n"
		"    --dry                                - just run PUT once, and display output statistics\n"
		"    --dry-i <INPUT>                      - just run PUT once with the given input\n"
		"    --responsive                         - Enables resposive console mode accepting inputs from use\n"
		"    --ui			    	              - Starts the program in GUI mode\n"
		"    --logtoconsole                       - Writes all logging output to the console\n"
		"    --separatelogfiles <FOLDER>          - Writes logfiles to \"/logs\" and uses timestamps in the logname\n"
		"    --create-conf <PATH>                 - Writes a default configuration file to the current folder\n"
		"    --update-grammar                     - Loads a new grammar and sets it as the default grammar for generation\n"
		"    --no-exclusiontree                   - Skips the loading of data from the exclusion tree\n"
		"    --debug                              - Enable debug logging\n"
		"    --clear-tasks                        - clears all tasks and active tests from the session\n"
		"    --save-status <time/sec> <FOLDER>    - saves the current status every x seconds\n"
		"    --savepath <FOLDER>                  - custom path to savefiles\n";

	std::string logpath = "";
	bool logtimestamps = false;
	std::string savepath = "";
	bool custsavepath = false;

	std::filesystem::path execpath = std::filesystem::path(argv[0]).parent_path();

	for (int32_t i = 1; i < argc; i++) {
		size_t pos = 0;
		std::string option = std::string(argv[i]);
		if (option.find("--help") != std::string::npos) {
			// print help dialogue and exit
			std::cout << cmdargs;
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
					exit(ExitCodes::ArgumentError);
				}
				CmdArgs::_load = true;
				i++;
			} else {
				std::cerr << "missing name of save to load";
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--clear-tasks") != std::string::npos) {
			std::cout << "Parameter: --clear-tasks\n";
			CmdArgs::_clearTasks = true;
		} else if (option.find("--no-exclusiontree") != std::string::npos) {
			std::cout << "Parameter: --no-exclusiontree\n";
			CmdArgs::_doNotLoadExclusionTree = true;
		} else if (option.find("--reloadconfig") != std::string::npos) {
			std::cout << "Parameter: --reloadconfig\n";
			CmdArgs::_reloadConfig = true;
		} else if (option.find("--update-grammar") != std::string::npos) {
			std::cout << "Parameter: --update-grammar\n";
			CmdArgs::_updateGrammar = true;
		} else if (option.find("--logtoconsole") != std::string::npos) {
			std::cout << "Parameter: --logtoconsole\n";
			Logging::StdOutError = true;
			Logging::StdOutLogging = true;
			Logging::StdOutWarn = true;
			Logging::StdOutDebug = true;
		} else if (option.find("--separatelogfiles") != std::string::npos) {
			if (i + 1 < argc) {
				std::cout << "Parameter: --separatelogfiles\n";
				logtimestamps = true;
				logpath = argv[i + 1];
				i++;
			} else {
				std::cerr << "missing logpath";
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--savepath") != std::string::npos) {
			if (i + 1 < argc) {
				std::cout << "Parameter: --savepath\n";
				custsavepath = true;
				savepath = argv[i + 1];
				i++;
			} else {
				std::cerr << "missing savepath";
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--consoleui") != std::string::npos) {
			std::cout << "Parameter: --consoleui\n";
			CmdArgs::_consoleUI = true;
		} else if (option.find("--save-status") != std::string::npos) {
			if (i + 2 < argc) {
				try {
					CmdArgs::_saveStatusSeconds = std::stoi(std::string(argv[i + 1]));
				} catch (std::exception&) {
					std::cerr << "missing number of seconds to save status";
					exit(ExitCodes::ArgumentError);
				}
				statuspath = argv[i + 2];
				std::cout << "Parameter: --save-status\t" + std::to_string(CmdArgs::_saveStatusSeconds) + "\t" + statuspath + "\n";
				CmdArgs::_saveStatus = true;
				i+=2;
			} else {
				std::cerr << "missing number of seconds to save status and/or folder";
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--load") != std::string::npos) {
			if (i + 1 < argc) {
				std::cout << "Parameter: --load\n";
				CmdArgs::_loadname = std::string(argv[i + 1]);
				CmdArgs::_load = true;
				i++;
			} else {
				std::cerr << "missing name of save to load";
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
					exit(ExitCodes::ArgumentError);
				}
				CmdArgs::_print = true;
				i++;
			} else {
				std::cerr << "missing name of save to print";
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
			Logging::EnableDebug = true;
		} else if (option.find("--create-conf") != std::string::npos) {
			if (i + 1 < argc) {
				std::cout << "Parameter: --create-conf\n";
				CmdArgs::_settingspath = Utility::ConvertToWideString(std::string_view{ argv[i + 1] }).value();
				//printf("%ls\t%s\t%s\n", Utility::ConvertToWideString(std::string_view{ argv[i + 1] }).value().c_str(), argv[i + 1]);
				Settings* settings = new Settings();
				settings->Load(CmdArgs::_settingspath);
				settings->Save(CmdArgs::_settingspath);
				printf("Wrote default configuration file to the specified location: %ls\n", CmdArgs::_settingspath.c_str());
				exit(ExitCodes::Success);
			} else {
				std::cerr << "missing configuration file name";
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
				exit(ExitCodes::ArgumentError);
			}
		} else if (option.find("--test") != std::string::npos) {
			std::vector<std::pair<size_t, size_t>> vec;
			vec.push_back({ 1, 3 });
			vec.push_back({ 5, 6 });
			vec.push_back({ 20, 24 });
			//RangeCalculator<size_t> calc(&vec, (size_t)100);
			//auto ranges = calc.GetNewRangesWithout((size_t)1, (size_t)25);
			exit(1);
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

	if (CmdArgs::_saveStatus)
		std::filesystem::create_directories(CmdArgs::workdir / statuspath);

	if (custsavepath)
	{
		CmdArgs::_customsavepath = custsavepath;
		std::filesystem::create_directories(CmdArgs::workdir / savepath);
		CmdArgs::_savepath = std::filesystem::absolute(CmdArgs::workdir / savepath);
	}

	// init logging engine
	Logging::InitializeLog(CmdArgs::workdir / logpath, false, logtimestamps);
	Profile::SetWriteLog(false);
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

	
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#else
#	define _XOPEN_SOURCE 700
	//#	include <features.h>
	//#	include <signal.h>
	//#   include <stdio.h>
	//    #include <stdlib.h>
	//   #include <unistd.h>
	//struct signaction action
	//{
	//	SIG_IGN
	//};
	//sigaction(SIGPIPE, &(action), NULL);
	loginfo("install signal handler for SIGPIPE");
	//signal(SIGPIPE, signal_callback_handler);
	//struct sigaction act;
	//memset(&act, 0, sizeof(act));
	//act.sa_handler = SIG_IGN;
	//act.sa_flags = SA_RESTART;
	//int r = sigaction(SIGPIPE, &act, NULL);
	//if (r)
	//	logcritical("error sigaction");
	std::signal(SIGPIPE, signal_callback_handler);
#endif

	#if defined(unix) || defined(__unix__) || defined(__unix)
	loginfo("install signal handler for SIGPIPE unix");
	std::signal(SIGPIPE, SIG_IGN);
	#endif


	// set debugging
	Logging::EnableDebug = CmdArgs::_debug;

#ifdef EnableUI
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
		GLFWwindow* window = glfwCreateWindow(1920, 1280, windowtitle.c_str(), nullptr, nullptr);
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
		auto context = ImGui::CreateContext();
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
		bool showStatusWindow = true;
		bool showAdvancedWindow = true;
		bool showTaskWindow = true;
		bool showTopK = true;
		bool showProfiling = true;
		bool showDeltaDebugging = true;
		bool showThreadStatus = true;
		bool showGeneration = true;
		bool showInput = false;
		bool showPositiveInputs = true;
		bool showLastGenerated = true;

		bool interrupted = false;

		bool destroying = false;

		std::chrono::steady_clock::time_point last;

		while (!glfwWindowShouldClose(window) && !stop) {
			// Poll and handle events (inputs, window resize, etc.)
			// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
			// - When io.WantCaptureMouse is true, do not dispatch mouse _input data to your main application, or clear/overwrite your copy of the mouse data.
			// - When io.WantCaptureKeyboard is true, do not dispatch keyboard _input data to your main application, or clear/overwrite your copy of the keyboard data.
			// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
			glfwPollEvents();
			if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
				//std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			auto diff = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - last);
			if (diff.count() < 16667)
				std::this_thread::sleep_for(std::chrono::microseconds((16667 - diff.count())/2));
			last = std::chrono::steady_clock::now();

			// Start the Dear ImGui frame
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			// get session status
			session->GetStatus(status);

			if (CmdArgs::_saveStatus)
				SaveStatus();

			displayadd = true;

			static UI::UIGeneration ActiveGeneration;

			static std::vector<UI::UIDeltaDebugging> ddcontrollers;
			static size_t numddcontrollers = 0;
			static UI::UIInputInformation inputinfo;
			bool updatedddcontrollers = false;

			// show control window
			{
				static bool wopen = true;
				ImGui::Begin("Control", &wopen);
				if (wopen) { 
#	if defined(unix) || defined(__unix__) || defined(__unix)
					auto memory_mem = Processes::GetProcessMemory(getpid());
#	elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
					auto memory_mem = Processes::GetProcessMemory(GetCurrentProcess());
#	endif
					memory_mem = memory_mem / 1048576;
					ImGui::Text("Memory Usage:    %lldMB", memory_mem);
					ImGui::NewLine();
					ImGui::Checkbox("Status window", &showStatusWindow);
					ImGui::Checkbox("Avanced status window", &showAdvancedWindow);
					ImGui::Checkbox("Task window", &showTaskWindow);
					ImGui::Checkbox("Top K inputs window", &showTopK);
					ImGui::Checkbox("Profiling window", &showProfiling);
					ImGui::Checkbox("Delta Debugging window", &showDeltaDebugging);
					ImGui::Checkbox("Show Thread Status", &showThreadStatus);
					ImGui::Checkbox("Show Generation window", &showGeneration);
					ImGui::Checkbox("Show Input Information", &showInput);
					ImGui::Checkbox("Show last generated", &showLastGenerated);
					ImGui::Text("Latency: %.3f ms, FPS: %.1f", 1000.0f / io.Framerate, io.Framerate);
				}
				ImGui::End();
			}

			// show simple window with stats
			if (showStatusWindow) {
				static bool wopen = true;
				ImGui::Begin("Status", &wopen);
				/*if (!wopen) {
					loginfo("UI window has been closed, exiting application.");
					if (session->Finished() == false && session->Loaded() == true) {
						session->StopSession(false);
						ddcontrollers.clear();
						inputinfo.Reset();
						ActiveGeneration.Reset();
					}
					exit(ExitCodes::Success);
				}*/
				if (destroying)
				{
					if (session->IsDestroyed())
					{
						session.reset();
						Profile::Dispose();
						exit(ExitCodes::Success);
					}
					else
					{
						ImGui::Text("Session Destruction Progress:                            %llu/%llu", status.saveload_current, status.saveload_max);
						ImGui::ProgressBar((float)status.gsaveload, ImVec2(0.0f, 0.0f));
					}
				}
				else{
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
						ImGui::Text("Undefined Tests:   %llu", status.undefinedTests);
						ImGui::Text("Pruned Tests:      %llu", status.prunedTests);
						ImGui::Text("Runtime:           %s", Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(status.runtime).count()).c_str());

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
									destroying = true;
									std::thread th(std::bind(&Session::DestroySession, session));
									th.detach();
								}
							} else if (session->Loaded() && session->Running() == false && session->Finished() == false) {
								if (ImGui::Button("Close")) {
									loginfo("UI window has been closed, exiting application.");
									destroying = true;
									std::thread th(std::bind(&Session::DestroySession, session));
									th.detach();
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
									std::thread th(std::bind(&Session::StopSession, session, std::placeholders::_1, std::placeholders::_2), true, false);
									th.detach();
								}
								ImGui::SameLine();
								if (ImGui::Button("Abort")) {
									std::thread th(std::bind(&Session::StopSession, session, std::placeholders::_1, std::placeholders::_2), false, false);
									th.detach();
								}
								ImGui::SameLine();
								if (!interrupted) {
									if (ImGui::Button("Pause")) {
										interrupted = true;
										//std::thread th(std::bind(&Session::PauseSession, session));
										//th.detach();

										// no async calls, otherwise we are unlock from another thread which isn't allowed
										session->PauseSession();
									}
								} else {
									if (ImGui::Button("Resume")) {
										interrupted = false;
										//std::thread th(std::bind(&Session::ResumeSession, session));
										//th.detach();
										session->ResumeSession();
									}
								}
							}
						}
					}
				}
				ImGui::End();
			}

			// show window with advanced stats
			if (showAdvancedWindow && !destroying) {
				static bool wopen = true;
				ImGui::Begin("Advanced", &wopen);
				if (wopen) {
					static size_t hashmapSize;
					session->UI_GetHashmapInformation(hashmapSize);
					if (displayadd) {
						ImGui::SeparatorText("Exclusion Tree");
						ImGui::Text("Depth:                   %lld", status.excl_depth);
						ImGui::Text("Nodes:                   %llu", status.excl_nodecount);
						ImGui::Text("Leaves:                  %llu", status.excl_leafcount);

						ImGui::SeparatorText("Task Controller");
						ImGui::Text("Waiting [All]:           %d", status.task_waiting);
						ImGui::Text("Waiting [Medium]:        %d", status.task_waiting_medium);
						ImGui::Text("Waiting [Light]:         %d", status.task_waiting_light);
						ImGui::Text("Completed:               %llu", status.task_completed);

						ImGui::SeparatorText("Execution Handler");
						ImGui::Text("Waiting:                 %d", status.exec_waiting);
						ImGui::Text("Initialized:             %d", status.exec_initialized);
						ImGui::Text("Running:                 %d", status.exec_running);
						ImGui::Text("Stopping:                %d", status.exec_stopping);

						ImGui::SeparatorText("Generation");
						ImGui::Text("Generated Inputs:        %lld", status.gen_generatedInputs);
						ImGui::Text("Excluded with Prefix:    %lld", status.gen_generatedWithPrefix);
						ImGui::Text("Excluded Approximation:  %lld", status.gen_excludedApprox);
						ImGui::Text("Generation Fails:        %lld", status.gen_generationFails);
						ImGui::Text("Add Test Fails:          %lld", status.gen_addtestfails);
						ImGui::Text("Failure Rate:            %llf", status.gen_failureRate);

						ImGui::SeparatorText("Test exit stats");
						ImGui::Text("Natural:                 %llu", status.exitstats.natural);
						ImGui::Text("Last Input:              %llu", status.exitstats.lastinput);
						ImGui::Text("Terminated:              %llu", status.exitstats.terminated);
						ImGui::Text("Timeout:                 %llu", status.exitstats.timeout);
						ImGui::Text("Fragment Timeout:        %llu", status.exitstats.fragmenttimeout);
						ImGui::Text("Memory:                  %llu", status.exitstats.memory);
						ImGui::Text("Pipe:                    %llu", status.exitstats.pipe);
						ImGui::Text("Init error:              %llu", status.exitstats.initerror);
						ImGui::Text("Repeated Inputs:         %llu", status.exitstats.repeat);

						ImGui::SeparatorText("Internals");
						ImGui::Text("Hashmap size:            %lu", hashmapSize);

						ImGui::SeparatorText("Actions");
						if (ImGui::SmallButton("Check Excl")) {
							std::thread th(std::bind(&Session::UI_CheckForAlternatives, session));
							th.detach();
						}
						if (ImGui::SmallButton("Report Object Status")) {
							std::thread th(std::bind(&Session::UI_GetDatabaseObjectStatus, session));
							th.detach();
						}
					} else {
						ImGui::Text("Not available");
					}
				}
				ImGui::End();
			}

			if (showTaskWindow && !destroying) {
				static bool wopen = true;
				ImGui::Begin("Task Window", &wopen);
				if (wopen) {
					static UI::UITaskController controller;
					if (controller.Initialized() == false)
						session->UI_GetTaskController(controller);
					if (controller.Initialized()) {
						controller.LockExecutedTasks();
						auto itr = controller.beginExecutedTasks();
						while (itr != controller.endExecutedTasks()) {
							ImGui::Text("%s: %lld", itr->first.c_str(), itr->second);
							itr++;
						}
						controller.UnlockExecutedTasks();
					}
				}
				ImGui::End();
			}

			// show inpu stats
			if (showInput && !destroying) {
				static bool wopen = true;
				ImGui::Begin("Input Information", &wopen);
				if (wopen) {
					if (inputinfo.Initialized()) {
						static bool listview = false;

						ImGui::SeparatorText("General Information");
						ImGui::Text("Input ID:                   %s", Utility::GetHexFill(inputinfo._inputID).c_str());
						ImGui::Text("Parent ID:                  %s", Utility::GetHexFill(inputinfo._parentInput).c_str());
						ImGui::Text("Generation ID:              %s", Utility::GetHexFill(inputinfo._generationID).c_str());
						ImGui::Text("Derived Inputs:             %llu", inputinfo._derivedInputs);
						ImGui::Text("Flags:                      %s", Utility::GetHexFill(inputinfo._flags).c_str());
						ImGui::Text("Has Finished:               %s", inputinfo._hasfinished ? "true" : "false");
						ImGui::NewLine();

						ImGui::SeparatorText("Generation Information");
						ImGui::Text("Generation Time:            %lld ns", inputinfo._generationTime.count());
						ImGui::Text("Generation Length:          %lld", inputinfo._naturallength);
						ImGui::Text("Trimmed Length:             %lld", inputinfo._trimmedlength);
						ImGui::NewLine();

						ImGui::SeparatorText("Test Information");
						ImGui::Text("Execution Time:             %lld ns", inputinfo._executionTime.count());
						ImGui::Text("Primary Score:              %.2f", inputinfo._primaryScore);
						ImGui::Text("Secondary Score:            %.2f", inputinfo._secondaryScore);
						ImGui::Text("Exitcode:                   %d", inputinfo._exitcode);
						ImGui::Text("ExitReason:                   %d", inputinfo._exitreason);

						ImGui::TextUnformatted("Command Line Args:");
						ImGui::TextWrapped("%s", inputinfo._cmdArgs.c_str());
						ImGui::TextUnformatted("Script Args:");
						ImGui::TextWrapped("%s", inputinfo._cmdArgs.c_str());
						ImGui::NewLine();

						ImGui::SeparatorText("Input");
						ImGui::SameLine();
						if (ImGui::SmallButton("Copy")) {
							if (listview) {
								ImGui::GetPlatformIO().Platform_SetClipboardTextFn(context, inputinfo._inputlist.c_str());
							} else
								ImGui::GetPlatformIO().Platform_SetClipboardTextFn(context, inputinfo._inputconcat.c_str());
						}
						ImGui::Checkbox("Show as list", &listview);
						if (listview)
							ImGui::TextWrapped("%s", inputinfo._inputlist.c_str());
						else
							ImGui::TextWrapped("%s", inputinfo._inputconcat.c_str());
						ImGui::NewLine();
						ImGui::SeparatorText("Output");
						ImGui::TextWrapped("%s", inputinfo._output.c_str());
					} else {
						ImGui::Text("Not available");
					}
				}
				ImGui::End();
			}

			if (showThreadStatus && !destroying) {
				static bool wopen = true;
				ImGui::Begin("Thread Status", &wopen);
				if (wopen) {
					auto toStringExec = [](ExecHandlerStatus stat) {
						switch (stat) {
						case ExecHandlerStatus::None:
							return "None";
						case ExecHandlerStatus::Exitted:
							return "Exitted";
						case ExecHandlerStatus::HandlingTests:
							return "HandlingTests";
						case ExecHandlerStatus::MainLoop:
							return "MainLoop";
						case ExecHandlerStatus::Sleeping:
							return "Sleeping";
						case ExecHandlerStatus::StartingTests:
							return "StartingTests";
						case ExecHandlerStatus::Waiting:
							return "Waiting";
						case ExecHandlerStatus::KillingProcessMemory:
							return "KillingProcessMemory";
						case ExecHandlerStatus::WriteFragment:
							return "WriteFragment";
						case ExecHandlerStatus::KillingProcessTimeout:
							return "KillingProcessTimeout";
						case ExecHandlerStatus::StoppingTest:
							return "StoppingTest";
						}
						return "None";
					};
					static std::vector<TaskController::ThreadStatus> stati;
					static std::vector<const char*> statiName;
					static std::vector<std::string> statiTime;
					static ExecHandlerStatus execstatus;
					session->UI_GetThreadStatus(stati, statiName, statiTime);
					ImGui::SeparatorText("TaskController");
					for (size_t i = 0; i < stati.size(); i++) {
						switch (stati[i]) {
						case TaskController::ThreadStatus::Initializing:
							ImGui::Text("Status: Initializing");
							break;
						case TaskController::ThreadStatus::Running:
							ImGui::Text("Status: Running | %s | %s", statiName[i], statiTime[i].c_str());
							break;
						case TaskController::ThreadStatus::Waiting:
							ImGui::Text("Status: Waiting");
							break;
						default:
							ImGui::Text("Status: None");
							break;
						}
					}
					ImGui::SeparatorText("ExecutionHandler");
					execstatus = session->UI_GetExecHandlerStatus();
					ImGui::Text("Status: %s", toStringExec(execstatus));
				}
				ImGui::End();
			}

			// show window with information about generations
			if (showGeneration && !destroying) {
				static bool wopen = true;
				ImGui::Begin("Generation", &wopen);
				if (wopen && session->Loaded()) {
					static std::vector<std::pair<FormID, int32_t>> generations;
					static size_t numgenerations;
					static std::pair<FormID, FormID> selection = { 0, 0 };
					static bool changed = false;
					if (!ActiveGeneration.Initialized()) {
						changed = true;
						session->UI_GetGenerations(generations, numgenerations);
						session->UI_GetCurrentGeneration(ActiveGeneration);
						if (ActiveGeneration.Initialized()) {
							selection = { ActiveGeneration.GetFormID(), ActiveGeneration.GetGenerationNumber() };
						}
					}

					if (ImGui::Button("Switch to current generation")) {
						session->UI_GetCurrentGeneration(ActiveGeneration);
						if (ActiveGeneration.Initialized()) {
							selection = { ActiveGeneration.GetFormID(), ActiveGeneration.GetGenerationNumber() };
							changed = true;
						}
					}

					static std::string preview;
					if (changed) {
						changed = false;
						if (ActiveGeneration.Initialized())
							preview = std::to_string(ActiveGeneration.GetGenerationNumber());
						else
							preview = "None";
					}

					if (ImGui::BeginCombo("Generations", preview.c_str())) {
						session->UI_GetGenerations(generations, numgenerations);
						if (numgenerations == 0) {
							const bool is_selected = true;
							ImGui::Selectable("None", is_selected);
						} else {
							for (size_t i = 0; i < numgenerations; i++) {
								const bool is_selected = (selection.first == generations[i].first);
								if (ImGui::Selectable(std::to_string(generations[i].second).c_str(), is_selected)) {
									session->UI_GetGeneration(generations[i].first, ActiveGeneration);
									changed = true;
								}
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					if (ActiveGeneration.Initialized()) {
						// update stuff
						static std::vector<UI::UIInput> sources;
						ActiveGeneration.GetSources(sources);
						static int64_t numsources;
						numsources = ActiveGeneration.GetNumberOfSources();
						ActiveGeneration.GetDDControllers(ddcontrollers, numddcontrollers);

						// build ui
						ImGui::Text("Generation Number:          %lld", ActiveGeneration.GetGenerationNumber());
						ImGui::Text("Total Size:                 %lld", ActiveGeneration.GetSize());
						ImGui::Text("Generated Size: %lld", ActiveGeneration.GetGeneratedSize());
						ImGui::SameLine(300);
						ImGui::Text("Target Size:    %lld", ActiveGeneration.GetTargetSize());
						ImGui::Text("DD Size:                    %lld", ActiveGeneration.GetDDSize());
						ImGui::Text("Active Inputs:              %lld", ActiveGeneration.GetActiveInputs());
						ImGui::Text("Number of DD controllers:   %u", numddcontrollers);
						ImGui::SameLine(300);
						int activedd = 0;
						for (auto& dd : ddcontrollers) {
							if (!dd.Finished())
								activedd++;
						}
						ImGui::Text("Active:         %d", activedd);
						ImGui::Text("Runtime:                    %s", Logging::FormatTime(ActiveGeneration.GetRuntime().count() / 1000).c_str());
						ImGui::NewLine();
						ImGui::Text("Source Inputs: %lld", numsources);
						static ImGuiTableFlags flags =
							ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;

						// do something with sources
						if (ImGui::BeginTable("Sources", 10, flags, ImVec2(0.0f, 0.0f), 0.0f)) {
							ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputID);
							ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputLength);
							ImGui::TableSetupColumn("Primary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputPrimaryScore);
							ImGui::TableSetupColumn("Secondary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputSecondaryScore);
							ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 120.0f, UI::UIInput::ColumnID::InputResult);
							ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIInput::ColumnID::InputFlags);
							ImGui::TableSetupColumn("Generation", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIInput::ColumnID::InputGenerationNum);
							ImGui::TableSetupColumn("Derived Inputs", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIInput::ColumnID::InputDerivedNum);
							ImGui::TableSetupColumn("ExecTime in ns", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputExecTime);
							ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputAction);
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableHeadersRow();

							// DO SOME SORTING

							// use clipper for large vertical lists
							ImGuiListClipper clipper;
							clipper.Begin((int32_t)numsources <= sources.size() ? (int32_t)numsources : (int32_t)sources.size());
							while (clipper.Step()) {
								for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
									auto item = &sources[i];
									ImGui::PushID((uint32_t)item->id);
									ImGui::TableNextRow();
									ImGui::TableNextColumn();
									ImGui::Text("%8llx", item->id);
									ImGui::TableNextColumn();
									ImGui::Text("%10lu", item->length);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->primaryScore);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->secondaryScore);
									ImGui::TableNextColumn();
									if (item->result == UI::Result::Failing)
										ImGui::Text("Failing");
									else if (item->result == UI::Result::Passing)
										ImGui::Text("Passing");
									else if (item->result == UI::Result::Running)
										ImGui::Text("Running");
									else
										ImGui::Text("Unfinished");
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(Utility::GetHexFill(item->flags).c_str());
									ImGui::TableNextColumn();
									ImGui::Text("%5d", item->generationNumber);
									ImGui::TableNextColumn();
									ImGui::Text("%6ld", item->derivedInputs);
									ImGui::TableNextColumn();
									ImGui::Text("%15lld", item->exectime.count());
									ImGui::TableNextColumn();
									if (ImGui::SmallButton("Replay")) {
										std::thread th(std::bind(&Session::Replay, session, std::placeholders::_1, std::placeholders::_2), item->id, &inputinfo);
										th.detach();
									}
									ImGui::SameLine();
									if (ImGui::SmallButton("Info")) {
										showInput = true;
										std::thread th([id = item->id]() { session->UI_GetInputInformation(inputinfo, id); });
										th.detach();
									}
									ImGui::PopID();
								}
							}
							ImGui::EndTable();
						}
					}
				} else
					session->UI_GetCurrentGeneration(ActiveGeneration);
				ImGui::End();
			}

			if (showPositiveInputs && !destroying)
			{
				static bool wopen = true;
				ImGui::Begin("Positive Inputs", &wopen);
				if (wopen) {
					static std::vector<UI::UIInput> elements;

					// GET ITEMS FROM SESSION
					session->UI_GetPositiveInputs(elements, 0);

					static char buf[32];
					ImGui::InputText("Number of rows", buf, 32, ImGuiInputTextFlags_CharsDecimal);
					static int32_t rownum = 0;
					try {
						rownum = std::atoi(buf);
					} catch (std::exception&) {
						rownum = 10;
					}

					// construct table
					static ImGuiTableFlags flags =
						ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;
					//PushStyleCompact
					//...
					//PopStyleCompact
					if (ImGui::BeginTable("postable", 10, flags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * rownum), 0.0f)) {
						ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputID);
						ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputLength);
						ImGui::TableSetupColumn("Primary Score", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputPrimaryScore);
						ImGui::TableSetupColumn("Secondary Score", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputSecondaryScore);
						ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 120.0f, UI::UIInput::ColumnID::InputResult);
						ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIInput::ColumnID::InputFlags);
						ImGui::TableSetupColumn("Generation", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputGenerationNum);
						ImGui::TableSetupColumn("Derived Inputs", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputDerivedNum);
						ImGui::TableSetupColumn("ExecTime in ns", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputExecTime);
						ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputAction);
						ImGui::TableSetupScrollFreeze(0, 1);
						ImGui::TableHeadersRow();

						// DO SOME SORTING

						// use clipper for large vertical lists
						ImGuiListClipper clipper;
						clipper.Begin((int32_t)elements.size());
						while (clipper.Step()) {
							for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
								auto item = &elements[i];
								ImGui::PushID((uint32_t)item->id);
								ImGui::TableNextRow();
								ImGui::TableNextColumn();
								ImGui::Text("%8llx", item->id);
								ImGui::TableNextColumn();
								ImGui::Text("%10llu", item->length);
								ImGui::TableNextColumn();
								ImGui::Text("%10.2f", item->primaryScore);
								ImGui::TableNextColumn();
								ImGui::Text("%10.2f", item->secondaryScore);
								ImGui::TableNextColumn();
								if (item->result == UI::Result::Failing)
									ImGui::Text("Failing");
								else if (item->result == UI::Result::Passing)
									ImGui::Text("Passing");
								else if (item->result == UI::Result::Running)
									ImGui::Text("Running");
								else
									ImGui::Text("Unfinished");
								ImGui::TableNextColumn();
								ImGui::TextUnformatted(Utility::GetHexFill(item->flags).c_str());
								ImGui::TableNextColumn();
								ImGui::Text("%6d", item->generationNumber);
								ImGui::TableNextColumn();
								ImGui::Text("%6lld", item->derivedInputs);
								ImGui::TableNextColumn();
								ImGui::Text("%15lld", item->exectime.count());
								ImGui::TableNextColumn();
								if (ImGui::SmallButton("Replay")) {
									std::thread th(std::bind(&Session::Replay, session, std::placeholders::_1, std::placeholders::_2), item->id, &inputinfo);
									th.detach();
								}
								ImGui::SameLine();
								if (ImGui::SmallButton("Info")) {
									showInput = true;
									std::thread th([id = item->id]() { session->UI_GetInputInformation(inputinfo, id); });
									th.detach();
								}
								ImGui::SameLine();
								if (ImGui::SmallButton("DeltaDebug")) {
									std::thread th(std::bind(&Session::UI_StartDeltaDebugging, session, std::placeholders::_1), item->id);
									th.detach();
								}
								ImGui::PopID();
							}
						}
						ImGui::EndTable();
					}
				}
				ImGui::End();
			}

			if (showLastGenerated && !destroying) {
				static bool wopen = true;
				ImGui::Begin("Last Generated", &wopen);
				if (wopen) {
					static std::vector<UI::UIInput> elements;

					// GET ITEMS FROM SESSION
					session->UI_GetLastRunInputs(elements, 0);

					// construct table
					static ImGuiTableFlags flags =
						ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;
					//PushStyleCompact
					//...
					//PopStyleCompact
					if (ImGui::BeginTable("postable", 10, flags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 11), 0.0f)) {
						ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputID);
						ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputLength);
						ImGui::TableSetupColumn("Primary Score", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputPrimaryScore);
						ImGui::TableSetupColumn("Secondary Score", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputSecondaryScore);
						ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 120.0f, UI::UIInput::ColumnID::InputResult);
						ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIInput::ColumnID::InputFlags);
						ImGui::TableSetupColumn("Generation", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputGenerationNum);
						ImGui::TableSetupColumn("Derived Inputs", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputDerivedNum);
						ImGui::TableSetupColumn("ExecTime in ns", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputExecTime);
						ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputAction);
						ImGui::TableSetupScrollFreeze(0, 1);
						ImGui::TableHeadersRow();

						// DO SOME SORTING

						// use clipper for large vertical lists
						ImGuiListClipper clipper;
						clipper.Begin((int32_t)elements.size());
						while (clipper.Step()) {
							for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
								auto item = &elements[i];
								ImGui::PushID((uint32_t)item->id);
								ImGui::TableNextRow();
								ImGui::TableNextColumn();
								ImGui::Text("%8llx", item->id);
								ImGui::TableNextColumn();
								ImGui::Text("%10llu", item->length);
								ImGui::TableNextColumn();
								ImGui::Text("%10.2f", item->primaryScore);
								ImGui::TableNextColumn();
								ImGui::Text("%10.2f", item->secondaryScore);
								ImGui::TableNextColumn();
								if (item->result == UI::Result::Failing)
									ImGui::Text("Failing");
								else if (item->result == UI::Result::Passing)
									ImGui::Text("Passing");
								else if (item->result == UI::Result::Running)
									ImGui::Text("Running");
								else
									ImGui::Text("Unfinished");
								ImGui::TableNextColumn();
								ImGui::TextUnformatted(Utility::GetHexFill(item->flags).c_str());
								ImGui::TableNextColumn();
								ImGui::Text("%6d", item->generationNumber);
								ImGui::TableNextColumn();
								ImGui::Text("%6lld", item->derivedInputs);
								ImGui::TableNextColumn();
								ImGui::Text("%15lld", item->exectime.count());
								ImGui::TableNextColumn();
								if (ImGui::SmallButton("Replay")) {
									std::thread th(std::bind(&Session::Replay, session, std::placeholders::_1, std::placeholders::_2), item->id, &inputinfo);
									th.detach();
								}
								ImGui::SameLine();
								if (ImGui::SmallButton("Info")) {
									showInput = true;
									std::thread th([id = item->id]() { session->UI_GetInputInformation(inputinfo, id); });
									th.detach();
								}
								ImGui::SameLine();
								if (ImGui::SmallButton("DeltaDebug")) {
									std::thread th(std::bind(&Session::UI_StartDeltaDebugging, session, std::placeholders::_1), item->id);
									th.detach();
								}
								ImGui::PopID();
							}
						}
						ImGui::EndTable();
					}
				}
				ImGui::End();
			}

			// show window with information about the best generated inputs
			if (showTopK && !destroying) {
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

						for (int i = 0; i < MAX_ITEMS && i < (int32_t)elements.size(); i++)
							imElements[i] = elements[i];

						static char buf[32];
						ImGui::InputText("Number of rows", buf, 32, ImGuiInputTextFlags_CharsDecimal);
						static int32_t rownum = 0;
						try {
							rownum = std::atoi(buf);
						} catch (std::exception&) {
							rownum = 10;
						}

						// construct table
						static ImGuiTableFlags flags =
							ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;
						//PushStyleCompact
						//...
						//PopStyleCompact
						if (ImGui::BeginTable("itemtable", 10, flags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * rownum), 0.0f)) {
							ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputID);
							ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputLength);
							ImGui::TableSetupColumn("Primary Score", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputPrimaryScore);
							ImGui::TableSetupColumn("Secondary Score", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputSecondaryScore);
							ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 120.0f, UI::UIInput::ColumnID::InputResult);
							ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIInput::ColumnID::InputFlags);
							ImGui::TableSetupColumn("Generation", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputGenerationNum);
							ImGui::TableSetupColumn("Derived Inputs", ImGuiTableColumnFlags_WidthFixed, 70.0f, UI::UIInput::ColumnID::InputDerivedNum);
							ImGui::TableSetupColumn("ExecTime in ns", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputExecTime);
							ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputAction);
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableHeadersRow();

							// DO SOME SORTING

							// use clipper for large vertical lists
							ImGuiListClipper clipper;
							clipper.Begin(imElements.size());
							while (clipper.Step()) {
								for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
									auto item = &imElements[i];
									ImGui::PushID((uint32_t)item->id);
									ImGui::TableNextRow();
									ImGui::TableNextColumn();
									ImGui::Text("%8llx", item->id);
									ImGui::TableNextColumn();
									ImGui::Text("%10lu", item->length);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->primaryScore);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->secondaryScore);
									ImGui::TableNextColumn();
									if (item->result == UI::Result::Failing)
										ImGui::Text("Failing");
									else if (item->result == UI::Result::Passing)
										ImGui::Text("Passing");
									else if (item->result == UI::Result::Running)
										ImGui::Text("Running");
									else
										ImGui::Text("Unfinished");
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(Utility::GetHexFill(item->flags).c_str());
									ImGui::TableNextColumn();
									ImGui::Text("%6d", item->generationNumber);
									ImGui::TableNextColumn();
									ImGui::Text("%6ld", item->derivedInputs);
									ImGui::TableNextColumn();
									ImGui::Text("%15lld", item->exectime.count());
									ImGui::TableNextColumn();
									if (ImGui::SmallButton("Replay")) {
										std::thread th(std::bind(&Session::Replay, session, std::placeholders::_1, std::placeholders::_2), item->id, &inputinfo);
										th.detach();
									}
									ImGui::SameLine();
									if (ImGui::SmallButton("Info")) {
										showInput = true;
										std::thread th([id = item->id]() { session->UI_GetInputInformation(inputinfo, id); });
										th.detach();
									}
									ImGui::SameLine();
									if (ImGui::SmallButton("DeltaDebug")) {
										std::thread th(std::bind(&Session::UI_StartDeltaDebugging, session, std::placeholders::_1), item->id);
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

			// show window with information about function execution times
			if (showProfiling && !destroying) {
				static bool wopen = true;
				ImGui::Begin("Profiling", &wopen);
				if (wopen) {
					static int32_t MAX_ITEMS = 100;
					if (displayadd) {
						static std::vector<UI::UIExecTime> dimElements(MAX_ITEMS);
						//if (dimElements.size() == (size_t)0)
						//	dimElements.resize(MAX_ITEMS);
						// Get items
						int32_t count = 0;
						auto itr = Profile::exectimes.begin();
						while (count < 100 && itr != Profile::exectimes.end()) {
							Profile::ExecTime* exec = itr->second;
							dimElements[count].ns = exec->exectime;
							dimElements[count].executions = exec->executions;
							dimElements[count].average = exec->average;
							dimElements[count].time = exec->lastexec;
							dimElements[count].func = exec->functionName;
							dimElements[count].file = exec->fileName;
							dimElements[count].usermes = exec->usermessage;
							dimElements[count].tmpid = count;
							count++;
							itr++;
						}

						static char buf[32];
						ImGui::InputText("Number of rows", buf, 32, ImGuiInputTextFlags_CharsDecimal);
						static int32_t rownum = 0;
						try {
							rownum = std::atoi(buf);
						} catch (std::exception&) {
							rownum = 10;
						}

						// construct table
						static ImGuiTableFlags flags =
							ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;
						//PushStyleCompact
						//...
						//PopStyleCompact
						if (ImGui::BeginTable("itemtable", 7, flags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * rownum), 0.0f)) {
							ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIExecTime::ColumnID::ExecTimeFile);
							ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIExecTime::ColumnID::ExecTimeFunction);
							ImGui::TableSetupColumn("Exec Time", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIExecTime::ColumnID::ExecTimeNano);
							ImGui::TableSetupColumn("Average Time", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIExecTime::ColumnID::ExecTimeAverage);
							ImGui::TableSetupColumn("Last executed", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIExecTime::ColumnID::ExecTimeLast);
							ImGui::TableSetupColumn("Executions", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIExecTime::ColumnID::ExecTimeExecutions);
							ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIExecTime::ColumnID::ExecTimeUserMes);
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableHeadersRow();

							// DO SOME SORTING

							// use clipper for large vertical lists
							ImGuiListClipper clipper;
							clipper.Begin(count < (int32_t)dimElements.size() ? count : (int32_t)dimElements.size());
							while (clipper.Step()) {
								for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
									auto item = &dimElements[i];
									ImGui::PushID((uint32_t)item->tmpid);
									ImGui::TableNextRow();
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(item->file.c_str());
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(item->func.c_str());
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(item->ns).count()).c_str());
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(item->average).count()).c_str());
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - item->time).count()).c_str());
									ImGui::TableNextColumn();
									ImGui::Text("%llu", item->executions);
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(item->usermes.c_str());
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

			// delta debugging
			if (showDeltaDebugging && !destroying) {
				static bool wopen = true;
				ImGui::Begin("Delta Debugging", &wopen);
				if (wopen) {
					static int32_t ITEMS = 100;
					static std::vector<UI::UIInput> inputs(ITEMS);
					static size_t numinputs;
					static std::vector<UI::UIDDResult> results(ITEMS);
					static size_t numresults;
					static UI::UIInput input;
					static UI::UIInput origInput;
					static UI::UIDeltaDebugging dd;
					static bool changed = false;
					// try to find active deltadebugging
					if (ActiveGeneration.Initialized() && dd.Initialized() == false) {
						if (!updatedddcontrollers) {
							ActiveGeneration.GetDDControllers(ddcontrollers, numddcontrollers);
							updatedddcontrollers = true;
						}
						changed = true;
						if (ddcontrollers.size() > 0)
							dd = ddcontrollers[0];
					}

					static std::string preview;
					if (changed) {
						changed = false;
						if (dd.Initialized())
							preview = Utility::GetHexFill(dd.GetFormID());
						else
							preview = "None";
					}

					if (ActiveGeneration.Initialized()) {
						if (ImGui::BeginCombo("Active DD Sessions", preview.c_str())) {
							if (!updatedddcontrollers) {
								ActiveGeneration.GetDDControllers(ddcontrollers, numddcontrollers);
								updatedddcontrollers = true;
							}
							if (numddcontrollers == 0) {
								const bool is_selected = true;
								ImGui::Selectable("None", is_selected);
								if (is_selected)
									ImGui::SetItemDefaultFocus();
							} else {
								for (size_t i = 0; i < numddcontrollers; i++) {
									const bool is_selected = (dd.GetFormID() == ddcontrollers[i].GetFormID());
									if (ImGui::Selectable(Utility::GetHexFill(ddcontrollers[i].GetFormID()).c_str(), is_selected)) {
										dd = ddcontrollers[i];
										changed = true;
									}
									if (is_selected)
										ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}
					}

					if (dd.Initialized() && ActiveGeneration.Initialized()) {
						// update stuff
						dd.GetInput(input);
						dd.GetOriginalInput(origInput);
						dd.GetActiveInputs(inputs, numinputs);
						dd.GetResults(results, numresults);

						// build ui
						ImGui::Text("Finished: %s", dd.Finished() ? "True" : "False");
						ImGui::Text("Goal:  %s", dd.GetGoal().c_str());
						ImGui::SameLine(300);
						ImGui::Text("Mode:  %s", dd.GetMode().c_str());
						ImGui::Text("Level: %d", dd.GetLevel());
						ImGui::SameLine(300);
						ImGui::Text("Total Tests: %d", dd.GetTotalTests());
						ImGui::Text("Skipped Tests: %d", dd.GetSkippedTests());
						ImGui::SameLine(300);
						ImGui::Text("Prefix Tests: %d", dd.GetPrefixTests());
						ImGui::SameLine(600);
						ImGui::Text("Invalid Tests: %d", dd.GetInvalidTests());
						ImGui::Text("Tests done: %d", dd.GetTests());
						ImGui::SameLine(300);
						ImGui::Text("Tests Remaining: %d", dd.GetTestsRemaining());
						ImGui::Text("Current Batchident: %llu", dd.GetBatchIdent());
						ImGui::Text("Runtime:      %s", Logging::FormatTime(dd.GetRuntime().count() / 1000).c_str());
						ImGui::NewLine();
						static ImGuiTableFlags flags =
							ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;

						// do something with original _input
						// // active inputs table
						if (ImGui::BeginTable("Original Input", 8, flags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 2), 0.0f)) {
							ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputID);
							ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputLength);
							ImGui::TableSetupColumn("Primary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputPrimaryScore);
							ImGui::TableSetupColumn("Secondary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputSecondaryScore);
							ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 120.0f, UI::UIInput::ColumnID::InputResult);
							ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIInput::ColumnID::InputFlags);
							ImGui::TableSetupColumn("ExecTime in ns", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputExecTime);
							ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputAction);
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableHeadersRow();

							// DO SOME SORTING

							// use clipper for large vertical lists
							ImGuiListClipper clipper;
							clipper.Begin(1);
							while (clipper.Step()) {
								for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
									auto item = &origInput;
									ImGui::PushID((uint32_t)item->id);
									ImGui::TableNextRow();
									ImGui::TableNextColumn();
									ImGui::Text("%8llx", item->id);
									ImGui::TableNextColumn();
									ImGui::Text("%10lu", item->length);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->primaryScore);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->secondaryScore);
									ImGui::TableNextColumn();
									if (item->result == UI::Result::Failing)
										ImGui::Text("Failing");
									else if (item->result == UI::Result::Passing)
										ImGui::Text("Passing");
									else if (item->result == UI::Result::Running)
										ImGui::Text("Running");
									else
										ImGui::Text("Unfinished");
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(Utility::GetHexFill(item->flags).c_str());
									ImGui::TableNextColumn();
									ImGui::Text("%15lld", item->exectime.count());
									ImGui::TableNextColumn();
									if (ImGui::SmallButton("Replay")) {
										std::thread th(std::bind(&Session::Replay, session, std::placeholders::_1, std::placeholders::_2), item->id, &inputinfo);
										th.detach();
									}
									ImGui::SameLine();
									if (ImGui::SmallButton("Info")) {
										showInput = true;
										std::thread th([id = item->id]() { session->UI_GetInputInformation(inputinfo, id); });
										th.detach();
									}
									ImGui::PopID();
								}
							}
							ImGui::EndTable();
						}
						// do something with currrent _input
						if (ImGui::BeginTable("Original Input", 8, flags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 2), 0.0f)) {
							ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputID);
							ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputLength);
							ImGui::TableSetupColumn("Primary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputPrimaryScore);
							ImGui::TableSetupColumn("Secondary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputSecondaryScore);
							ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 120.0f, UI::UIInput::ColumnID::InputResult);
							ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIInput::ColumnID::InputFlags);
							ImGui::TableSetupColumn("ExecTime in ns", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputExecTime);
							ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputAction);
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableHeadersRow();

							// DO SOME SORTING

							// use clipper for large vertical lists
							ImGuiListClipper clipper;
							clipper.Begin(1);
							while (clipper.Step()) {
								for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
									auto item = &input;
									ImGui::PushID((uint32_t)item->id);
									ImGui::TableNextRow();
									ImGui::TableNextColumn();
									ImGui::Text("%8llx", item->id);
									ImGui::TableNextColumn();
									ImGui::Text("%10lu", item->length);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->primaryScore);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->secondaryScore);
									ImGui::TableNextColumn();
									if (item->result == UI::Result::Failing)
										ImGui::Text("Failing");
									else if (item->result == UI::Result::Passing)
										ImGui::Text("Passing");
									else if (item->result == UI::Result::Running)
										ImGui::Text("Running");
									else
										ImGui::Text("Unfinished");
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(Utility::GetHexFill(item->flags).c_str());
									ImGui::TableNextColumn();
									ImGui::Text("%15lld", item->exectime.count());
									ImGui::TableNextColumn();
									if (ImGui::SmallButton("Replay")) {
										std::thread th(std::bind(&Session::Replay, session, std::placeholders::_1, std::placeholders::_2), item->id, &inputinfo);
										th.detach();
									}
									ImGui::SameLine();
									if (ImGui::SmallButton("Info")) {
										showInput = true;
										std::thread th([id = item->id]() { session->UI_GetInputInformation(inputinfo, id); });
										th.detach();
									}
									ImGui::PopID();
								}
							}
							ImGui::EndTable();
						}
						// results table
						if (ImGui::BeginTable("Results", 11, flags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 10), 0.0f)) {
							ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIDDResult::ColumnID::InputID);
							ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIDDResult::ColumnID::InputLength);
							ImGui::TableSetupColumn("Primary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIDDResult::ColumnID::InputPrimaryScore);
							ImGui::TableSetupColumn("Secondary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIDDResult::ColumnID::InputSecondaryScore);
							ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 120.0f, UI::UIDDResult::ColumnID::InputResult);
							ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIDDResult::ColumnID::InputFlags);
							ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIDDResult::ColumnID::InputAction);
							ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIDDResult::ColumnID::InputLevel);
							ImGui::TableSetupColumn("Loss (Primary)", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIDDResult::ColumnID::InputLossPrimary);
							ImGui::TableSetupColumn("Loss (Secondary)", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIDDResult::ColumnID::InputLossSecondary);
							ImGui::TableSetupColumn("ExecTime in ns", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIDDResult::ColumnID::InputExecTime);
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableHeadersRow();

							// DO SOME SORTING

							// use clipper for large vertical lists
							ImGuiListClipper clipper;
							clipper.Begin((int32_t)(numresults < results.size() ? numresults : results.size()));
							while (clipper.Step()) {
								for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
									auto item = &results[i];
									ImGui::PushID((uint32_t)item->id);
									ImGui::TableNextRow();
									ImGui::TableNextColumn();
									ImGui::Text("%8llx", item->id);
									ImGui::TableNextColumn();
									ImGui::Text("%10lu", item->length);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->primaryScore);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->secondaryScore);
									ImGui::TableNextColumn();
									if (item->result == UI::Result::Failing)
										ImGui::Text("Failing");
									else if (item->result == UI::Result::Passing)
										ImGui::Text("Passing");
									else if (item->result == UI::Result::Running)
										ImGui::Text("Running");
									else
										ImGui::Text("Unfinished");
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(Utility::GetHexFill(item->flags).c_str());
									ImGui::TableNextColumn();
									if (ImGui::SmallButton("Replay")) {
										std::thread th(std::bind(&Session::Replay, session, std::placeholders::_1, std::placeholders::_2), item->id, &inputinfo);
										th.detach();
									}
									ImGui::SameLine();
									if (ImGui::SmallButton("Info")) {
										showInput = true;
										std::thread th([id = item->id]() { session->UI_GetInputInformation(inputinfo, id); });
										th.detach();
									}
									ImGui::TableNextColumn();
									ImGui::Text("%8d", item->level);
									ImGui::TableNextColumn();
									ImGui::Text("%2.4f", item->primaryLoss);
									ImGui::TableNextColumn();
									ImGui::Text("%2.4f", item->secondaryLoss);
									ImGui::TableNextColumn();
									ImGui::Text("%15lld", item->exectime.count());
									ImGui::PopID();
								}
							}
							ImGui::EndTable();
						}

						// active inputs table
						if (ImGui::BeginTable("Active Inputs", 8, flags, ImVec2(0.0f, 0.0f), 0.0f)) {
							ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 30.f, UI::UIInput::ColumnID::InputID);
							ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputLength);
							ImGui::TableSetupColumn("Primary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputPrimaryScore);
							ImGui::TableSetupColumn("Secondary Score", ImGuiTableColumnFlags_WidthFixed, 70.f, UI::UIInput::ColumnID::InputSecondaryScore);
							ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 120.0f, UI::UIInput::ColumnID::InputResult);
							ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 80.0f, UI::UIInput::ColumnID::InputFlags);
							ImGui::TableSetupColumn("ExecTime in ns", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputExecTime);
							ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 0.0f, UI::UIInput::ColumnID::InputAction);
							ImGui::TableSetupScrollFreeze(0, 1);
							ImGui::TableHeadersRow();

							// DO SOME SORTING

							// use clipper for large vertical lists
							ImGuiListClipper clipper;
							clipper.Begin((int32_t)(numinputs < inputs.size() ? numinputs : inputs.size()));
							while (clipper.Step()) {
								for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
									auto item = &inputs[i];
									ImGui::PushID((uint32_t)item->id);
									ImGui::TableNextRow();
									ImGui::TableNextColumn();
									ImGui::Text("%8llx", item->id);
									ImGui::TableNextColumn();
									ImGui::Text("%10lu", item->length);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->primaryScore);
									ImGui::TableNextColumn();
									ImGui::Text("%10.2f", item->secondaryScore);
									ImGui::TableNextColumn();
									if (item->result == UI::Result::Failing)
										ImGui::Text("Failing");
									else if (item->result == UI::Result::Passing)
										ImGui::Text("Passing");
									else if (item->result == UI::Result::Running)
										ImGui::Text("Running");
									else
										ImGui::Text("Unfinished");
									ImGui::TableNextColumn();
									ImGui::TextUnformatted(Utility::GetHexFill(item->flags).c_str());
									ImGui::TableNextColumn();
									ImGui::Text("%15lld", item->exectime.count());
									ImGui::TableNextColumn();
									if (ImGui::SmallButton("Replay")) {
										std::thread th(std::bind(&Session::Replay, session, std::placeholders::_1, std::placeholders::_2), item->id, &inputinfo);
										th.detach();
									}
									ImGui::SameLine();
									if (ImGui::SmallButton("Info")) {
										showInput = true;
										std::thread th([id = item->id]() { session->UI_GetInputInformation(inputinfo, id); });
										th.detach();
									}
									ImGui::PopID();
								}
							}
							ImGui::EndTable();
						}
					} else {
						ImGui::Text("No delta debugging active.");
						if (ImGui::Button("Delta Debug best unfinished input")) {
							// ignore return value, since we will find the delta debugging instance by scouring the session
							std::thread th(std::bind(&Session::UI_StartDeltaDebugging, session, std::placeholders::_1), 0);
							th.detach();
						}
					}
				}
				ImGui::End();
			}

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

		session->Wait();
		session->DestroySession();
		session.reset();

	} else if (CmdArgs::_responsive) {
Responsive:
#else
	if (CmdArgs::_responsive) {
#endif
		Logging::StdOutDebug = false;
		Logging::StdOutLogging = false;
		// wait for session to load
		StartSession();
		while (session->Loaded() == false)
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		session->SetSessionEndCallback(endCallback);
		// stops loop
		while (!stop) {
			if (CmdArgs::_saveStatus)
				SaveStatus();
			// read commands from cmd and act on them
			std::string line;
			getline(std::cin, line);
			line = Utility::ToLower(line);
			// we aren't using LLM so we only accept simple prefix-based commands

			// CMD: kill
			if (line.substr(0, 4).find("kill") != std::string::npos) {
				session->StopSession(false, true);
				break;
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
				std::cout << Snapshot(false);
			}
			// CMD: INTERNAL: exitinternal
			else if (line.substr(0, 12).find("exitinternal") != std::string::npos) {
				// internal command to exit main loop and program
				stop = true;
			}
		}
		session->Wait();
		session->DestroySession();
		session.reset();
	} else if (CmdArgs::_consoleUI == true) {
		setupConsole();
		clearScreen();
		moveTo(1, 1);
		Logging::StdOutDebug = false;
		Logging::StdOutLogging = false;
		StartSession();
		std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
		//while (session->Loaded() == false) {
		//	std::this_thread::sleep_for(std::chrono::milliseconds(50));
		//}
		while (session->Finished() == false) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			auto out = Snapshot(false);
			clearScreen();
			moveTo(1, 1);
			std::cout << out;

			if (CmdArgs::_saveStatus)
				SaveStatus();
		}

		restoreConsole();
		std::cout << Snapshot(true);
		session->Wait();
		session->DestroySession();
		session.reset();
	} else {
		Logging::StdOutDebug = false;
		Logging::StdOutLogging = false;
		// wait for session to load
		StartSession();
		std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
		while (session->Loaded() == false) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << "Beginning infinite wait for session end\n";
		while (session->Finished() == false) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			if (std::chrono::steady_clock::now() - last >= std::chrono::seconds(10)) {
				last = std::chrono::steady_clock::now();
				std::cout << Snapshot(false);
			}

			if (CmdArgs::_saveStatus)
				SaveStatus();
		}
		std::cout << Snapshot(true);
		session->Wait();
		session->DestroySession();
		session.reset();
	}
	Profile::Dispose();
	std::cout << "Exiting program";
	return 0;
}
