#include <memory>
#include <stdlib.h>
#include <string>
#include <mutex>
#include <exception>

#include "ExitCodes.h"
#include "Logging.h"
#include "Session.h"
#include "Data.h"
#include "SessionFunctions.h"
#include "LuaEngine.h"

Session* Session::GetSingleton()
{
	static Session session;
	return std::addressof(session);
}

Session::Session()
{
}

std::shared_ptr<Session> Session::CreateSession()
{
	Data* dat = new Data();
	return dat->CreateForm<Session>();
}

void Session::LoadSession_Async(Data* dat, std::string name, int32_t number,bool startsession)
{
	if (number == -1)
		dat->Load(name);
	else
		dat->Load(name, number);
	auto session = dat->CreateForm<Session>();
	session->loaded = true;
	bool error = false;
	if (startsession)
		session->StartLoadedSession(error);
}

std::shared_ptr<Session> Session::LoadSession(std::string name, std::wstring settingsPath, SessionStatus* status, bool startsession)
{
	Data* dat = new Data();
	auto sett = dat->CreateForm<Settings>();
	sett->Load(settingsPath);
	sett->Save(settingsPath);
	if (status != nullptr)
		InitStatus(status, sett);
	auto session = dat->CreateForm<Session>();
	session->_settings = sett;
	session->data = dat;
	dat->SetSavePath(sett->saves.savepath);
	std::thread th(LoadSession_Async, dat, name, -1, startsession);
	th.detach();
	return session;
}

std::shared_ptr<Session> Session::LoadSession(std::string name, int32_t number, std::wstring settingsPath, SessionStatus* status, bool startsession)
{
	Data* dat = new Data();
	auto sett = dat->CreateForm<Settings>();
	sett->Load(settingsPath);
	sett->Save(settingsPath);
	if (status != nullptr)
		InitStatus(status, sett);
	auto session = dat->CreateForm<Session>();
	session->_settings = sett;
	session->data = dat;
	dat->SetSavePath(sett->saves.savepath);
	std::thread th(LoadSession_Async, dat, name, number, startsession);
	th.detach();
	return session;
}

Session::~Session()
{
	Clear();
}

void Session::Clear()
{
	abort = true;
	if (_sessioncontroller.joinable())
		_sessioncontroller.detach();
	_sessioncontroller = {};
	if (_oracle)
		_oracle.reset();
	if (_controller)
	{
		_controller->Stop(false);
		_controller.reset();
	}
	if (_exechandler) {
		_exechandler->StopHandler();
		_exechandler->Clear();
		_exechandler.reset();
	}
	if (_generator)
	{
		_generator->Clear();
		_generator.reset();
	}
	if (_grammar)
	{
		_grammar->Clear();
		_grammar.reset();
	}
	if (_settings)
	{
		_settings.reset();
	}
	if (_excltree) {
		_excltree->Clear();
		_excltree.reset();
	}
	try {
		sessiondata.Clear();
	} catch (std::exception)
	{
		throw std::runtime_error("session clear fail");
	}
	Lua::DestroyAll();
	if (_self)
		_self.reset();
	if (data != nullptr) {
		auto tmp = data;
		data = nullptr;
		tmp->Clear();
	}
	LastError = 0;
	data = nullptr;
	running = false;
}

void Session::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}

void Session::Wait()
{
	// get mutex and wait until Finished becomes true, then simply return
	logmessage("Waiting for session to end");
	std::unique_lock<std::mutex> guard(_waitSessionLock);
	_waitSessionCond.wait(guard, [this] { return Finished(); });
	logmessage("Session Ended");
	return;
}

bool Session::Wait(std::chrono::nanoseconds timeout)
{
	// get mutex and wait until the timeout is over or we are notified of the sessions ending
	std::unique_lock<std::mutex> guard(_waitSessionLock);
	_waitSessionCond.wait_for(guard, timeout);
	// return whether the session is finished, this also means that the timeout has elapsed if it is not finished yet
	return Finished();

}

