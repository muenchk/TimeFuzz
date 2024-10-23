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

std::shared_ptr<Session> Session::LoadSession(std::string name, std::wstring settingsPath)
{
	Data* dat = new Data();
	auto sett = dat->CreateForm<Settings>();
	sett->Load(settingsPath);
	dat->SetSavePath(sett->general.savepath);
	dat->Load(name);
	if (!dat->_loaded) {
		delete dat;
		return {};
	}
	return dat->CreateForm<Session>();
}

std::shared_ptr<Session> Session::LoadSession(std::string name, int32_t number, std::wstring settingsPath)
{
	Data* dat = new Data();
	auto sett = dat->CreateForm<Settings>();
	sett->Load(settingsPath);
	dat->SetSavePath(sett->general.savepath);
	dat->Load(name, number);
	if (!dat->_loaded) {
		delete dat;
		return {};
	}
	return dat->CreateForm<Session>();
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
	try {
		sessiondata.Clear();
	} catch (std::exception)
	{
		throw std::runtime_error("session clear fail");
	}
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

void Session::StartLoadedSession(bool& error, bool reloadsettings, std::wstring settingsPath, std::function<void()> callback)
{
	logmessage("Starting loaded session");
	// as Session itself is a Form and saved with the rest of the session, all internal variables are already set when we
	// resume the session, so we only have to set the runtime stuff, and potentially reload the settings and verify the oracle
	if (reloadsettings)
		_settings->Load(settingsPath);
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
	logmessage("Starting new session");
	// init settings
	_settings = data->CreateForm<Settings>();
	_settings->Load(settingsPath);
	data->_globalTasks = globalTaskController;
	data->_globalExec = globalExecutionHandler;
	// set taskcontroller
	_controller = data->CreateForm<TaskController>();
	// set executionhandler
	_exechandler = data->CreateForm<ExecutionHandler>();
	_callback = callback;
	// set save path
	data->SetSavePath(_settings->general.savepath);
	data->SetSaveName(_settings->general.savename);
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

		// start iterations

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
	_controller->Start(taskthreads);
	_self = data->CreateForm<Session>();
	_exechandler->Init(_self, _settings, _controller, _settings->general.concurrenttests, _oracle);
	_exechandler->StartHandlerAsIs();
	_excltree = data->CreateForm<ExclusionTree>();

	// run master control and kick stuff of
	// generates inputs, etc.
	SessionFunctions::MasterControl(_self);

	logmessage("Kicked session off.");
}

void Session::End()
{
	logmessage("Ending Session...");
	_hasFinished = true;
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
