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

#include "Function.h"

namespace Functions
{
	class Callback : BaseFunction
	{
	public:
		std::shared_ptr<Input> input;

		void Run() override
		{
			if (input->Finished())
				if (auto ptr = input->test; ptr)
					logdebug("TEST: {}, EXITREASON: {}, EXITCODE: {}, EXECTIME: {}ms, OUTPUT: {}", ptr->identifier, ptr->exitreason, input->GetExitCode(), std::chrono::duration_cast<std::chrono::milliseconds>(input->GetExecutionTime()).count(), ptr->output);
		}

		static uint64_t GetTypeStatic() { return 'CALL'; }
		uint64_t GetType() override { return 'CALL'; }
		bool ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver) override
		{
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, id, resolver]() {
				this->input = resolver->ResolveFormID<Input>(id);
			});
			return true;
		}
		bool WriteData(unsigned char* buffer, size_t& offset) override
		{
			BaseFunction::WriteData(buffer, offset);
			Buffer::Write(input->GetFormID(), buffer, offset);
			return true;
		}

		static BaseFunction* Create()
		{
			return new Callback();
		}

		void Dispose() override
		{
			delete this;
		}

		size_t GetLength() override
		{
			return BaseFunction::GetLength() + 8;
		}
	};

	class TaskControllerTestCallback : BaseFunction
	{
	public:
		int32_t i = 0;

		void Run() override
		{
			loginfo("TaskControllerTestCallback: {}", i);
		}

		static uint64_t GetTypeStatic() { return 'TATE'; }
		uint64_t GetType() override { return 'TATE'; }
		bool ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver*) override
		{
			i = Buffer::ReadInt32(buffer, offset);
			return true;
		}
		bool WriteData(unsigned char* buffer, size_t& offset) override
		{
			BaseFunction::WriteData(buffer, offset);
			Buffer::Write(i, buffer, offset);
			return true;
		}

		static BaseFunction* Create()
		{
			return new TaskControllerTestCallback();
		}

		void Dispose() override
		{
			delete this;
		}

		size_t GetLength() override
		{
			return BaseFunction::GetLength() + 4;
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

	std::shared_ptr<Session> sess = Session::CreateSession();
	std::shared_ptr<Settings> sett = sess->data->CreateForm<Settings>();

	//////// build a taskcontroller and add some functions
	Functions::RegisterFactory(Functions::Callback::GetTypeStatic(), Functions::Callback::Create);
	Functions::RegisterFactory(Functions::TaskControllerTestCallback::GetTypeStatic(), Functions::TaskControllerTestCallback::Create);
	std::shared_ptr<TaskController> controller = sess->data->CreateForm<TaskController>();
	controller->SetDisableLua();
	//controller.Start(10);
	int arr[100];
	for (int i = 0; i < 100; i++) {
		auto task = Functions::BaseFunction::Create<Functions::TaskControllerTestCallback>();
		arr[i] = i;
		task->i = i;
		controller->AddTask((Functions::BaseFunction*)(task));
	}
	//controller.Stop();
	//for (int i = 0; i < 100; i++) {
	//	if (arr[i] != i)
	//		return 1;
	//}
	
	// build an execution controller and add some testss
	logdebug("Initialized Settings");
	std::shared_ptr<Oracle> oracle = sess->data->CreateForm<Oracle>();
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	oracle->Set(Oracle::PUTType::Script, "C:/Users/Kai/AppData/Local/Microsoft/WindowsApps/python3.8.exe");
#else
	oracle->Set(Oracle::PUTType::Script, "/usr/local/bin/python3.8");
#endif
	oracle->SetLuaCmdArgs(lua);
	// check the oracle for validity
	if (oracle->Validate() == false) {
		logcritical("Oracle isn't valid.");
		exit(1);
	}
	logdebug("Created Oracle");
	std::shared_ptr<ExecutionHandler> execution = sess->data->CreateForm<ExecutionHandler>();
	execution->Init(sess, sett, controller, 1, oracle);
	execution->SetMaxConcurrentTests(10);
	logdebug("Created executionhandler");
	//execution->StartHandler();
	logdebug("Started executionhandler");
	logdebug("Adding test");
	std::vector<std::shared_ptr<Input>> ls;
	for (int i = 0; i < 5; i++) {
		std::shared_ptr<Input> input = sess->data->CreateForm<Input>();

		auto task = Functions::BaseFunction::Create<Functions::Callback>();
		task->input = input;
		execution->AddTest(input, (Functions::BaseFunction*)(task));
		ls.push_back(input);
	}

	//execution->StopHandlerAfterTestsFinishAndWait();
	logdebug("waited on executionhandler to finish all tests");

	sess->data->SetSaveName("test");
	sess->data->SetSavePath("./saves");
	sess->data->Save();
	execution.reset();
	controller.reset();
	sett.reset();
	oracle.reset();
	sess.reset();
	sess = Session::LoadSession("test", L"");
	execution = sess->data->CreateForm<ExecutionHandler>();
	controller = sess->data->CreateForm<TaskController>();
	controller->SetDisableLua();
	sett = sess->data->CreateForm<Settings>();
	oracle = sess->data->CreateForm<Oracle>();
	oracle->SetLuaCmdArgs(lua);

	controller->Start(sess, 3);
	execution->StartHandlerAsIs();

	execution->StopHandlerAfterTestsFinishAndWait();
	controller->Stop();
	execution.reset();
	controller.reset();
	oracle.reset();

	return 0;
}
