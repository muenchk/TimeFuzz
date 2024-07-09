#include "Logging.h"
#include "ExecutionHandler.h"
#if defined(unix) || defined(__unix__) || defined(__unix)
# include <cstdlib>
#		include <unistd.h>
#		include <sys/wait.h>
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include "CrashHandler.h"
#endif
#include "Settings.h"
#include "TaskController.h"
#include "Input.h"

void Callback(std::shared_ptr<Input> input)
{
	if (input->Finished())
		logdebug("TEST: {}, EXITREASON: {}, EXITCODE: {}, EXECTIME: {}ms, OUTPUT: {}", input->test->identifier, input->test->exitreason, input->GetExitCode(), std::chrono::duration_cast<std::chrono::milliseconds>(input->GetExecutionTime()).count(), input->test->output);
}

std::string getCMDArgs(std::shared_ptr<Input>)
{
	std::string arg = "[[], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], []]";

	return "-m TestMario_NoGraphics \"" + arg + "\"";
}

int main(/*int argc, char** argv*/)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif
	logdebug("Init");
	Settings* sett = Settings::GetSingleton();
	logdebug("Initialized Settings");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::shared_ptr<Oracle> oracle = std::make_shared<Oracle>(Oracle::PUTType::Script, "C:/Users/Kai/AppData/Local/Microsoft/WindowsApps/python3.8.exe");
#else
	std::shared_ptr<Oracle> oracle = std::make_shared<Oracle>(Oracle::PUTType::Script, "/usr/local/bin/python3.8");
#endif
	// check the oracle for validity
	if (oracle->Validate() == false) {
		logcritical("Oracle isn't valid.");
		exit(1);
	}
	logdebug("Created Oracle");
	std::shared_ptr<TaskController> controller = std::make_shared<TaskController>();
	logdebug("Created TaskController");
	controller->Start(1);
	logdebug("Started TaskController with 1 thread");
	std::shared_ptr<ExecutionHandler> execution = std::make_shared<ExecutionHandler>();
	execution->Init(sett, controller, 1, oracle, getCMDArgs);
	execution->SetMaxConcurrentTests(10);
	logdebug("Created executionhandler");
	execution->StartHandler();
	logdebug("Started executionhandler");
	logdebug("Adding test");
	std::vector<std::shared_ptr<Input>> ls;
	for (int i = 0; i < 5; i++) {
		std::shared_ptr<Input> input = std::make_shared<Input>();
		execution->AddTest(input, [input]() {
			Callback(input);
		});
		ls.push_back(input);
	}
	//std::shared_ptr<Input> input = std::make_shared<Input>();
	//execution->AddTest(input, [input]() {
	//	Callback(input);
	//});
	execution->StopHandlerAfterTestsFinishAndWait();
	logdebug("waited on executionhandler to finish all tests");


	execution.reset();
	controller->Stop();
	controller.reset();
	oracle.reset();
}
