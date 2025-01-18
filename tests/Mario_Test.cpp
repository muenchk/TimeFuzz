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
#include "Data.h"

namespace Functions
{
	class Callback : public BaseFunction
	{
	public:

		std::shared_ptr<Input> input;

		void Run() override
		{
			if (input->Finished())
				if (auto ptr = input->test; ptr)
					logdebug("TEST: {}, EXITREASON: {}, EXITCODE: {}, EXECTIME: {}ms, OUTPUT: {}", ptr->_identifier, ptr->_exitreason, input->GetExitCode(), std::chrono::duration_cast<std::chrono::milliseconds>(input->GetExecutionTime()).count(), ptr->_output);
		}

		static uint64_t GetTypeStatic() { return 'CALL'; }
		uint64_t GetType() override { return 'CALL'; }
		FunctionType GetFunctionType() override { return FunctionType::Heavy; };

		virtual std::shared_ptr<BaseFunction> DeepCopy() override
		{
			auto ptr = std::make_shared<Callback>();
			ptr->input = input;
			return dynamic_pointer_cast<BaseFunction>(ptr);
		}

		bool ReadData(std::istream*, size_t&, size_t, LoadResolver*) override
		{
			return true;
		}
		bool WriteData(std::ostream* buffer, size_t& offset) override
		{
			BaseFunction::WriteData(buffer, offset);
			return true;
		}

		static std::shared_ptr<BaseFunction> Create()
		{
			return dynamic_pointer_cast<BaseFunction>(std::make_shared<Callback>());
		}

		void Dispose() override
		{
			if (input)
				if (input->test)
					input->test->_callback.reset();
			input.reset();
		}

		size_t GetLength() override
		{
			return BaseFunction::GetLength();
		}
	};
}

std::string lua =
	"function GetCmdArgs()\n"
	"return \"../../FormatExamples/TestMario_NoGraphics.py \\\"[[], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'Z', 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [ 'MOTION_RIGHT', 'RIGHT' ], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], [], []]\\\"\"\n"
	"end";

int main(/*int argc, char** argv*/)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif
	Functions::RegisterFactory(Functions::Callback::GetTypeStatic(), Functions::Callback::Create);
	logdebug("Init");
	std::shared_ptr<Settings> sett = std::make_shared<Settings>();
	std::shared_ptr<Session> sess = Session::CreateSession();
	std::shared_ptr<SessionData> sessdata = sess->data->CreateForm<SessionData>();
	logdebug("Initialized Settings");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::shared_ptr<Oracle> oracle = std::make_shared<Oracle>();
	oracle->Set(Oracle::PUTType::Script, "C:/Users/Kai/AppData/Local/Microsoft/WindowsApps/python3.8.exe");
#else
	std::shared_ptr<Oracle> oracle = std::make_shared<Oracle>();
	oracle->Set(Oracle::PUTType::Script, "/usr/local/bin/python3.8");
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
	controller->Start(sessdata, 1);
	logdebug("Started TaskController with 1 thread");
	std::shared_ptr<ExecutionHandler> execution = std::make_shared<ExecutionHandler>();
	execution->Init(sess, sessdata, sett, controller, 1, oracle);
	execution->SetMaxConcurrentTests(10);
	logdebug("Created executionhandler");
	execution->StartHandler();
	logdebug("Started executionhandler");
	logdebug("Adding test");
	std::vector<std::shared_ptr<Input>> ls;
	for (int i = 0; i < 5; i++) {
		std::shared_ptr<Input> input = std::make_shared<Input>();
		input->AddEntry("[]");
		input->SetGenerated();

		auto task = Functions::BaseFunction::Create<Functions::Callback>();
		task->input = input;
		execution->AddTest(input, dynamic_pointer_cast<Functions::BaseFunction>(task));
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
