#include "Logging.h"
#include "TaskController.h"
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include "CrashHandler.h"
#endif

#include "Function.h"
#include "Session.h"
#include "Data.h"

#include <memory>

namespace Functions
{
	class TaskControllerTestCallback : public BaseFunction
	{
	public:
		int* arr = nullptr;
		int i = 0;

		void Run()
		{
			arr[i] = i;
		}

		static uint64_t GetTypeStatic() { return 'TATE'; }
		uint64_t GetType() override { return 'TATE'; }
		FunctionType GetFunctionType() override { return FunctionType::Heavy; };
		bool ReadData(std::istream*, size_t&, size_t, LoadResolver*)
		{
			return true;
		}
		bool WriteData(std::ostream* buffer, size_t& offset)
		{
			BaseFunction::WriteData(buffer, offset);
			return true;
		}

		static std::shared_ptr<BaseFunction> Create()
		{
			return dynamic_pointer_cast<BaseFunction>(std::make_shared<TaskControllerTestCallback>());
		}

		void Dispose()
		{
			arr = nullptr;
		}

		size_t GetLength()
		{
			return BaseFunction::GetLength();
		}
	};
}

int main(/*int argc, char** argv*/)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif
	std::shared_ptr<Session> session = Session::CreateSession();
	std::shared_ptr<SessionData> sessiondata = session->data->CreateForm<SessionData>();
	Functions::RegisterFactory(Functions::TaskControllerTestCallback::GetTypeStatic(), Functions::TaskControllerTestCallback::Create);
	TaskController controller;
	controller.SetDisableLua();
	controller.Start(sessiondata, 1);
	int arr[100];
	for (int i = 0; i < 100; i++) {
		auto task = Functions::BaseFunction::Create<Functions::TaskControllerTestCallback>();
		task->arr = arr;
		task->i = i;
		controller.AddTask(dynamic_pointer_cast<Functions::BaseFunction>(task));
	}
	controller.Stop();
	for (int i = 0; i < 100; i++) {
		if (arr[i] != i)
			return 1;
	}
	return 0;
}
