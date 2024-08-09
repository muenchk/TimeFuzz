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

namespace Functions
{
	class Callback : BaseFunction
	{
	public:

		std::shared_ptr<Input> input;

		void Run()
		{
			if (input->Finished())
				if (auto ptr = input->test.lock(); ptr)
					logdebug("TEST: {}, EXITREASON: {}, EXITCODE: {}, EXECTIME: {}ms, OUTPUT: {}", ptr->identifier, ptr->exitreason, input->GetExitCode(), std::chrono::duration_cast<std::chrono::milliseconds>(input->GetExecutionTime()).count(), ptr->output);
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
			BaseFunction::Dispose();
			delete this;
		}

		size_t GetLength()
		{
			return BaseFunction::GetLength();
		}
	};
}

std::string getCMDArgs(std::shared_ptr<Input>)
{
	std::string arg = "[[], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['Z', 'MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], ['MOTION_RIGHT', 'RIGHT'], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], []]";

	return "../../FormatExamples/TestMario_NoGraphics.py \"" + arg + "\"";
}

int main(/*int argc, char** argv*/)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif
	Functions::RegisterFactory(Functions::Callback::GetTypeStatic(), Functions::Callback::Create);
	logdebug("Init");
	std::shared_ptr<Settings> sett = std::make_shared<Settings>();
	std::shared_ptr<Session> sess = std::make_shared<Session>();
	logdebug("Initialized Settings");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::shared_ptr<Oracle> oracle = std::make_shared<Oracle>();
	oracle->Set(Oracle::PUTType::Script, "C:/Users/Kai/AppData/Local/Microsoft/WindowsApps/python3.8.exe");
#else
	std::shared_ptr<Oracle> oracle = std::make_shared<Oracle>();
	oracle->Set(Oracle::PUTType::Script, "/usr/local/bin/python3.8");
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
	execution->Init(sess, sett, controller, 1, oracle, getCMDArgs);
	execution->SetMaxConcurrentTests(10);
	logdebug("Created executionhandler");
	execution->StartHandler();
	logdebug("Started executionhandler");
	logdebug("Adding test");
	std::vector<std::shared_ptr<Input>> ls;
	for (int i = 0; i < 5; i++) {
		std::shared_ptr<Input> input = std::make_shared<Input>();

		auto task = Functions::BaseFunction::Create<Functions::Callback>();
		task->input = input;
		execution->AddTest(input, (Functions::BaseFunction*)(task));
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
