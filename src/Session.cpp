#include <memory>
#include <stdlib.h>
#include <string>
#include <mutex>
#include <exception>

#include "ExitCodes.h"
#include "Logging.h"
#include "Session.h"
#include "Data.h"

Session* Session::GetSingleton()
{
	static Session session;
	return std::addressof(session);
}

Session::Session()
{
}

std::shared_ptr<Session> Session::CreateSeassion()
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
	if (data != nullptr) {
		auto tmp = data;
		data = nullptr;
		tmp->Clear();
	}
	LastError = 0;
	data = nullptr;
	running = false;
}

void Session::Wait()
{
	throw std::runtime_error("Not implemented");
}

std::vector<std::shared_ptr<Input>> Session::GenerateNegatives(int32_t /*negatives*/, bool& /*error*/, int32_t /*maxiterations*/, int32_t /*timeout*/, bool /*returnpositives*/)
{
	return {};
}

void Session::StopSession(bool savesession)
{
	if (savesession)
		data->Save();

	abort = true;
	if (_oracle) {
		_oracle.reset();
	}
	if (_controller) {
		_controller->Stop(false);
		_controller.reset();
	}
	if (_generator) {
		_generator.reset();
	}
	if (_grammar) {
		_grammar.reset();
	}
	if (_settings) {
		_settings.reset();
	}
	sessiondata.Clear();
	if (data != nullptr) {
		auto tmp = data;
		data = nullptr;
		tmp->Clear();
	}
}

void Session::StartSession(bool& error, bool globalTaskController, bool globalExecutionHandler, std::wstring settingsPath)
{
	// init settings
	_settings = data->CreateForm<Settings>();
	_settings->Load(settingsPath);
	data->_globalTasks = globalTaskController;
	data->_globalExec = globalExecutionHandler;
	// set taskcontroller
	_controller = data->CreateForm<TaskController>();
	// set executionhandler
	_exechandler = data->CreateForm<ExecutionHandler>();
	// start session
	StartSessionIntern(error);
}

void Session::StartSessionIntern(bool &error)
{
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
	// check the oracle for validity
	if (_oracle->Validate() == false) {
		logcritical("Oracle isn't valid.");
		LastError = ExitCodes::StartupError;
		error = true;
		return;
	}
	// set iteration variables
	_iteration = 0;
	// set generator

	//

	profile(TimeProfiling, "Time taken for session setup.");

	// start iterations

	_sessioncontroller = std::thread(&Session::SessionControl, this);
}

void Session::Snap()
{
	StartProfiling;
	// clear all running threads
	if (_controller) {
		_controller->Stop();
		_controller.reset();
	}
	// clean up generator leftovers for next iteration
	if (_generator) {
		_generator->Clean();
	}

	profile(TimeProfiling, "Time taken to create snap.");
}

void Session::Iterate()
{
	StartProfiling;
	// update iteration variables
	_iteration++;

	// start threadcontroller
	// threads need to be split into computing and test execution. if usehardwarethreads is activated, make sure we don't exceed.
	int32_t numc = _settings->general.numcomputingthreads;
	int32_t nume = _settings->general.concurrenttests;
	if (numc + nume > (int)std::thread::hardware_concurrency())
	{
		numc = (int)((((double)numc) / ((double)nume)) * std::thread::hardware_concurrency());
		nume = std::thread::hardware_concurrency() - numc;
	}
	_controller->Start(numc);

	// generate inputs, execute, sort relevance, delta debug


	// calculate generation exclusions

	// update generation values and grammar values



	profile(TimeProfiling, "Time taken for iteration {}", _iteration);
}

void Session::SessionControl()
{
	// this function controls the session.
	// it runs periodically and handles top-level processing and checks for abort conditions and success conditions

	while (!abort)
	{

	}
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

void Session::Delete(Data*)
{
	Clear();
}
