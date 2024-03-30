#include "TaskController.h"

int main(int argc, char** argv)
{
	TaskController controller;
	controller.Start(false, 10);
	int arr[100];
	for (int i = 0; i < 101; i++) {
		controller.AddTask([&arr, i]() {
			arr[i] = i;
		});
	}
	controller.Stop();
	for (int i = 0; i < 101; i++) {
		if (arr[i] != i)
			return 1;
	}
	return 0;
}
