#include <memory>
#include <stdlib.h>
#include <string>

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
	StartProfiling;
	// register settings
	_settings = settings;
	if (settings == nullptr) {
		logcritical("Didn't get a valid settings pointer.");
		exit(ExitCodes::StartupError);
	}
	// populate the oracle
	if (_settings->oracle == Oracle::OracleType::Undefined)
	{
		logcritical("The oracle type could not be identified");
		exit(ExitCodes::StartupError);
	}
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
	// set generator

	//

	profile(TimeProfiling, "Time taken for session setup.");

	// start iterations
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


void Session::Save()
{

}

void Session::Load()
{

}