std::vector<std::shared_ptr<Input>> Session::GenerateNegatives(int32_t /*negatives*/, bool& /*error*/, int32_t /*maxiterations*/, int32_t /*timeout*/, bool /*returnpositives*/)
{
	return {};
}

void Session::StopSession(bool savesession)
{
	logmessage("Stopping session.");
	if (savesession)
		data->Save();

	// set abort -> session controller should automatically stop
	abort = true;
	// stop controller
	if (_controller) {
		_controller->Stop(false);
	}
	// stop executionHandler
	if (_exechandler) {
		_exechandler->StopHandler();
	}
	
	// don't clear any data, we may want to use the data for statistics, etc.
	
	// notify all threads waiting on the session to end, of the sessions end
	End();
}

void Session::DestroySession()
{
	// delete everything. If this isn't called the session is likely to persist until the program ends
	Delete(data);
}

void Session::Save()
{
	if (data)
		data->Save();
}

void Session::SetSessionEndCallback(std::function<void()> callback)
{
	_callback = callback;
}

void Session::StartLoadedSession(bool& error, bool reloadsettings, std::wstring settingsPath, std::function<void()> callback)
{
	if (loaded == false)
	{
		logcritical("Cannot start session before it has been fully loaded.");
		LastError = ExitCodes::StartupError;
		error = true;
		return;
	}
	logmessage("Starting loaded session");
	// as Session itself is a Form and saved with the rest of the session, all internal variables are already set when we
	// resume the session, so we only have to set the runtime stuff, and potentially reload the settings and verify the oracle
	if (reloadsettings) {
		_settings->Load(settingsPath);
		_settings->Save(settingsPath);
	}
	_callback = callback;
	if (_oracle->Validate() == false) {
		logcritical("Oracle isn't valid.");
		LastError = ExitCodes::StartupError;
		error = true;
		return;
	}

	_sessioncontroller = std::thread(&Session::SessionControl, this);
}

void Session::StartSession(bool& error, bool globalTaskController, bool globalExecutionHandler, std::wstring settingsPath, std::function<void()> callback)
{
	loaded = true;
	logmessage("Starting new session");
	// init settings
	_settings = data->CreateForm<Settings>();
	_settings->Load(settingsPath);
	_settings->Save(settingsPath);
	data->_globalTasks = globalTaskController;
	data->_globalExec = globalExecutionHandler;
	// set taskcontroller
	_controller = data->CreateForm<TaskController>();
	// set executionhandler
	_exechandler = data->CreateForm<ExecutionHandler>();
	_callback = callback;
	// set save path
	data->SetSavePath(_settings->saves.savepath);
	data->SetSaveName(_settings->saves.savename);
	// start session
	StartSessionIntern(error);
}

void Session::StartSessionIntern(bool &error)
{
	try {
		StartProfiling;
		// populate the oracle
		if (_settings->oracle == Oracle::PUTType::Undefined) {
			logcritical("The oracle type could not be identified");
			LastError = ExitCodes::StartupError;
			error = true;
			return;
		}
		_oracle = data->CreateForm<Oracle>();
		_oracle->Set(_settings->oracle, _settings->oraclepath);
		_oracle->SetLuaCmdArgs(CmdArgs::workdir / "CmdArgs.lua");
		_oracle->SetLuaOracle(CmdArgs::workdir / "Oracle.lua");
		// check the oracle for validity
		if (_oracle->Validate() == false) {
			logcritical("Oracle isn't valid.");
			LastError = ExitCodes::StartupError;
			error = true;
			return;
		}
		// set generator
		_generator = data->CreateForm<Generator>();
		_generator->Init();

		profile(TimeProfiling, "Time taken for session setup.");

		// start session controller

		_sessioncontroller = std::thread(&Session::SessionControl, this);
	}
	catch (std::exception& e)
	{
		std::cout << e.what();
	}
}

