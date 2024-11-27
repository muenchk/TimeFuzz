#include <atomic>
#include <queue>
#include <exception>

#include "TaskController.h"
#include "Threading.h"
#include "BufferOperations.h"
#include "Logging.h"
#include "LuaEngine.h"
#include "SessionData.h"

TaskController* TaskController::GetSingleton()
{
	static TaskController session;
	return std::addressof(session);
}

void TaskController::AddTask(std::shared_ptr<Functions::BaseFunction> a_task)
{
	if (_optimizeFuncExec && a_task->GetFunctionType() == Functions::FunctionType::Light) {
		std::unique_lock<std::mutex> guard(_lock);
		_tasks_light.push_back(a_task);
		_condition_light.notify_one();
	} else if (_optimizeFuncExec && a_task->GetFunctionType() == Functions::FunctionType::Medium) {
		std::unique_lock<std::mutex> guard(_lock);
		_tasks_medium.push_back(a_task);
	} else {
		std::unique_lock<std::mutex> guard(_lock);
		_tasks.push_back(a_task);
	}
	_condition.notify_one();
}

void TaskController::Start(std::shared_ptr<SessionData> session, int32_t numthreads)
{
	_sessiondata = session;
	if (numthreads == 0)
		throw std::runtime_error("Cannot start a TaskController with 0 threads.");
	_numthreads = numthreads;
	if (_numthreads < 1)
		_numthreads = 1;
	if (_numthreads == 1 && _optimizeFuncExec) {
		_status.push_back(ThreadStatus::Initializing);
		_threads.emplace_back(std::thread(&TaskController::InternalLoop_SingleThread, this, 0));
	}
	else {
		for (int32_t i = 0; i < numthreads; i++) {
			_status.push_back(ThreadStatus::Initializing);
			if (_optimizeFuncExec && i == 0)
				_threads.emplace_back(std::thread(&TaskController::InternalLoop_LightExclusive, this, i));
			else
				_threads.emplace_back(std::thread(&TaskController::InternalLoop, this, i));
		}
	}
}

void TaskController::Stop(bool completeall)
{
	{
		std::unique_lock<std::mutex> guard(_lock);
		_terminate = true;
		_wait = completeall;
	}
	_condition.notify_all();
	_condition_light.notify_all();
	for (std::thread& t : _threads)
		t.join();
	_threads.clear();
	while (_tasks.empty() == false)
	{
		_tasks.front()->Dispose();
		_tasks.pop_front();
	}
	while (_tasks_light.empty() == false) {
		_tasks_light.front()->Dispose();
		_tasks_light.pop_front();
	}
	while (_tasks_medium.empty() == false) {
		_tasks_medium.front()->Dispose();
		_tasks_medium.pop_front();
	}
}

TaskController::~TaskController()
{
	if (_terminate == false)
	{
		// we are exiting unexpectedly
		// set terminate and detach the threads so we do not have to wait on them
		_terminate = true;
		for (auto& thread : _threads)
			thread.detach();
		while (_tasks.empty() == false) {
			_tasks.front()->Dispose();
			_tasks.pop_front();
		}
		while (_tasks_light.empty() == false) {
			_tasks_light.front()->Dispose();
			_tasks_light.pop_front();
		}
		while (_tasks_medium.empty() == false) {
			_tasks_medium.front()->Dispose();
			_tasks_medium.pop_front();
		}
	}
}

bool TaskController::Busy()
{
	bool running = true;
	for (int32_t i = 0; i < (int32_t)_status.size(); i++)
		running &= _status[i] == ThreadStatus::Running;
	return !_tasks.empty() || running;
}

void TaskController::InternalLoop_LightExclusive(int32_t number)
{
	// register new lua state for lua functions executed by the thread
	if (!_disableLua)
		Lua::RegisterThread(_sessiondata);

	while (true) {
		std::shared_ptr<Functions::BaseFunction> del;
		{
			std::unique_lock<std::mutex> guard(_lock);
			_status[number] = ThreadStatus::Waiting;
			// while freeze is [true], this will never return, if freeze is [false] it only returns when [tasks is non-empty], when [terinated and not waiting], or when [terminating and tasks is empty]
			_condition_light.wait(guard, [this] { return _freeze == false && (!_tasks_light.empty() || _terminate && _wait == false || _terminate && _tasks_light.empty()); });
			if (_terminate && _wait == false || _terminate && _tasks_light.empty())
				return;
			_status[number] = ThreadStatus::Running;
			del = _tasks_light.front();
			_tasks_light.pop_front();
		}
		if (del) {
			del->Run();
			del->Dispose();
			_completedjobs++;
		}
	}

	// unregister thread from lua functions
	if (!_disableLua)
		Lua::UnregisterThread();
}

