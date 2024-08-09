#include <atomic>
#include <queue>
#include <exception>

#include "TaskController.h"
#include "Threading.h"
#include "BufferOperations.h"
#include "Logging.h"

TaskController* TaskController::GetSingleton()
{
	static TaskController session;
	return std::addressof(session);
}

void TaskController::AddTask(Functions::BaseFunction* a_task)
{
	{
		std::unique_lock<std::mutex> guard(lock);
		tasks.push(a_task);
	}
	condition.notify_one();
}

void TaskController::Start(int32_t numthreads)
{
	if (numthreads == 0)
		throw std::runtime_error("Cannot start a TaskController with 0 threads.");
	_numthreads = numthreads;
	for (int32_t i = 0; i < numthreads; i++) {
		status.push_back(ThreadStatus::Initializing);
		threads.emplace_back(std::thread(&TaskController::InternalLoop, this, i));
	}
}

void TaskController::Stop(bool completeall)
{
	{
		std::unique_lock<std::mutex> guard(lock);
		terminate = true;
		wait = completeall;
	}
	condition.notify_all();
	for (std::thread& t : threads)
		t.join();
	threads.clear();
	while (tasks.empty() == false)
	{
		tasks.front()->Dispose();
		tasks.pop();
	}
}

TaskController::~TaskController()
{
	if (terminate == false)
	{
		// we are exiting unexpectedly
		// set terminate and detach the threads so we do not have to wait on them
		terminate = true;
		for (auto& thread : threads)
			thread.detach();
		while (tasks.empty() == false) {
			tasks.front()->Dispose();
			tasks.pop();
		}
	}
}

bool TaskController::Busy()
{
	bool running = true;
	for (int32_t i = 0; i < (int32_t)status.size(); i++)
		running &= status[i] == ThreadStatus::Running;
	return !tasks.empty() || running;
}

void TaskController::InternalLoop(int32_t number)
{
	while (true)
	{
		Functions::BaseFunction* del;
		{
			std::unique_lock<std::mutex> guard(lock);
			status[number] = ThreadStatus::Waiting;
			// while freeze is [true], this will never return, if freeze is [false] it only returns when [tasks is non-empty], when [terinated and not waiting], or when [terminating and tasks is empty]
			condition.wait(guard, [this] { return freeze == false && (!tasks.empty() || terminate && wait == false || terminate && tasks.empty()); });
			if (terminate && wait == false || terminate && tasks.empty())
				return;
			status[number] = ThreadStatus::Running;
			del = tasks.front();
			tasks.pop();
		}
		del->Run();
		del->Dispose();
	}
}

size_t TaskController::GetStaticSize(int32_t version)
{
	static size_t size0x1 = Form::GetDynamicSize()  // form base size
	                        + 4                     // version
	                        + 1                     // terminate
	                        + 1                     // wait
	                        + 4;                    // numthreads
	switch (version)
	{
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t TaskController::GetDynamicSize()
{
	return GetStaticSize(classversion);
}

bool TaskController::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(terminate, buffer, offset);
	Buffer::Write(wait, buffer, offset);
	Buffer::Write(_numthreads, buffer, offset);
	return true;
}

bool TaskController::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			terminate = Buffer::ReadBool(buffer, offset);
			wait = Buffer::ReadBool(buffer, offset);
			_numthreads = Buffer::ReadInt32(buffer, offset);
			Start(_numthreads);
			return true;
		}
		break;
	default:
		return false;
	}
}

void TaskController::Delete(Data*)
{

}

void TaskController::Freeze()
{
	loginfo("Freezing execution...");
	// set freezing to true
	freeze = true;
	// wait for all threads to freeze
	bool frozen = false;
	while (frozen == false)
	{
		frozen = true;
		for (int32_t i = 0; i < (int32_t)status.size(); i++)
			frozen &= status[i] == ThreadStatus::Waiting;
	}
	loginfo("Frozen execution.");
}

void TaskController::Thaw()
{
	loginfo("Thawing execution...");
	freeze = false;
	condition.notify_all();
	loginfo("Resumed execution.");
}
