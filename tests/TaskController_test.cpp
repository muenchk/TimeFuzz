#include "Logging.h"
#include "TaskController.h"
#include "CrashHandler.h"

int main(int argc, char** argv)
{
	Logging::InitializeLog(".");
	Crash::Install(".");
	TaskController controller;
	controller.Start(10);
	int arr[100];
	for (int i = 0; i < 100; i++) {
		controller.AddTask([&arr, i]() {
			arr[i] = i;
		});
	}
	controller.Stop();
	for (int i = 0; i < 100; i++) {
		if (arr[i] != i)
			return 1;
	}
	return 0;
}
