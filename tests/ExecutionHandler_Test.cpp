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
#include "Session.h"

#define NUM_TESTS 100

#include "Function.h"

namespace Functions
{
	class Callback : BaseFunction
	{
	public:

		void Run()
		{

		}

		static uint64_t GetTypeStatic() { return 'CALL'; }
		uint64_t GetType() override { return 'CALL'; }
		bool ReadData(unsigned char*, size_t&, size_t, LoadResolver*)
		{
			return true;
		}
		bool WriteData(unsigned char* buffer, size_t& offset)
		{
			BaseFunction::WriteData(buffer, offset);
			return true;
		}

		static BaseFunction* Create()
		{
			return new Callback();
		}

		void Dispose()
		{
			delete this;
		}

		size_t GetLength()
		{
			return BaseFunction::GetLength();
		}
	};
}
std::string lua =
	"function GetCmdArgs()\n"
	"return \"\"\n"
	"end";

int32_t main(/*int32_t argc, char** argv*/)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif
	Functions::RegisterFactory(Functions::Callback::GetTypeStatic(), Functions::Callback::Create);
	logdebug("Init");
	logdebug("Started TaskController with 1 thread");
	std::shared_ptr<Settings> sett = std::make_shared<Settings>();
	std::shared_ptr<Session> sess = Session::CreateSession();
	logdebug("Initialized Settings");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::shared_ptr<Oracle> oracle = std::make_shared<Oracle>();
	oracle->Set(Oracle::PUTType::STDIN_Dump, std::filesystem::absolute(std::filesystem::path("Test_PUT_General.exe")));
#else
	std::shared_ptr<Oracle> oracle = std::make_shared<Oracle>();
	oracle->Set(Oracle::PUTType::STDIN_Dump, std::filesystem::absolute(std::filesystem::path("Test_PUT_General")));
#endif
	oracle->SetLuaCmdArgs(lua);
	// check the oracle for validity
	if (oracle->Validate() == false) {
		logcritical("Oracle isn't valid.");
		exit(1);
	}
	logdebug("Created Oracle");
	std::shared_ptr<TaskController> controller = std::make_shared<TaskController>();
	controller->SetDisableLua();
	logdebug("Created TaskController");
	controller->Start(sess, 1);
	std::shared_ptr<ExecutionHandler> execution = std::make_shared<ExecutionHandler>();
	execution->Init(sess, sett, controller, 1, oracle);
	execution->SetMaxConcurrentTests(50);
	logdebug("Created executionhandler");
	execution->StartHandler();
	logdebug("Started executionhandler");
	logdebug("Adding tests");
	std::vector<std::shared_ptr<Input>> ls;
	for (int32_t i = 0; i < NUM_TESTS; i++) {
		std::shared_ptr<Input> input = std::make_shared<Input>();
		execution->AddTest(input, (Functions::BaseFunction*)(new Functions::Callback()));
		ls.push_back(input);
	}
	std::this_thread::sleep_for(std::chrono::seconds(1));
	execution->StopHandlerAfterTestsFinishAndWait();
	logdebug("waited on executionhandler to finish all tests");

	for (int32_t i = 0; i < NUM_TESTS; i++)
	{
		if (ls[i]->Finished())
			if (auto ptr = ls[i]->test; ptr)
				logdebug("TEST: {}, EXITREASON: {}, EXITCODE: {}, EXECTIME: {}ms, OUTPUT: {}", ptr->identifier, ptr->exitreason, ls[i]->GetExitCode(), std::chrono::duration_cast<std::chrono::milliseconds>(ls[i]->GetExecutionTime()).count(), ptr->output);
	}

	execution.reset();
	oracle.reset();


#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	oracle = std::make_shared<Oracle>();
	oracle->Set(Oracle::PUTType::STDIN_Dump, std::filesystem::absolute(std::filesystem::path("Test_PUT_IO.exe")));
#else
	oracle = std::make_shared<Oracle>();
	oracle->Set(Oracle::PUTType::STDIN_Dump, std::filesystem::absolute(std::filesystem::path("Test_PUT_IO")));
#endif
	// check the oracle for validity
	if (oracle->Validate() == false) {
		logcritical("Oracle isn't valid.");
		controller->Stop();
		exit(1);
	}
	execution = std::make_shared<ExecutionHandler>();
	execution->Init(sess, sett, controller, 1, oracle);
	logdebug("Set up IO test");
	sett->tests.use_testtimeout = true;
	sett->tests.testtimeout = 10000000;
	execution->SetEnableFragments();
	execution->StartHandler();
	std::shared_ptr<Input> inp = std::make_shared<Input>();
	inp->AddEntry("What a wonderful day.");
	inp->AddEntry(".");
	execution->AddTest(inp, (Functions::BaseFunction*)(new Functions::Callback()));
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	execution->StopHandlerAfterTestsFinishAndWait();
	logdebug("Finished execution handler");
	if (auto ptr = inp->test; ptr)
		logdebug("TEST: {}, EXITREASON: {}, EXITCODE: {}, EXECTIME: {}ms, OUTPUT: {}", ptr->identifier, ptr->exitreason, inp->GetExitCode(), std::chrono::duration_cast<std::chrono::milliseconds>(inp->GetExecutionTime()).count(), ptr->output);

	execution.reset();
	controller->Stop();
	controller.reset();
	exit(0);
}
