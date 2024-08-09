#include "Logging.h"
#include "TaskController.h"
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include "CrashHandler.h"
#endif

#include "Function.h"

namespace Functions
{
	class TaskControllerTestCallback : BaseFunction
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
			return new TaskControllerTestCallback();
		}

		void Dispose()
		{
			BaseFunction::Dispose();
			arr = nullptr;
			delete this;
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
	Functions::RegisterFactory(Functions::TaskControllerTestCallback::GetTypeStatic(), Functions::TaskControllerTestCallback::Create);
	TaskController controller;
	controller.Start(10);
	int arr[100];
	for (int i = 0; i < 100; i++) {
		auto task = Functions::BaseFunction::Create<Functions::TaskControllerTestCallback>();
		task->arr = arr;
		task->i = i;
		controller.AddTask((Functions::BaseFunction*)(task));
	}
	controller.Stop();
	for (int i = 0; i < 100; i++) {
		if (arr[i] != i)
			return 1;
	}
	return 0;
}
