#include "ExecutionHandler.h"
#include "Logging.h"
#if defined(unix) || defined(__unix__) || defined(__unix)
#	include <cstdlib>
#	include <unistd.h>
#	include <sys/wait.h>
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include "ChrashHandlerINCL.h"
#endif
#include "Data.h"
#include "Input.h"
#include "Session.h"
#include "Settings.h"
#include "TaskController.h"

#include "Function.h"

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

		bool ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver) override
		{
			uint64_t id = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, id, resolver]() {
				this->input = resolver->ResolveFormID<Input>(id);
			});
			return true;
		}
		bool WriteData(std::ostream* buffer, size_t& offset) override
		{
			BaseFunction::WriteData(buffer, offset);
			Buffer::Write(input->GetFormID(), buffer, offset);
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
			return BaseFunction::GetLength() + 8;
		}

		virtual const char* GetName() override
		{
			return "Callback";
		}
	};

	class TaskControllerTestCallback : public BaseFunction
	{
	public:
		int32_t i = 0;

		void Run() override
		{
			loginfo("TaskControllerTestCallback: {}", i);
		}

		static uint64_t GetTypeStatic() { return 'TATE'; }
		uint64_t GetType() override { return 'TATE'; }
		FunctionType GetFunctionType() override { return FunctionType::Heavy; };

		virtual std::shared_ptr<BaseFunction> DeepCopy() override
		{
			auto ptr = std::make_shared<TaskControllerTestCallback>();
			ptr->i = i;
			return dynamic_pointer_cast<BaseFunction>(ptr);
		}

		bool ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver*) override
		{
			i = Buffer::ReadInt32(buffer, offset);
			return true;
		}
		bool WriteData(std::ostream* buffer, size_t& offset) override
		{
			BaseFunction::WriteData(buffer, offset);
			Buffer::Write(i, buffer, offset);
			return true;
		}

		static std::shared_ptr<BaseFunction> Create()
		{
			return dynamic_pointer_cast<BaseFunction>(std::make_shared<TaskControllerTestCallback>());
		}

		void Dispose() override
		{
		}

		size_t GetLength() override
		{
			return BaseFunction::GetLength() + 4;
		}

		virtual const char* GetName() override
		{
			return "TaskControllerTestCallback";
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
	std::shared_ptr<SessionData> sessdata = sess->data->CreateForm<SessionData>();

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
		controller->AddTask(dynamic_pointer_cast<Functions::BaseFunction>(task));
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
	execution->Init(sess, sessdata, sett, controller, 1, oracle);
	execution->SetMaxConcurrentTests(10);
	logdebug("Created executionhandler");
	//execution->StartHandler();
	logdebug("Started executionhandler");
	logdebug("Adding test");
	//std::vector<std::shared_ptr<Input>> ls;
	for (int i = 0; i < 5; i++) {
		std::shared_ptr<Input> input = sess->data->CreateForm<Input>();
		input->AddEntry("[]");
		input->SetGenerated();

		auto task = Functions::BaseFunction::Create<Functions::Callback>();
		task->input = input;
		execution->AddTest(input, dynamic_pointer_cast<Functions::BaseFunction>(task));
		//ls.push_back(input);
	}

	//sess->data->SetSaveName("test");
	//sess->data->SetSavePath("./saves");
	//sess->data->Save();
	//execution.reset();
	//controller.reset();
	//sett.reset();
	//oracle.reset();
	//sess.reset();
	//sess = Session::LoadSession("test", L"");
	//execution = sess->data->CreateForm<ExecutionHandler>();
	//controller = sess->data->CreateForm<TaskController>();
	//sett = sess->data->CreateForm<Settings>();
	//oracle = sess->data->CreateForm<Oracle>();
	//oracle->SetLuaCmdArgs(lua);

	//controller->Start(3);
	//execution->StartHandlerAsIs();
	execution.reset();
	controller.reset();
	oracle.reset();
	sett.reset();
	sessdata.reset();

	auto hashmap = sess->data->GetWeakHashmapCopy();
	sess->StopSession(false);
	sess->DestroySession();
	sess.reset();

	// check wether all the weak_ptr are inaccessible (which means the original object is properly deleted)
	bool deleted = true;
	for (auto& [id, form] : hashmap)
	{
		if (auto ptr = form.lock(); ptr)
			deleted = false;
	}

	if (deleted)
		return 0;
	else
		return 1;
}
