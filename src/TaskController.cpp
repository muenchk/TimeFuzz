#include <atomic>
#include <queue>
#include <exception>

#include "TaskController.h"
#include "Threading.h"
#include "BufferOperations.h"

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
	for (int32_t i = 0; i < numthreads; i++)
		threads.emplace_back(std::thread(&TaskController::InternalLoop, this));
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
	return !tasks.empty();
}

void TaskController::InternalLoop()
{
	while (true)
	{
		Functions::BaseFunction* del;
		{
			std::unique_lock<std::mutex> guard(lock);
			condition.wait(guard, [this] { return !tasks.empty() || terminate && wait == false || terminate && tasks.empty(); });
			if (terminate && wait == false || terminate && tasks.empty())
				return;
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
