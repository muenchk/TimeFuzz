#pragma once

#include "Input.h"
#include "TaskController.h"

namespace DeltaDebugging
{
	class DeltaController
	{
	public:
		/// <summary>
		/// Sets the number of tasks that have been started for this instance of delta debugging
		/// </summary>
		/// <param name="numtasks"></param>
		void SetTasks(int32_t numtasks);
		/// <summary>
		/// Sets one task started for this instance of delta debugging as completed
		/// </summary>
		void CompleteTask();
		/// <summary>
		/// starts delta debugging for an input on a specific taskcontroller
		/// </summary>
		/// <param name="input">the input to delta debug</param>
		/// <param name="controller">the (optional) controller to execute on</param>
		void DeltaDebugging(std::shared_ptr<Input> input, std::shared_ptr<TaskController> controller = std::shared_ptr<TaskController>());

	private:
		/// <summary>
		/// number of tasks that were started
		/// </summary>
		int32_t tasks = 0;
		/// <summary>
		/// number of tasks that need to be completed, before the callback can be scheduled
		/// </summary>
		int32_t remainingtasks = 0;
		/// <summary>
		/// Callback used to evaluate result of all executed tasks
		/// </summary>
		TaskController::Task* callback;
	};
}
