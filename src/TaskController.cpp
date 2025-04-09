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

/*
void TaskController::AddTask(std::shared_ptr<Functions::BaseFunction> a_task)
{
	if (a_task) {
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
		} else {
			logcritical("Tried to add nullptr as task to taskcontroller");
		}
}*/

void TaskController::Start(std::shared_ptr<SessionData> session, int32_t numthreads)
{
	_sessiondata = session;
	if (numthreads == 0)
		throw std::runtime_error("Cannot start a TaskController with 0 threads.");
	_numthreads = numthreads;
	if (_numthreads < 1)
		_numthreads = 1;
	if (_numthreads == 1) {
		_numLightThreads = 0;
		_numMediumThreads = 0;
		_numHeavyThreads = 0;
		_numAllThreads = 1;
		_controlEnableFine = false;
		_status.push_back(ThreadStatus::Initializing);
		_statusTime.push_back(std::chrono::steady_clock::now());
		_statusTask.push_back("None");
		_threads.emplace_back(std::thread(&TaskController::InternalLoop_SingleThread, this, 0));
	} else {
		_controlEnableFine = true;
		_controlEnableLight = true;
		_controlEnableMedium = false;
		_controlEnableHeavy = true;
		_numLightThreads = 1;
		_numMediumThreads = 0;
		_numHeavyThreads = _numthreads - 1;
		_numAllThreads = 0;
		for (int32_t i = 0; i < numthreads; i++) {
			_status.push_back(ThreadStatus::Initializing);
			_statusTime.push_back(std::chrono::steady_clock::now());
			_statusTask.push_back("None");
			if (i == 0)
				_threads.emplace_back(std::thread(&TaskController::InternalLoop_LightExclusive, this, i));
			else
				_threads.emplace_back(std::thread(&TaskController::InternalLoop_Heavy, this, i));
		}
	}
}

void TaskController::Start(std::shared_ptr<SessionData> session, int32_t numLightThreads, int32_t numMediumThreads, int32_t numHeavyThreads, int32_t numAllThreads)
{
	_sessiondata = session;
	if (numHeavyThreads == 0)
		throw std::runtime_error("Cannot start a TaskController with 0 heavy threads.");
	_numLightThreads = numLightThreads;
	_numMediumThreads = numMediumThreads;
	_numHeavyThreads = numHeavyThreads;
	_numAllThreads = numAllThreads;
	if (_numAllThreads == 0)
		_numthreads = numLightThreads + numMediumThreads + numHeavyThreads;
	else {
		_numthreads = numAllThreads;
		_numLightThreads = 0;
		_numMediumThreads = 0;
		_numHeavyThreads = 0;
	}
	if (_numthreads < 1 || _numthreads == 1) {
		_numthreads = 1;
		_numAllThreads = 1;
		_numLightThreads = 0;
		_numMediumThreads = 0;
		_numHeavyThreads = 0;
	}
	int32_t i = 0;
	if (_numthreads == 1 || _numAllThreads > 0) {
		_numLightThreads = 0;
		_numMediumThreads = 0;
		_numHeavyThreads = 0;
		_controlEnableFine = false;
		for (int32_t c = 0; c < _numAllThreads; c++, i++) {
			_status.push_back(ThreadStatus::Initializing);
			_statusTime.push_back(std::chrono::steady_clock::now());
			_statusTask.push_back("None");
			_threads.emplace_back(std::thread(&TaskController::InternalLoop_SingleThread, this, i));
		}
	} else {
		_controlEnableFine = true;
		if (_numLightThreads > 0)
			_controlEnableLight = true;
		for (int32_t c = 0; c < _numLightThreads; c++, i++) {
			_status.push_back(ThreadStatus::Initializing);
			_statusTime.push_back(std::chrono::steady_clock::now());
			_statusTask.push_back("None");
			_threads.emplace_back(std::thread(&TaskController::InternalLoop_LightExclusive, this, i));
		}
		if (_numMediumThreads > 0)
			_controlEnableMedium = true;
		for (int32_t c = 0; c < _numMediumThreads; c++, i++) {
			_status.push_back(ThreadStatus::Initializing);
			_statusTime.push_back(std::chrono::steady_clock::now());
			_statusTask.push_back("None");
			_threads.emplace_back(std::thread(&TaskController::InternalLoop_Medium, this, i));
		}
		if (_numHeavyThreads > 0)
			_controlEnableHeavy = true;
		for (int32_t c = 0; c < _numHeavyThreads; c++, i++) {
			_status.push_back(ThreadStatus::Initializing);
			_statusTime.push_back(std::chrono::steady_clock::now());
			_statusTask.push_back("None");
			_threads.emplace_back(std::thread(&TaskController::InternalLoop_Heavy, this, i));
		}
	}
}

