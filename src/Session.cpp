#include <memory>
#include <stdlib.h>
#include <string>
#include <mutex>

#include "ExitCodes.h"
#include "Logging.h"
#include "Session.h"

Session* Session::GetSingleton()
{
	static Session session;
	return std::addressof(session);
}

Session::~Session()
{
	Clear();
}

void Session::Clear()
{
	abort = true;
	if (_sessioncontroller.joinable())
		_sessioncontroller.join();
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
	data.Clear();
	LastError = 0;
	running = false;
}

void Session::Wait()
{
	throw std::runtime_error("Not implemented");
}

std::vector<std::shared_ptr<Input>> Session::GenerateNegatives(int /*negatives*/, bool& /*error*/, int /*maxiterations*/, int /*timeout*/, bool /*returnpositives*/)
{
	return {};
}

void Session::StartSession(std::shared_ptr<Settings> settings, bool &error)
{
	// register settings
	_settings = settings;
	if (settings.get() == nullptr) {
		logcritical("Didn't get a valid settings pointer.");
		LastError = ExitCodes::StartupError;
		error = true;
		return;
	}
	// set taskcontroller
	_controller = std::make_shared<TaskController>();
	// set executionhandler
	_exechandler = std::make_shared<ExecutionHandler>();
	// start session
	StartSessionIntern(error);
}

void Session::StartSession(bool& error, bool globalTaskController, bool globalExecutionHandler, bool globalSettings, std::wstring settingsPath)
{
	// init settings
	if (globalSettings == true)
		_settings = std::shared_ptr<Settings>(Settings::GetSingleton());
	else
		_settings = std::make_shared<Settings>();
	_settings->Load(settingsPath);
	// set taskcontroller
	if (globalTaskController)
		_controller = std::shared_ptr<TaskController>(TaskController::GetSingleton());
	else
		_controller = std::make_shared<TaskController>();
	// set executionhandler
	if (globalExecutionHandler)
		_exechandler = std::shared_ptr<ExecutionHandler>(ExecutionHandler::GetSingleton());
	else
		_exechandler = std::make_shared<ExecutionHandler>();
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
	_oracle = std::make_shared<Oracle>(_settings->oracle, _settings->oraclepath);
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
	int numc = _settings->general.numcomputingthreads;
	int nume = _settings->general.concurrenttests;
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

void Session::Save()
{

}

void Session::Load()
{

}
