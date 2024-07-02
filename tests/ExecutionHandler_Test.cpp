#include "Logging.h"
#include "ExecutionHandler.h"
#include "CrashHandler.h"
#include "Settings.h"
#include "TaskController.h"
#include "Input.h"

#define NUM_TESTS 100

void Callback()
{

}

std::string ReturnArgs(std::shared_ptr<Input> input)
{
	return "";
}

int main(int argc, char** argv)
{
	Logging::InitializeLog(".");
	Crash::Install(".");
	logdebug("Init");
	std::shared_ptr<TaskController> controller = std::make_shared<TaskController>();
	logdebug("Created TaskController");
	controller->Start(1);
	logdebug("Started TaskController with 1 thread");
	Settings* sett = Settings::GetSingleton();
	logdebug("Initialized Settings");
	std::shared_ptr<Oracle> oracle = std::make_shared<Oracle>(Oracle::OracleType::CommandlineProgramDump, "Test_PUT_General.exe");
	logdebug("Created Oracle");
	std::shared_ptr<ExecutionHandler> execution = std::make_shared<ExecutionHandler>(sett, controller, 1, oracle, ReturnArgs);
	execution->SetMaxConcurrentTests(50);
	logdebug("Created executionhandler");
	execution->StartHandler();
	logdebug("Started executionhandler");
	logdebug("Adding tests");
	std::vector<std::shared_ptr<Input>> ls;
	for (int i = 0; i < NUM_TESTS; i++) {
		std::shared_ptr<Input> input = std::make_shared<Input>();
		execution->AddTest(input, Callback);
		ls.push_back(input);
	}
	std::this_thread::sleep_for(std::chrono::seconds(1));
	execution->StopHandlerAfterTestsFinishAndWait();
	logdebug("waited on executionhandler to finish all tests");

	for (int i = 0; i < NUM_TESTS; i++)
	{
		if (ls[i]->Finished())
			logdebug("TEST: {}, EXITREASON: {}, EXITCODE: {}, EXECTIME: {}ms, OUTPUT: {}", ls[i]->test->identifier, ls[i]->test->exitreason, ls[i]->GetExitCode(), std::chrono::duration_cast<std::chrono::milliseconds>(ls[i]->GetExecutionTime()).count(), ls[i]->test->output);
	}

	execution.reset();
	oracle.reset();
	oracle = std::make_shared<Oracle>(Oracle::OracleType::CommandlineProgramDump, "Test_PUT_IO.exe");
	execution = std::make_shared<ExecutionHandler>(sett, controller, 1, oracle, ReturnArgs);
	logdebug("Set up IO test");
	sett->tests.use_testtimeout = true;
	sett->tests.testtimeout = 10000000;
	execution->SetEnableFragments();
	execution->StartHandler();
	std::shared_ptr<Input> inp = std::make_shared<Input>();
	inp->AddEntry("What a wonderful day.");
	inp->AddEntry(".");
	execution->AddTest(inp, Callback);
	execution->StopHandlerAfterTestsFinishAndWait();
	logdebug("Finished execution handler");
	logdebug("TEST: {}, EXITREASON: {}, EXITCODE: {}, EXECTIME: {}ms, OUTPUT: {}", inp->test->identifier, inp->test->exitreason, inp->GetExitCode(), std::chrono::duration_cast<std::chrono::milliseconds>(inp->GetExecutionTime()).count(), inp->test->output);

	execution.reset();
	//controller->Stop();
	controller.reset();
	return 0;
}
