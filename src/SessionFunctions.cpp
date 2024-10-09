#include "SessionFunctions.h"
#include "Session.h"
#include "Data.h"

#include <mutex>


void SessionFunctions::GenerateInput(std::shared_ptr<Session>& /*session*/)
{
	// generate a new input

	// setup input class

	// add test to execution handler

	throw new std::runtime_error("not implemented");
}

void SessionFunctions::SaveCheck(std::shared_ptr<Session>& session)
{
	// check whether to save at all
	if (!session->_settings->general.enablesaves)
		return;
	static auto autosavetime = std::chrono::steady_clock::now();
	static uint64_t testnum = 0;
	static std::mutex savelock;
	bool save = false;
	{
		std::unique_lock<std::mutex> guard(savelock);
		// check whether we should save now
		if (session->_settings->general.autosave_every_seconds > 0 && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - autosavetime).count() >= session->_settings->general.autosave_every_seconds) {
			// set new last save-time
			autosavetime = std::chrono::steady_clock::now();
			save = true;
		} 
		if (!save) {
			auto tn = SessionStatistics::TestsExecuted(session);
			if (session->_settings->general.autosave_every_tests > 0 && (int64_t)(tn - testnum) > session->_settings->general.autosave_every_tests) {  // set neew last test count
				testnum = tn;
				save = true;
			}
		}
	}

	if (!save)
		return;






	// perform the save
	throw new std::runtime_error("not implemented");
}

void SessionFunctions::SaveSession_Async(std::shared_ptr<Session> /*session*/)
{
	// async function that simply initiates the save
	// we are using this to free us from a potential deadlock in the TaskController
	// if the save is orchestrated by a callback within the TaskController
	// if we would allow it, the saving function that stops the taskcontroller temporarily
	// would deadlock itself
}

bool SessionFunctions::EndCheck(std::shared_ptr<Session>& session)
{
	// check whether end conditions are met
	throw new std::runtime_error("not implemented");

	// start thread that clears the session
	std::thread(EndSession_Async, session).detach();
}

void SessionFunctions::EndSession_Async(std::shared_ptr<Session> session)
{
	// async function that ends the session itself

	// force save
	session->data->Save();

	// end the session

	// set abort flag to catch all threads that try to escape
	session->abort = true;

	// notify all threads waiting on the session to end, of the sessions end
	session->_hasFinished = true;
	session->_waitSessionCond.notify_all();
	throw new std::runtime_error("not implemented");
}


void SessionFunctions::MasterControl(std::shared_ptr<Session>& session)
{
	// check whether we fulfilled our goals and can end the session
	if (EndCheck(session) == true)
		return;

	// check whether we need to generate new inputs
	// the goal is to always a certain number of tests prepared waiting for execution. 
	// Normally this can simply be achieved by adding a new test for each one that finishes,
	// but since stuff can happen and it doesn't cost much, we will check the number of
	// waiting tests against the parameters

	// number of new tests we need to generate
	int32_t generate = session->_settings->generation.generationsize - session->_exechandler->WaitingTasks();

	// check whether we should save


	/*  Data management?
	* check how much memory we are using, if too much:
	* delete some inputs from our memory of failed inputs
	* trim the exclusion tree for stray inputs (those that are very deep pathes and aren't often reached
	* ...
	*/



	
	throw new std::runtime_error("not implemented");
}