void Session::SessionControl()
{
	// this function controlls the session start. This could be easily done in a synchronous function but
	// its better to do it asyncronously to avoid any future threading problems
	// MIGHT MAKE THIS SYNCRONOUS IN THE FUTURE

	logmessage("Kicking off session...");

	// calculate number of threads to use for each controller and start them
	int32_t taskthreads = _settings->general.numcomputingthreads;
	int32_t execthreads = _settings->general.concurrenttests;
	if (_settings->general.usehardwarethreads)
	{
		// don't overshoot over actually available hardware threads
		int32_t max = (int)std::thread::hardware_concurrency();
		if (taskthreads + execthreads > max) {
			taskthreads = (int)((((double)taskthreads) / ((double)execthreads)) * std::thread::hardware_concurrency());
			execthreads = std::thread::hardware_concurrency() - taskthreads;
		}
	}
	if (taskthreads == 0)
		taskthreads = 1;
	if (execthreads == 0)
		execthreads = 1;
	_self = data->CreateForm<Session>();
	_controller->Start(_self, taskthreads);
	_exechandler->Init(_self, _settings, _controller, _settings->general.concurrenttests, _oracle);
	_exechandler->StartHandlerAsIs();
	_excltree = data->CreateForm<ExclusionTree>();

	running = true;
	data->StartClock();

	// run master control and kick stuff of
	// generates inputs, etc.
	Lua::RegisterThread(_self);
	SessionFunctions::MasterControl(_self);
	Lua::UnregisterThread();

	logmessage("Kicked session off.");
}

void Session::End()
{
	logmessage("Ending Session...");
	_hasFinished = true;
	running = false;
	_waitSessionCond.notify_all();
	if (_callback != nullptr) {
		logmessage("Executing session end callback...");
		_callback();
	}
	logmessage("Session Ended.");
}

std::string Session::PrintStats()
{
	std::string output;
	output += "Tests Executed:    " + std::to_string(SessionStatistics::TestsExecuted(_self)) + "\n";
	output += "Positive Tests:    " + std::to_string(SessionStatistics::PositiveTestsGenerated(_self)) + "\n";
	output += "Negative Tests:    " + std::to_string(SessionStatistics::NegativeTestsGenerated(_self)) + "\n";
	output += "Runtime:           " + std::to_string(SessionStatistics::Runtime(_self).count()) + "ns\n";
	return output;
}

void Session::InitStatus(SessionStatus& status)
{
	InitStatus(&status, _settings);
}

void Session::InitStatus(SessionStatus* status, std::shared_ptr<Settings> sett)
{
	if (sett->conditions.use_overalltests)
		status->overallTests_goal = sett->conditions.overalltests;
	else
		status->overallTests_goal = 0;
	if (sett->conditions.use_foundnegatives)
		status->negativeTests_goal = sett->conditions.foundnegatives;
	else
		status->negativeTests_goal = 0;
	if (sett->conditions.use_foundpositives)
		status->positiveTests_goal = sett->conditions.foundpositives;
	else
		status->positiveTests_goal = 0;
	if (sett->conditions.use_timeout)
		status->runtime_goal = std::chrono::seconds(sett->conditions.timeout);
	else
		status->runtime_goal = std::chrono::seconds(0);
	status->gnegative = -1.f;
	status->gpositive = -1.f;
	status->goverall = -1.f;
	status->gtime= -1.f;
	status->gsaveload = -1.f;
}

