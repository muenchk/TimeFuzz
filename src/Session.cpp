#include <memory>
#include <stdlib.h>

#include "ExitCodes.h"
#include "Logging.h"
#include "Session.h"

Session* Session::GetSingleton()
{
	static Session session;
	return std::addressof(session);
}

void Session::StartSession(Settings* settings)
{
	// register settings
	_settings = settings;
	if (settings == nullptr) {
		logcritical("Didn't get a valid settings pointer.");
		exit(ExitCodes::StartupError);
	}

	// populate the oracle
	_oracle = std::make_shared<Oracle>(_settings->oracle, _settings->oraclepath);
	// check the oracle for validity
	if (_oracle->Validate() == false) {
		logcritical("Oracle isn't valid.");
		exit(ExitCodes::StartupError);
	}
	// set iteration variables
	_iteration = 0;
	// set threadcontroller
	_controller = std::make_shared<TaskController>();
	_controller->Start()
	// set generator

	//
}

void Session::Snap()
{
	// clear all running threads
	if (_controller) {
		_controller->Stop();
		_controller.reset();
	}
	// clean up generator leftovers for next iteration
	if (_generator) {
		_generator->Clean();
	}
}

void Session::Iterate()
{
	// update iteration variables
	iteration++;

	// generate inputs, execute, sort relevance, delta debug


	// calculate generation exclusions

	// update generation values and grammar values
}