void TaskController::InternalLoop_Light(int32_t number)
{
	// register new lua state for lua functions executed by the thread
	if (!_disableLua)
		Lua::RegisterThread(_sessiondata);

	while (true) {
		std::shared_ptr<Functions::BaseFunction> del;
		{
			std::unique_lock<std::mutex> guard(_lock);
			_status[number] = ThreadStatus::Waiting;
			// while freeze is [true], this will never return, if freeze is [false] it only returns when [tasks is non-empty], when [terinated and not waiting], or when [terminating and tasks is empty]
			_condition.wait(guard, [this] { return _freeze == false && (!_tasks_light.empty() || !_tasks_medium.empty() || _terminate && _wait == false || _terminate && _tasks_light.empty() && _tasks_medium.empty()); });
			if (_terminate && _wait == false || _terminate && _tasks_light.empty() && _tasks_medium.empty())
				return;
			_status[number] = ThreadStatus::Running;
			if (!_tasks_light.empty()) {
				del = _tasks_light.front();
				_tasks_light.pop_front();
			} else if (!_tasks_medium.empty()) {
				del = _tasks_medium.front();
				_tasks_medium.pop_front();
			}
		}
		if (del) {
			del->Run();
			del->Dispose();
			_completedjobs++;
		}
	}

	// unregister thread from lua functions
	if (!_disableLua)
		Lua::UnregisterThread();
}

void TaskController::InternalLoop(int32_t number)
{
	// register new lua state for lua functions executed by the thread
	if (!_disableLua)
		Lua::RegisterThread(_sessiondata);

	while (true) {
		std::shared_ptr<Functions::BaseFunction> del;
		{
			std::unique_lock<std::mutex> guard(_lock);
			_status[number] = ThreadStatus::Waiting;
			// while freeze is [true], this will never return, if freeze is [false] it only returns when [tasks is non-empty], when [terinated and not waiting], or when [terminating and tasks is empty]
			_condition.wait(guard, [this] { return _freeze == false && (!_tasks_medium.empty() || !_tasks.empty() || _terminate && _wait == false || _terminate && _tasks_medium.empty() && _tasks.empty()); });
			if (_terminate && _wait == false || _terminate && _tasks_medium.empty() && _tasks.empty())
				return;
			_status[number] = ThreadStatus::Running;
			if (!_tasks_medium.empty()) {
				del = _tasks_medium.front();
				_tasks_medium.pop_front();
			} else if (!_tasks.empty()) {
				del = _tasks.front();
				_tasks.pop_front();
			}
		}
		if (del) {
			del->Run();
			del->Dispose();
			_completedjobs++;
		}
	}

	// unregister thread from lua functions
	if (!_disableLua)
		Lua::UnregisterThread();
}

void TaskController::InternalLoop_SingleThread(int32_t number)
{
	// register new lua state for lua functions executed by the thread
	if (!_disableLua)
		Lua::RegisterThread(_sessiondata);

	while (true) {
		std::shared_ptr<Functions::BaseFunction> del;
		{
			std::unique_lock<std::mutex> guard(_lock);
			_status[number] = ThreadStatus::Waiting;
			// while freeze is [true], this will never return, if freeze is [false] it only returns when [tasks is non-empty], when [terinated and not waiting], or when [terminating and tasks is empty]
			_condition.wait(guard, [this] { return _freeze == false && (!_tasks_light.empty() || !_tasks_medium.empty() || !_tasks.empty() || _terminate && _wait == false || _terminate && _tasks_light.empty() && _tasks_medium.empty() && _tasks.empty()); });
			if (_terminate && _wait == false || _terminate && _tasks_light.empty() && _tasks_medium.empty() && _tasks.empty())
				return;
			_status[number] = ThreadStatus::Running;
			if (!_tasks_light.empty()) {
				del = _tasks_light.front();
				_tasks_light.pop_front();
			} else if (!_tasks_medium.empty()) {
				del = _tasks_medium.front();
				_tasks_medium.pop_front();
			} else if (!_tasks.empty()) {
				del = _tasks.front();
				_tasks.pop_front();
			}
		}
		if (del) {
			del->Run();
			del->Dispose();
			_completedjobs++;
		}
	}

	// unregister thread from lua functions
	if (!_disableLua)
		Lua::UnregisterThread();
}