int32_t TaskController::GetHeavyThreadCount()
{
	return _numHeavyThreads + _numAllThreads;
}

int32_t TaskController::GetMediumThreadCount()
{
	return _numMediumThreads + _numAllThreads;
}

int32_t TaskController::GetLightThreadCount()
{
	// all of the below can execute light threads and will do so first if some are available
	return (_numLightThreads + _numAllThreads);
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

	_status[number] = ThreadStatus::Waiting;
	while (true) {
		std::shared_ptr<Functions::BaseFunction> del;
		{
			std::unique_lock<std::mutex> guard(_lockLight);
			// while freeze is [true], this will never return, if freeze is [false] it only returns when [tasks is non-empty], when [terinated and not waiting], or when [terminating and tasks is empty]
			_condition_light.wait(guard, [this] { return _freeze == false && (!_tasks_light.empty() || _terminate && _wait == false || _terminate && _tasks_light.empty()); });
			if (_terminate && _wait == false || _terminate && _tasks_light.empty())
				return;
			del = _tasks_light.front();
			_tasks_light.pop_front();
		}
		if (del) {
			_status[number] = ThreadStatus::Running;
			_statusTime[number] = std::chrono::steady_clock::now();
			_statusTask[number] = del->GetName();
			del->Run();
			del->Dispose();
			_completedjobs++;
		}
		_status[number] = ThreadStatus::Waiting;
	}

	// unregister thread from lua functions
	if (!_disableLua)
		Lua::UnregisterThread();
}

void TaskController::InternalLoop_Medium(int32_t number)
{
	// register new lua state for lua functions executed by the thread
	if (!_disableLua)
		Lua::RegisterThread(_sessiondata);

	_status[number] = ThreadStatus::Waiting;
	while (true) {
		std::shared_ptr<Functions::BaseFunction> del;
		{
			std::unique_lock<std::mutex> guard(_lockMedium);
			// while freeze is [true], this will never return, if freeze is [false] it only returns when [tasks is non-empty], when [terinated and not waiting], or when [terminating and tasks is empty]
			_condition_medium.wait(guard, [this] { return _freeze == false && (!_tasks_medium.empty() || _terminate && _wait == false || _terminate && _tasks_medium.empty()); });
			if (_terminate && _wait == false || _terminate && _tasks_medium.empty())
				return;
			if (!_tasks_medium.empty()) {
				del = _tasks_medium.front();
				_tasks_medium.pop_front();
			}
		}
		if (del) {
			_status[number] = ThreadStatus::Running;
			_statusTime[number] = std::chrono::steady_clock::now();
			_statusTask[number] = del->GetName();
			del->Run();
			del->Dispose();
			_completedjobs++;
		}
		_status[number] = ThreadStatus::Waiting;
	}

	// unregister thread from lua functions
	if (!_disableLua)
		Lua::UnregisterThread();
}