void Session::GetStatus(SessionStatus& status)
{
	if (loaded && running) {
		status.overallTests = SessionStatistics::TestsExecuted(_self);
		status.positiveTests = SessionStatistics::PositiveTestsGenerated(_self);
		status.negativeTests = SessionStatistics::NegativeTestsGenerated(_self);
		status.prunedTests = SessionStatistics::TestsPruned(_self);
		status.runtime = SessionStatistics::Runtime(_self);
		if (status.overallTests_goal != 0)
			status.goverall = (double)status.overallTests / (double)(status.overallTests_goal);
		else
			status.goverall = -1.0f;
		if (status.negativeTests_goal != 0)
			status.gnegative = (double)status.negativeTests / (double)status.negativeTests_goal;
		else
			status.gnegative = -1.0f;
		if (status.positiveTests_goal != 0)
			status.gpositive = (double)status.positiveTests / (double)status.positiveTests_goal;
		else
			status.gpositive = -1.0f;
		if (status.runtime_goal.count() != 0)
			status.gtime = (double)(std::chrono::duration_cast<std::chrono::seconds>(status.runtime).count()) / (double)status.runtime_goal.count();
		else
			status.gtime = -1;
	}
	status.saveload = data->actionloadsave;
	status.status = data->status;
	status.saveload_max = data->actionloadsave_max;
	status.saveload_current = data->actionloadsave_current;
	if (status.saveload_max != 0)
		status.gsaveload = (double)status.saveload_current / (double)status.saveload_max;
	else
		status.gsaveload = -1.f;
}

bool Session::IsRunning()
{
	std::lock_guard<std::mutex> guard(runninglock);
	return running;
}

bool Session::SetRunning(bool state)
{
	std::lock_guard<std::mutex> guard(runninglock);
	if (running != state) {
		// either we are starting a session or we are ending it, so this is a viable state
		running = state;
		return true;
	} else {
		// we are either not running a session and have encountered an unexpected situation,
		// or we are running a session and are trying to start another one
		logwarn("Trying to set session state has failes. Current state: {}", running);
		return false;
	}
}


size_t Session::GetStaticSize(int32_t version)
{
	size_t size0x1 = Form::GetDynamicSize()          // form size
	                 + 4                             // version
	                 + 8                             // grammar id
	                 + sessiondata.GetStaticSize();  // session data


	switch (version) {
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t Session::GetDynamicSize()
{
	return GetStaticSize() + sessiondata.GetDynamicSize();
}

bool Session::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	if (_grammar)
		Buffer::Write(_grammar->GetFormID(), buffer, offset);
	else
		Buffer::Write((uint64_t)0, buffer, offset);
	sessiondata.WriteData(buffer, offset);
	return true;
}

bool Session::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			FormID grammarid = Buffer::ReadUInt64(buffer, offset);
			sessiondata.ReadData(buffer, offset, length, resolver);
			resolver->AddTask([this, resolver, grammarid]() {
				auto oracle = resolver->ResolveFormID<Oracle>(Data::StaticFormIDs::Oracle);
				auto controller = resolver->ResolveFormID<TaskController>(Data::StaticFormIDs::TaskController);
				auto exechandler = resolver->ResolveFormID<ExecutionHandler>(Data::StaticFormIDs::ExecutionHandler);
				auto generator = resolver->ResolveFormID<Generator>(Data::StaticFormIDs::Generator);
				auto grammar = resolver->ResolveFormID<Grammar>(grammarid);
				auto settings = resolver->ResolveFormID<Settings>(Data::StaticFormIDs::Settings);
				auto excltree = resolver->ResolveFormID<ExclusionTree>(Data::StaticFormIDs::ExclusionTree);
				this->SetInternals(oracle, controller, exechandler, generator, grammar, settings, excltree);
			});
			return true;
		}
	default:
		return false;
	}
}


void Session::SetInternals(std::shared_ptr<Oracle> oracle, std::shared_ptr<TaskController> controller, std::shared_ptr<ExecutionHandler> exechandler, std::shared_ptr<Generator> generator, std::shared_ptr<Grammar> grammar, std::shared_ptr<Settings> settings, std::shared_ptr<ExclusionTree> excltree)
{
	_oracle = oracle;
	_controller = controller;
	_exechandler = exechandler;
	_generator = generator;
	_grammar = grammar;
	_settings = settings;
	_excltree = excltree;
}

void Session::Delete(Data*)
{
	Clear();
}