uint64_t TaskController::GetCompletedJobs()
{
	return _completedjobs;
}

int32_t TaskController::GetWaitingJobs()
{
	return (int32_t)_tasks.size() + (int32_t)_tasks_medium.size() + (int32_t)_tasks_light.size();
}

int32_t TaskController::GetWaitingLightJobs()
{
	return (int32_t)_tasks_light.size();
}

int32_t TaskController::GetWaitingMediumJobs()
{
	return (int32_t)_tasks_medium.size();
}

void TaskController::ClearTasks()
{
	std::unique_lock<std::mutex> guard(_lock);
	while (_tasks.empty() == false) {
		_tasks.front()->Dispose();
		_tasks.pop_front();
	}
	while (_tasks_light.empty() == false) {
		_tasks_light.front()->Dispose();
		_tasks_light.pop_front();
	}
	while (_tasks_medium.empty() == false) {
		_tasks_medium.front()->Dispose();
		_tasks_medium.pop_front();
	}
}

size_t TaskController::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4                     // version
	                        + 1                     // terminate
	                        + 1                     // wait
	                        + 1                     // active
	                        + 4                     // numthreads
	                        + 8                     // size of waiting tasks
	                        + 8;                    // completedjobs
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
	for (auto task : _tasks)
	{
		if (task != nullptr)
		{
			sz += task->GetLength();
		}
	}
	return Form::GetDynamicSize()  // form stuff
	       + GetStaticSize(classversion) + sz;
}

bool TaskController::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(_terminate, buffer, offset);
	Buffer::Write(_wait, buffer, offset);
	Buffer::Write(_threads.size() > 0, buffer, offset);
	Buffer::Write(_numthreads, buffer, offset);
	Buffer::WriteSize(_tasks.size(), buffer, offset);
	for (auto task : _tasks)
	{
		task->WriteData(buffer, offset);
	}
	Buffer::Write((uint64_t)_completedjobs.load(), buffer, offset);
	return true;
}

bool TaskController::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			_terminate = Buffer::ReadBool(buffer, offset);
			_wait = Buffer::ReadBool(buffer, offset);
			bool active = Buffer::ReadBool(buffer, offset);
			_numthreads = Buffer::ReadInt32(buffer, offset);
			size_t num = Buffer::ReadSize(buffer, offset);
			for (int32_t i = 0; i < (int32_t)num; i++)
			{
				std::shared_ptr<Functions::BaseFunction> func = Functions::BaseFunction::Create(buffer, offset, length, resolver);
				_tasks.push_back(func);
			}
			_completedjobs = Buffer::ReadUInt64(buffer, offset);
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
	_terminate = true;
	_wait = false;
	_condition.notify_all();
	_condition_light.notify_all();
	for (std::thread& t : _threads)
		t.detach();
	_threads.clear();
	while (_tasks.empty() == false) {
		if (_tasks.front())
			_tasks.front()->Dispose();
		_tasks.pop_front();
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
	_freeze = true;
	// wait for all threads to freeze
	bool frozen = false;
	while (frozen == false && _threads.size() > 0)
	{
		frozen = true;
		for (int32_t i = 0; i < (int32_t)_status.size(); i++)
			frozen &= _status[i] == ThreadStatus::Waiting;
	}
	_lock.lock();
	loginfo("Frozen execution.");
}

bool TaskController::IsFrozen()
{
	return _freeze;
}

void TaskController::Thaw()
{
	loginfo("Thawing execution...");
	_freeze = false;
	_lock.unlock();
	_condition.notify_all();
	loginfo("Resumed execution.");
}

void TaskController::GetThreadStatus(std::vector<ThreadStatus>& status)
{
	if (status.size() < _status.size())
		status.resize(_status.size());
	for (int32_t i = 0; i < (int32_t)_status.size(); i++)
		status[i] = _status[i];
}
