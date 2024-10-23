#include <atomic>
#include <queue>
#include <exception>

#include "TaskController.h"
#include "Threading.h"
#include "BufferOperations.h"
#include "Logging.h"
#include "LuaEngine.h"

TaskController* TaskController::GetSingleton()
{
	static TaskController session;
	return std::addressof(session);
}

void TaskController::AddTask(Functions::BaseFunction* a_task)
{
	{
		std::unique_lock<std::mutex> guard(lock);
		tasks.push_back(a_task);
	}
	condition.notify_one();
}

void TaskController::Start(std::shared_ptr<Session> session, int32_t numthreads)
{
	_session = session;
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
		tasks.pop_front();
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
			tasks.pop_front();
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
	// register new lua state for lua functions executed by the thread
	Lua::RegisterThread(_session);

	while (true) {
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
			tasks.pop_front();
		}
		if (del != nullptr) {
			del->Run();
			del->Dispose();
		}
	}

	// unregister thread from lua functions
	Lua::UnregisterThread();
}

size_t TaskController::GetStaticSize(int32_t version)
{
	static size_t size0x1 = Form::GetDynamicSize()  // form base size
	                        + 4                     // version
	                        + 1                     // terminate
	                        + 1                     // wait
	                        + 1                     // active
	                        + 4                     // numthreads
	                        + 8;                    // size of waiting tasks
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
	size_t sz = 0;
	for (auto task : tasks)
	{
		if (task != nullptr)
		{
			sz += task->GetLength();
		}
	}
	return GetStaticSize(classversion) + sz;
}

bool TaskController::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(terminate, buffer, offset);
	Buffer::Write(wait, buffer, offset);
	Buffer::Write(threads.size() > 0, buffer, offset);
	Buffer::Write(_numthreads, buffer, offset);
	Buffer::WriteSize(tasks.size(), buffer, offset);
	for (auto task : tasks)
	{
		task->WriteData(buffer, offset);
	}
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
			bool active = Buffer::ReadBool(buffer, offset);
			_numthreads = Buffer::ReadInt32(buffer, offset);
			size_t num = Buffer::ReadSize(buffer, offset);
			for (int32_t i = 0; i < (int32_t)num; i++)
			{
				Functions::BaseFunction* func = Functions::BaseFunction::Create(buffer, offset, length, resolver);
				tasks.push_back(func);
			}
			if (active)
				//Start(_numthreads);
				active = false;
			return true;
		}
		break;
	default:
		return false;
	}
}

void TaskController::Delete(Data*)
{
	Clear();
}

void TaskController::Clear()
{
	terminate = true;
	wait = false;
	condition.notify_all();
	for (std::thread& t : threads)
		t.detach();
	threads.clear();
	while (tasks.empty() == false) {
		tasks.front()->Dispose();
		tasks.pop_front();
	}
}

void TaskController::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}


void TaskController::Freeze()
{
	loginfo("Freezing execution...");
	// set freezing to true
	freeze = true;
	// wait for all threads to freeze
	bool frozen = false;
	while (frozen == false && threads.size() > 0)
	{
		frozen = true;
		for (int32_t i = 0; i < (int32_t)status.size(); i++)
			frozen &= status[i] == ThreadStatus::Waiting;
	}
	lock.lock();
	loginfo("Frozen execution.");
}

void TaskController::Thaw()
{
	loginfo("Thawing execution...");
	freeze = false;
	lock.unlock();
	condition.notify_all();
	loginfo("Resumed execution.");
}

