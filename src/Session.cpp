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

std::vector<std::shared_ptr<Input>> Session::GenerateNegatives(int negatives, bool& error, int maxiterations, int timeout, bool returnpositives)
{

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
}

void Session::StartSession(bool &error)
{
	StartProfiling;
	// populate the oracle
	if (_settings->oracle == Oracle::OracleType::Undefined) {
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
	// set threadcontroller
	_controller = std::make_shared<TaskController>();
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
	_controller->Start(_settings->general.usehardwarethreads, _settings->general.numthreads);

	// generate inputs, execute, sort relevance, delta debug


	// calculate generation exclusions

	// update generation values and grammar values



	profile(TimeProfiling, "Time taken for iteration {}", _iteration);
}

void Session::SessionControl()
{
	// this function controls the session.
	// it runs periodically and handles top-level processing and checks for abort conditions and success conditions

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