void TaskController::InternalLoop_Heavy(int32_t number)
{
	// register new lua state for lua functions executed by the thread
	if (!_disableLua)
		Lua::RegisterThread(_sessiondata);

	_status[number] = ThreadStatus::Waiting;
	while (true) {
		std::shared_ptr<Functions::BaseFunction> del;
		{
			std::unique_lock<std::mutex> guard(_lock);
			// while freeze is [true], this will never return, if freeze is [false] it only returns when [tasks is non-empty], when [terinated and not waiting], or when [terminating and tasks is empty]
			_condition.wait(guard, [this] { return _freeze == false && (!_tasks.empty() || _terminate && _wait == false || _terminate && _tasks.empty()); });
			if (_terminate && _wait == false || _terminate && _tasks.empty())
				return;
			if (!_tasks.empty()) {
				del = _tasks.front();
				_tasks.pop_front();
			}
		}
		if (del) {
			_status[number] = ThreadStatus::Running;
			_statusTime[number] = std::chrono::steady_clock::now();
			_statusTask[number] = del->GetName();
			del->Run();
			del->Dispose();
			_completedjobs++;
		}
		_status[number] = ThreadStatus::Waiting;
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

	_status[number] = ThreadStatus::Waiting;
	while (true) {
		std::shared_ptr<Functions::BaseFunction> del;
		{
			std::unique_lock<std::mutex> guard(_lock);
			// while freeze is [true], this will never return, if freeze is [false] it only returns when [tasks is non-empty], when [terinated and not waiting], or when [terminating and tasks is empty]
			_condition.wait_for(guard, std::chrono::milliseconds(100), [this] { return _freeze == false && (!_tasks_light.empty() || !_tasks_medium.empty() || !_tasks.empty() || _terminate && _wait == false || _terminate && _tasks_light.empty() && _tasks_medium.empty() && _tasks.empty()); });
			if (_terminate && _wait == false || _terminate && _tasks_light.empty() && _tasks_medium.empty() && _tasks.empty())
				return;
			if (_freeze)
				continue;
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
			_status[number] = ThreadStatus::Running;
			_statusTime[number] = std::chrono::steady_clock::now();
			_statusTask[number] = del->GetName();
			del->Run();
			del->Dispose();
			_completedjobs++;
		}
		_status[number] = ThreadStatus::Waiting;
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

size_t TaskController::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4     // version
	                        + 1   // terminate
	                        + 1   // wait
	                        + 1   // active
	                        + 4   // numthreads
	                        + 4   // _numLightThreads
	                        + 4   // _numMediumThreads
	                        + 4   // _numHeavyThreads
	                        + 4   // _numAllThreads
	                        + 1   // _controlEnableFine
	                        + 1   // _controlEnableLight
	                        + 1   // _controlEnableMedium
	                        + 1   // _controlEnableHeavy
	                        + 8   // size of waiting tasks
	                        + 8   // size of waiting tasks_medium
	                        + 8   // size of waiting tasks_light
	                        + 8;  // completedjobs
	switch (version)
	{
	case 0x1:
		return size0x1;
	case 0x2:
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
	for (auto task : _tasks_medium) {
		if (task != nullptr) {
			sz += task->GetLength();
		}
	}
	for (auto task : _tasks_light) {
		if (task != nullptr) {
			sz += task->GetLength();
		}
	}
	sz += 8;
	for (auto [str, val] : _executedTasks)
	{
		sz += Buffer::CalcStringLength(str) + 8;
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
	Buffer::Write(_numLightThreads, buffer, offset);
	Buffer::Write(_numMediumThreads, buffer, offset);
	Buffer::Write(_numHeavyThreads, buffer, offset);
	Buffer::Write(_numAllThreads, buffer, offset);
	Buffer::Write(_controlEnableFine, buffer, offset);
	Buffer::Write(_controlEnableLight, buffer, offset);
	Buffer::Write(_controlEnableMedium, buffer, offset);
	Buffer::Write(_controlEnableHeavy, buffer, offset);
	Buffer::WriteSize(_tasks.size(), buffer, offset);
	for (auto task : _tasks)
	{
		task->WriteData(buffer, offset);
	}
	Buffer::WriteSize(_tasks_medium.size(), buffer, offset);
	for (auto task : _tasks_medium) {
		task->WriteData(buffer, offset);
	}
	Buffer::WriteSize(_tasks_light.size(), buffer, offset);
	for (auto task : _tasks_light) {
		task->WriteData(buffer, offset);
	}
	Buffer::Write((uint64_t)_completedjobs.load(), buffer, offset);

	Buffer::WriteSize(_executedTasks.size(), buffer, offset);
	for (auto [str, val] : _executedTasks)
	{
		Buffer::Write(str, buffer, offset);
		Buffer::Write(val, buffer, offset);
	}
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
	case 0x2:
		{
			Form::ReadData(buffer, offset, length, resolver);
			_terminate = Buffer::ReadBool(buffer, offset);
			_wait = Buffer::ReadBool(buffer, offset);
			bool active = Buffer::ReadBool(buffer, offset);
			_numthreads = Buffer::ReadInt32(buffer, offset);
			_numLightThreads = Buffer::ReadInt32(buffer, offset);
			_numMediumThreads = Buffer::ReadInt32(buffer, offset);
			_numHeavyThreads = Buffer::ReadInt32(buffer, offset);
			_numAllThreads = Buffer::ReadInt32(buffer, offset);
			_controlEnableFine = Buffer::ReadBool(buffer, offset);
			_controlEnableLight = Buffer::ReadBool(buffer, offset);
			_controlEnableMedium = Buffer::ReadBool(buffer, offset);
			_controlEnableHeavy = Buffer::ReadBool(buffer, offset);
			size_t num = Buffer::ReadSize(buffer, offset);
			for (int32_t i = 0; i < (int32_t)num; i++) {
				std::shared_ptr<Functions::BaseFunction> func = Functions::BaseFunction::Create(buffer, offset, length, resolver);
				_tasks.push_back(func);
			}
			num = Buffer::ReadSize(buffer, offset);
			for (int32_t i = 0; i < (int32_t)num; i++) {
				std::shared_ptr<Functions::BaseFunction> func = Functions::BaseFunction::Create(buffer, offset, length, resolver);
				_tasks_medium.push_back(func);
			}
			num = Buffer::ReadSize(buffer, offset);
			for (int32_t i = 0; i < (int32_t)num; i++) {
				std::shared_ptr<Functions::BaseFunction> func = Functions::BaseFunction::Create(buffer, offset, length, resolver);
				_tasks_light.push_back(func);
			}
			_completedjobs = Buffer::ReadUInt64(buffer, offset);
			if (active)
				//Start(_numthreads);
				active = false;
			num = Buffer::ReadSize(buffer, offset);
			for (size_t i = 0; i < num; i++)
			{
				std::string str = Buffer::ReadString(buffer, offset);
				int64_t val = Buffer::ReadInt64(buffer, offset);
				_executedTasks.insert_or_assign(str, val);
			}
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
	Form::ClearForm();
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
	while (_tasks_light.empty() == false) {
		if (_tasks_light.front())
			_tasks_light.front()->Dispose();
		_tasks_light.pop_front();
	}
	while (_tasks_medium.empty() == false) {
		if (_tasks_medium.front())
			_tasks_medium.front()->Dispose();
		_tasks_medium.pop_front();
	}
}

void TaskController::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}

size_t TaskController::MemorySize()
{
	return sizeof(TaskController);
}


void TaskController::RequestFreeze()
{
	loginfo("Requesting freezing of execution...");
	// set freezing to true
	_freeze = true;
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
	//_lock.lock();
	//_lockLight.lock();
	//_lockMedium.lock();
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
	//_lock.unlock();
	//_lockLight.unlock();
	//_lockMedium.unlock();
	_condition.notify_all();
	_condition_light.notify_all();
	_condition_medium.notify_all();
	loginfo("Resumed execution.");
}

void TaskController::ClearTasks()
{
	bool locks = false;
	if (!IsFrozen())
	{
		// was not frozen so get lock first
		_lock.lock();
		_lockLight.lock();
		_lockMedium.lock();
		locks = true;
	}
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
	if (locks)
	{
		_lock.unlock();
		_lockLight.unlock();
		_lockMedium.unlock();
	}
}

void TaskController::GetThreadStatus(std::vector<ThreadStatus>& status, std::vector<const char*>& names, std::vector<std::string>& time)
{
	if (status.size() < _status.size()) {
		status.resize(_status.size());
		names.resize(_status.size());
		time.resize(_status.size());
	}
	auto now = std::chrono::steady_clock::now();
	for (int32_t i = 0; i < (int32_t)_status.size(); i++) {
		status[i] = _status[i];
		names[i] = _statusTask[i];
		time[i] = Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(now - _statusTime[i]).count());
	}
}
