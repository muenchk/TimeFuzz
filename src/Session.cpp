#include <memory>
#include <stdlib.h>
#include <string>
#include <mutex>
#include <exception>
#include <set>

#include "ExitCodes.h"
#include "Logging.h"
#include "Session.h"
#include "Data.h"
#include "SessionFunctions.h"
#include "LuaEngine.h"
#include "SessionData.h"
#include "ExclusionTree.h"
#include "Generation.h"
#include "Evaluation.h"

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

void Session::LoadSession_Async(Data* dat, std::string name, int32_t number, bool* error, LoadSessionArgs* args)
{
	Data::LoadSaveArgs loadArgs;
	if (args) {
		loadArgs.skipExlusionTree = args->skipExclusionTree;
	}
	if (number == -1)
		dat->Load(name, loadArgs);
	else
		dat->Load(name, number, loadArgs);
	if (dat->_loaded == false)
	{
		logcritical("CRITICAL ERROR");
		*error = true;
		return;
	}
	auto session = dat->CreateForm<Session>();
	session->_self = session;
	session->_loaded = true;
	if (args && args->loadNewGrammar)
	{
		auto sessdata = dat->CreateForm<SessionData>();
		// load grammar
		auto oldgrammar = sessdata->_grammar;
		sessdata->_grammar = dat->CreateForm<Grammar>();
		sessdata->_grammar->SetGenerationParameters(
			sessdata->_settings->methods.IterativeConstruction_Extension_Backtrack_min,
			sessdata->_settings->methods.IterativeConstruction_Extension_Backtrack_max,
			sessdata->_settings->methods.IterativeConstruction_Backtrack_Backtrack_min,
			sessdata->_settings->methods.IterativeConstruction_Backtrack_Backtrack_max);
		sessdata->_grammar->ParseScala(sessdata->_settings->oracle.grammar_path);
		if (!sessdata->_grammar->IsValid()) {
			logcritical("Grammar isn't valid. Re-setting old grammar.");
			dat->DeleteForm(sessdata->_grammar);
			sessdata->_grammar = oldgrammar;
		}
		else
		{
			sessdata->_generator->SetGrammar(sessdata->_grammar);
		}
	}
	// relaod settings
	if (args && args->reloadSettings)
	{
		auto sett = dat->CreateForm<Settings>();
		sett->Load(args->settingsPath);
	}
	if (args && args->clearTasks) {
		auto sessdata = dat->CreateForm<SessionData>();
		// clear tasks
		sessdata->_controller->ClearTasks();
		// clear tests
		sessdata->_exechandler->ClearTests();
	}
	if (args && args->startSession == true)
		session->StartLoadedSession(*error, args->reloadSettings, args->settingsPath);
	if (args)
		delete args;
}

std::shared_ptr<Session> Session::LoadSession(std::string name, LoadSessionArgs& loadargs, bool* error, SessionStatus* status)
{
	loginfo("Load Session");
	Data* dat = new Data();
	auto sett = dat->CreateForm<Settings>();
	sett->Load(loadargs.settingsPath);
	sett->Save(loadargs.settingsPath);
	logdebug("Handled settings");
	if (status != nullptr)
		InitStatus(status, sett);
	logdebug("Initialized status");
	auto session = dat->CreateForm<Session>();
	session->_sessiondata = dat->CreateForm<SessionData>();
	session->_sessiondata->_settings = sett;
	session->data = dat;
	if (loadargs.customsavepath)
		dat->SetSavePath(loadargs.savepath);
	else
		dat->SetSavePath(sett->saves.savepath);
	logdebug("Set save path");
	LoadSessionArgs* asyncargs = nullptr;
	if (loadargs.startSession) {
		asyncargs = new LoadSessionArgs;
		asyncargs->startSession = loadargs.startSession;
		asyncargs->reloadSettings = loadargs.reloadSettings;
		asyncargs->settingsPath = loadargs.settingsPath;
		asyncargs->loadNewGrammar = loadargs.loadNewGrammar;
		asyncargs->clearTasks = loadargs.clearTasks;
		asyncargs->skipExclusionTree = loadargs.skipExclusionTree;
		asyncargs->customsavepath = loadargs.customsavepath;
		asyncargs->savepath = loadargs.savepath;
	}
	std::thread th(LoadSession_Async, dat, name, -1, error, asyncargs);
	th.detach();
	logdebug("Started async loader");
	return session;
}

std::shared_ptr<Session> Session::LoadSession(std::string name, int32_t number, LoadSessionArgs& loadargs, bool* error, SessionStatus* status)
{
	loginfo("Load Session");
	Data* dat = new Data();
	auto sett = dat->CreateForm<Settings>();
	sett->Load(loadargs.settingsPath);
	sett->Save(loadargs.settingsPath);
	logdebug("Handled settings");
	if (status != nullptr)
		InitStatus(status, sett);
	logdebug("Initialized status");
	auto session = dat->CreateForm<Session>();
	session->_sessiondata = dat->CreateForm<SessionData>();
	session->_sessiondata->_settings = sett;
	session->data = dat;
	if (loadargs.customsavepath)
		dat->SetSavePath(loadargs.savepath);
	else
		dat->SetSavePath(sett->saves.savepath);
	logdebug("Set save path");
	LoadSessionArgs* asyncargs = nullptr;
	if (loadargs.startSession) {
		asyncargs = new LoadSessionArgs;
		asyncargs->startSession = loadargs.startSession;
		asyncargs->reloadSettings = loadargs.reloadSettings;
		asyncargs->settingsPath = loadargs.settingsPath;
		asyncargs->loadNewGrammar = loadargs.loadNewGrammar;
		asyncargs->clearTasks = loadargs.clearTasks;
		asyncargs->skipExclusionTree = loadargs.skipExclusionTree;
		asyncargs->customsavepath = loadargs.customsavepath;
		asyncargs->savepath = loadargs.savepath;
	}
	std::thread th(LoadSession_Async, dat, name, number, error, asyncargs);
	th.detach();
	logdebug("Started async loader");
	return session;
}

Session::~Session()
{
	Clear();
}

void Session::Clear()
{
	Form::ClearForm();
	_abort = true;
	if (_sessioncontroller.joinable())
		_sessioncontroller.detach();
	_sessioncontroller = {};
	if (_sessiondata) {
		if (_sessiondata->_oracle)
			_sessiondata->_oracle.reset();
		if (_sessiondata->_controller) {
			_sessiondata->_controller->Stop(false);
			_sessiondata->_controller.reset();
		}
		if (_sessiondata->_exechandler) {
			_sessiondata->_exechandler->StopHandler();
			_sessiondata->_exechandler->Clear();
			_sessiondata->_exechandler.reset();
		}
		if (_sessiondata->_generator) {
			_sessiondata->_generator->Clear();
			_sessiondata->_generator.reset();
		}
		if (_sessiondata->_grammar) {
			_sessiondata->_grammar->Clear();
			_sessiondata->_grammar.reset();
		}
		if (_sessiondata->_settings) {
			_sessiondata->_settings.reset();
		}
		if (_sessiondata->_excltree) {
			_sessiondata->_excltree->Clear();
			_sessiondata->_excltree.reset();
		}
		try {
			_sessiondata->Clear();
			_sessiondata.reset();
		} catch (std::exception) {
			throw std::runtime_error("session clear fail");
		}
	}
	Lua::DestroyAll();
	if (_self)
		_self.reset();
	if (data != nullptr) {
		//auto tmp = data;
		//data = nullptr;
		//tmp->Clear();
		data->Clear();
		data = nullptr;
	}
	_lastError = 0;
	data = nullptr;
	_running = false;
	_destroyed = true;
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

void Session::StopSession(bool savesession, bool stopHandler)
{
	if (_abort == true)
		return;
	logmessage("Stopping session...");
	if (savesession)
		data->Save({});
	data->EndClock();

	// set abort -> session controller should automatically stop
	_abort = true;
	_sessionControlWait.notify_all();
	if (_sessiondata) {
		if (stopHandler) {
			logmessage("Stopping handlers....");
			// stop executionhandler first, so no more new tests are started and the
			// taskcontroller can catch up if necessary
			if (_sessiondata->_exechandler) {
				_sessiondata->_exechandler->StopHandler();
				logmessage("Stopped ExecHandler.");
			}
			// stop controller
			if (_sessiondata->_controller) {
				_sessiondata->_controller->Stop(false);
				logmessage("Stopped Controller.");
			}
		} else {
			// we don't want to stop the handlers, after all we might want to check stuff, etc.
			// so try freezing them and delete any active tasks and tests
			// this will effectively end the session and any ongoing stuff, without affecting the 
			// availability of the handlers
			if (_sessiondata->_controller) {
				_sessiondata->_controller->Freeze();
				logmessage("Frozen controller.");
			}
			if (_sessiondata->_exechandler) {
				_sessiondata->_exechandler->Freeze(false);
				_sessiondata->_exechandler->ClearTests();
				_sessiondata->_exechandler->Thaw();
				logmessage("Stopped ExecHandler.");
			}
			if (_sessiondata->_controller) {
				_sessiondata->_controller->ClearTasks();
				_sessiondata->_controller->Thaw();
				logmessage("Stopped Controller.");
			}
		}
	}
	
	// don't clear any data, we may want to use the data for statistics, etc.
	logmessage("Stopped session.");
	
	// notify all threads waiting on the session to end, of the sessions end
	End();
}

void Session::DestroySession()
{
	// delete everything. If this isn't called the session is likely to persist until the program ends
	if (_sessiondata) {
		// stop executionhandler first, so no more new tests are started and the
		// taskcontroller can catch up if necessary
		if (_sessiondata->_exechandler) {
			_sessiondata->_exechandler->StopHandler();
		}
		// stop controller
		if (_sessiondata->_controller) {
			_sessiondata->_controller->Stop(false);
		}
	}
	_loaded = false;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	_lastError = 0;
	Delete(data);
}

void Session::Save()
{
	if (data)
		data->Save({});
}

void Session::SetSessionEndCallback(std::function<void()> callback)
{
	_callback = callback;
}

void Session::StartLoadedSession(bool& error, bool reloadsettings, std::wstring settingsPath, std::function<void()> callback)
{
	if (_loaded == false)
	{
		logcritical("Cannot start session before it has been fully loaded.");
		_lastError = ExitCodes::StartupError;
		error = true;
		return;
	}
	logmessage("Starting loaded session");
	// as Session itself is a Form and saved with the rest of the session, all internal variables are already set when we
	// resume the session, so we only have to set the runtime stuff, and potentially reload the settings and verify the oracle
	if (reloadsettings) {
		loginfo("Reloading settings...");
		printf("%ls\n", settingsPath.c_str());
		_sessiondata->_settings->Load(settingsPath, true);
		_sessiondata->_settings->Save(settingsPath);
		// reset oracle after settings reload
		if (_sessiondata->_settings->oracle.oracle == Oracle::PUTType::Undefined) {
			logcritical("The oracle type could not be identified");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}
		_sessiondata->_oracle = data->CreateForm<Oracle>();
		_sessiondata->_oracle->Set(_sessiondata->_settings->oracle.oracle, _sessiondata->_settings->GetOraclePath());
		// set lua cmdargs scripts
		auto path = std::filesystem::path(_sessiondata->_settings->oracle.lua_path_cmd);
		if (!std::filesystem::exists(path))
		{
			logcritical("Lua CmdArgs script cannot be found.");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}
		_sessiondata->_oracle->SetLuaCmdArgs(path);
		if (_sessiondata->_oracle->GetOracletype() == Oracle::PUTType::Script) {
			// set lua scriptargs scripts
			path = std::filesystem::path(_sessiondata->_settings->oracle.lua_path_script);
			if (!std::filesystem::exists(path)) {
				logcritical("Lua ScriptArgs script cannot be found.");
				_lastError = ExitCodes::StartupError;
				error = true;
				return;
			}
			_sessiondata->_oracle->SetLuaScriptArgs(path);
		}
		// set lua cmd args replay
		path = std::filesystem::path(_sessiondata->_settings->oracle.lua_path_cmd_replay);
		if (std::filesystem::exists(path)) {
			_sessiondata->_oracle->SetLuaCmdArgsReplay(path);
		}
		// set lua oracle script
		path = std::filesystem::path(_sessiondata->_settings->oracle.lua_path_oracle);
		if (!std::filesystem::exists(path)) {
			logcritical("Lua Oracle script cannot be found.");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}
		_sessiondata->_oracle->SetLuaOracle(path);
	} else
		loginfo("Proceeding with settings from savefile.");
	if (callback != nullptr)
		_callback = callback;
	if (_sessiondata->_oracle->Validate() == false) {
		logcritical("Oracle isn't valid.");
		_lastError = ExitCodes::StartupError;
		error = true;
		return;
	}

	_sessioncontroller = std::thread(&Session::SessionControl, this);
}

void Session::StartSession(bool& error, bool disableexclusiontree, bool globalTaskController, bool globalExecutionHandler, std::wstring settingsPath, std::function<void()> callback, bool customsavepath, std::filesystem::path savepath)
{
	_loaded = true;
	logmessage("Starting new session");
	// init sessiondata
	_sessiondata = data->CreateForm<SessionData>();
	_sessiondata->data = data;
	// init settings
	_sessiondata->_settings = data->CreateForm<Settings>();
	_sessiondata->_settings->Load(settingsPath);
	_sessiondata->_settings->Save(settingsPath);
	_sessiondata->Init();
	data->_globalTasks = globalTaskController;
	data->_globalExec = globalExecutionHandler;
	// set taskcontroller
	_sessiondata->_controller = data->CreateForm<TaskController>();
	// set executionhandler
	_sessiondata->_exechandler = data->CreateForm<ExecutionHandler>();
	if (callback != nullptr)
		_callback = callback;
	// set save path
	if (customsavepath)
		data->SetSavePath(savepath);
	else
		data->SetSavePath(_sessiondata->_settings->saves.savepath);
	data->SetSaveName(_sessiondata->_settings->saves.savename);
	// disable exclusion tree if set
	if (disableexclusiontree)
		_sessiondata->_settings->runtime.enableExclusionTree = false;
	// start session
	StartSessionIntern(error);
}

void Session::StartSessionIntern(bool &error)
{
	try {
		StartProfiling;
		// populate the oracle
		if (_sessiondata->_settings->oracle.oracle == Oracle::PUTType::Undefined) {
			logcritical("The oracle type could not be identified");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}
		_sessiondata->_oracle = data->CreateForm<Oracle>();
		_sessiondata->_oracle->Set(_sessiondata->_settings->oracle.oracle, _sessiondata->_settings->GetOraclePath());
		// set lua cmdargs scripts
		auto path = std::filesystem::path(_sessiondata->_settings->oracle.lua_path_cmd);
		if (!std::filesystem::exists(path)) {
			logcritical("Lua CmdArgs script cannot be found.");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}
		_sessiondata->_oracle->SetLuaCmdArgs(path);
		if (_sessiondata->_oracle->GetOracletype() == Oracle::PUTType::Script) {
			// set lua scriptargs scripts
			path = std::filesystem::path(_sessiondata->_settings->oracle.lua_path_script);
			if (!std::filesystem::exists(path)) {
				logcritical("Lua ScriptArgs script cannot be found.");
				_lastError = ExitCodes::StartupError;
				error = true;
				return;
			}
			_sessiondata->_oracle->SetLuaScriptArgs(path);
		}
		// set lua oracle script
		path = std::filesystem::path(_sessiondata->_settings->oracle.lua_path_oracle);
		if (!std::filesystem::exists(path)) {
			logcritical("Lua Oracle script cannot be found.");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}
		_sessiondata->_oracle->SetLuaOracle(path);
		// set lua cmd args replay
		path = std::filesystem::path(_sessiondata->_settings->oracle.lua_path_cmd_replay);
		if (std::filesystem::exists(path)) {
			_sessiondata->_oracle->SetLuaCmdArgsReplay(path);
		}
		// set lua oracle script
		// check the oracle for validity
		if (_sessiondata->_oracle->Validate() == false) {
			logcritical("Oracle isn't valid.");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}

		// load grammar
		_sessiondata->_grammar = data->CreateForm<Grammar>();
		_sessiondata->_grammar->SetGenerationParameters(
			_sessiondata->_settings->methods.IterativeConstruction_Extension_Backtrack_min,
			_sessiondata->_settings->methods.IterativeConstruction_Extension_Backtrack_max,
			_sessiondata->_settings->methods.IterativeConstruction_Backtrack_Backtrack_min,
			_sessiondata->_settings->methods.IterativeConstruction_Backtrack_Backtrack_max);
		_sessiondata->_grammar->ParseScala(_sessiondata->_settings->oracle.grammar_path);
		if (!_sessiondata->_grammar->IsValid()) {
			logcritical("Grammar isn't valid.");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}

		// init generation
		_sessiondata->SetNewGeneration(true);

		// set generator
		_sessiondata->_generator = data->CreateForm<Generator>();
		_sessiondata->_generator->Init();
		_sessiondata->_generator->SetGrammar(_sessiondata->_grammar);

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

	// get session data
	_sessiondata = data->CreateForm<SessionData>();
	_sessiondata->data = data;

	// calculate number of threads to use for each controller and start them
	int32_t taskthreads = _sessiondata->_settings->general.numcomputingthreads;
	int32_t execthreads = _sessiondata->_settings->general.concurrenttests;
	if (_sessiondata->_settings->general.usehardwarethreads)
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
	if (_sessiondata->_settings->controller.activateSettings)
		_sessiondata->_controller->Start(_sessiondata, _sessiondata->_settings->controller.numLightThreads, _sessiondata->_settings->controller.numMediumThreads, _sessiondata->_settings->controller.numHeavyThreads, _sessiondata->_settings->controller.numAllThreads);
	else
		_sessiondata->_controller->Start(_sessiondata, taskthreads);
	_sessiondata->_exechandler->Init(_self, _sessiondata, _sessiondata->_settings, _sessiondata->_controller, _sessiondata->_settings->general.concurrenttests, _sessiondata->_oracle);
	_sessiondata->_exechandler->SetEnableFragments(_sessiondata->_settings->tests.executeFragments);
	_sessiondata->_exechandler->SetPeriod(_sessiondata->_settings->general.testEnginePeriod());
	_sessiondata->_exechandler->StartHandlerAsIs();
	_sessiondata->_excltree = data->CreateForm<ExclusionTree>();
	_sessiondata->_excltree->Init(_sessiondata);

	_running = true;
	data->StartClock();

	// set max gen callbacks 
	_sessiondata->SetMaxGenerationCallbacks(_sessiondata->_controller->GetNumThreads() * 5);

	// run master control and kick stuff of
	// generates inputs, etc.
	Lua::RegisterThread(_sessiondata);
	if (CmdArgs::_clearTasks == false) {
		SessionFunctions::MasterControl(_sessiondata);
		SessionFunctions::GenerateTests(_sessiondata);
	}
	Lua::UnregisterThread();

	logmessage("Kicked session off.");
	std::unique_lock<std::mutex> guard(_sessionControlMutex);
	logmessage("Entering into Deadlock avoidance mode");
	while (_abort == false)
	{
		// sometimes shit happens in multithreaded applications where things go wrong, especially under linux 
		// and with inter-process communication.
		// To avoid some problems this thread is gonna periodically check whether shit has gone wrong and we need to restart / push stuff
		_sessionControlWait.wait_for(guard, std::chrono::milliseconds(500));
		//logmessage("SessionControl bump, {}, {}", _abort, _paused);
		if (_abort)
			return;
		if (_paused)
			continue;

		//UI_GetDatabaseObjectStatus();

		if (_sessiondata->_controller->GetWaitingJobs() == 0 && !_sessiondata->_controller->IsFrozen() && (_sessiondata->_exechandler->GetWaitingTests() == 0 && _sessiondata->_exechandler->GetInitializedTests() == 0 && _sessiondata->_exechandler->GetRunningTests() == 0 && _sessiondata->_exechandler->IsFrozen() == false))
		{
			//logwarn("Work you lazy bastards.");
			// check wether the generation should end
			if (_sessiondata->CheckGenerationEnd() == false && CmdArgs::_clearTasks == false)
			{
				// if it isn't ending give generation a little push
				int32_t maxtasks = std::min((int32_t)(_sessiondata->_controller->GetHeavyThreadCount() * 0.8), _sessiondata->_settings->generation.activeGeneratedInputs / _sessiondata->_settings->generation.generationstep);
				if (maxtasks < 1)
					maxtasks = 1;
				if (maxtasks == _sessiondata->_controller->GetHeavyThreadCount() && maxtasks != 1)
					maxtasks = _sessiondata->_controller->GetHeavyThreadCount() - 1;
				for (int32_t i = 0; i < maxtasks; i++)
					SessionFunctions::GenerateTests(_sessiondata);
			}

			// check whether dd is running ans has decided to hang itself
			auto gen = _sessiondata->GetCurrentGeneration();
			loginfo("Check for dd");
			if (gen && gen->IsDeltaDebuggingActive() && CmdArgs::_clearTasks == false)
			{
				auto visitor = [sessiondata = _sessiondata](std::shared_ptr<DeltaDebugging::DeltaController> controller) {
					if (controller && controller->GetBatchTasks())
						loginfo("visiting {}, tasks: {}, sendend: {}, procend: {}", controller->GetFormID(), controller->GetBatchTasks()->tasks.load(), controller->GetBatchTasks()->sendEndEvent, controller->GetBatchTasks()->processedEndEvent);
					if (controller && !controller->Finished())
					{
						// if not finished check how many tasks are active
						auto tasks = controller->GetBatchTasks();
						if (tasks && tasks->tasks == 0 && tasks->sendEndEvent == false) {
							logwarn("DeltaController has tasks at 0, but did not send end event");
							auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
							callback->_DDcontroller = controller;
							tasks->sendEndEvent = true;
							sessiondata->_controller->AddTask(callback);
						} else if (tasks && tasks->tasks == 0 && tasks->processedEndEvent == false) {
							logwarn("DeltaController has tasks at 0, but did not process end event");
							auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
							callback->_DDcontroller = controller;
							tasks->sendEndEvent = true;
							sessiondata->_controller->AddTask(callback);
						}
					}
					return false;
				};
				gen->VisitDeltaDebugging(visitor);
			}
		}
		if (_sessiondata->_exechandler->IsStale(std::chrono::milliseconds(10000)))
		{
			logwarn("ExecHandler is stale, restarting...");
			_sessiondata->_exechandler->ReinitHandler();
		}
	}
	loginfo("Exiting sessioncontrol");
}

void Session::End()
{
	logmessage("Ending Session...");
	_hasFinished = true;
	_running = false;
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
	output += "Tests Executed:    " + std::to_string(SessionStatistics::TestsExecuted(_sessiondata)) + "\n";
	output += "Positive Tests:    " + std::to_string(SessionStatistics::PositiveTestsGenerated(_sessiondata)) + "\n";
	output += "Negative Tests:    " + std::to_string(SessionStatistics::NegativeTestsGenerated(_sessiondata)) + "\n";
	output += "Runtime:           " + std::to_string(SessionStatistics::Runtime(_sessiondata).count()) + "ns\n";
	return output;
}

void Session::InitStatus(SessionStatus& status)
{
	InitStatus(&status, _sessiondata->_settings);
}

void Session::InitStatus(SessionStatus* status, std::shared_ptr<Settings> sett)
{
	status->sessionname = sett->saves.savename;
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
	if (_loaded) {
		status.overallTests = SessionStatistics::TestsExecuted(_sessiondata);
		status.positiveTests = SessionStatistics::PositiveTestsGenerated(_sessiondata);
		status.negativeTests = SessionStatistics::NegativeTestsGenerated(_sessiondata);
		status.unfinishedTests = SessionStatistics::UnfinishedTestsGenerated(_sessiondata);
		status.undefinedTests = SessionStatistics::UndefinedTestsGenerated(_sessiondata);
		status.prunedTests = SessionStatistics::TestsPruned(_sessiondata);
		status.runtime = SessionStatistics::Runtime(_sessiondata);
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

		// ExclusionTree
		status.excl_depth = _sessiondata->_excltree->GetDepth();
		status.excl_nodecount = _sessiondata->_excltree->GetNodeCount();
		status.excl_leafcount = _sessiondata->_excltree->GetLeafCount();

		// TaskController
		status.task_completed = _sessiondata->_controller->GetCompletedJobs();
		status.task_waiting = _sessiondata->_controller->GetWaitingJobs();
		status.task_waiting_light = _sessiondata->_controller->GetWaitingLightJobs();
		status.task_waiting_medium = _sessiondata->_controller->GetWaitingMediumJobs();
		
		// ExecutionHandler
		status.exec_running = _sessiondata->_exechandler->GetRunningTests();
		status.exec_waiting = _sessiondata->_exechandler->GetWaitingTests();
		status.exec_initialized = _sessiondata->_exechandler->GetInitializedTests();
		status.exec_stopping = _sessiondata->_exechandler->GetStoppingTests();

		// Generation
		status.gen_generatedInputs = _sessiondata->GetGeneratedInputs();
		status.gen_generatedWithPrefix = _sessiondata->GetGeneratedPrefix();
		status.gen_excludedApprox = _sessiondata->GetExcludedApproximation();
		status.gen_generationFails = _sessiondata->GetGenerationFails();
		status.gen_failureRate = _sessiondata->GetGenerationFailureRate();
		status.gen_addtestfails = _sessiondata->GetAddTestFails();

		// Tests
		status.exitstats = _sessiondata->GetTestExitStats();
	}
	if (data != nullptr) {
		status.saveload = data->_actionloadsave;
		status.status = data->_status;
		status.record = data->_record;
		status.saveload_max = data->_actionloadsave_max;
		status.saveload_current = data->_actionloadsave_current;
		status.saveload_record_len = data->_actionrecord_len;
		status.saveload_record_current = data->_actionrecord_offset;
	}
	if (status.saveload_max != 0)
		status.gsaveload = (double)status.saveload_current / (double)status.saveload_max;
	else
		status.gsaveload = -1.f;

	if (status.saveload_record_len != 0)
		status.grecord = (double)status.saveload_record_current / (double)status.saveload_record_len;
	else
		status.grecord = -1.f;
}

bool Session::IsRunning()
{
	std::lock_guard<std::mutex> guard(_runninglock);
	return _running;
}

bool Session::SetRunning(bool state)
{
	std::lock_guard<std::mutex> guard(_runninglock);
	if (_running != state) {
		// either we are starting a session or we are ending it, so this is a viable state
		_running = state;
		return true;
	} else {
		// we are either not running a session and have encountered an unexpected situation,
		// or we are running a session and are trying to start another one
		logwarn("Trying to set session state has failes. Current state: {}", _running);
		return false;
	}
}


size_t Session::GetStaticSize(int32_t version)
{
	size_t size0x1 = 4;                    // version

	switch (version) {
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t Session::GetDynamicSize()
{
	return Form::GetDynamicSize()  // form stuff
	       + GetStaticSize(classversion);
}

bool Session::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	return true;
}

bool Session::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			return true;
		}
	default:
		return false;
	}
}

void Session::Delete(Data*)
{
	Clear();
}

size_t Session::MemorySize()
{
	return sizeof(Session);
}


void Session::Replay(FormID inputid, UI::UIInputInformation* feedback)
{
	auto input = data->LookupFormID<Input>(inputid);
	if (input) {
		bool reg = Lua::RegisterThread(_sessiondata);
		auto newinput = data->CreateForm<Input>();
		input->DeepCopy(newinput);
		auto callback = dynamic_pointer_cast<Functions::ReplayTestCallback>(Functions::ReplayTestCallback::Create());
		callback->_sessiondata = _sessiondata;
		callback->_input = newinput;
		callback->_feedback = feedback;
		_sessiondata->_exechandler->AddTest(newinput, callback, true, true);
		if (reg)
			Lua::UnregisterThread();
	}
}

void Session::UI_GetTopK(std::vector<UI::UIInput>& vector, size_t k)
{
	if (!_loaded)
		return;
	if (vector.size() < k)
		vector.resize(k);
	std::vector<std::shared_ptr<Input>> vec;
	switch (_sessiondata->_settings->generation.sourcesType) {
	case Settings::GenerationSourcesType::FilterLength:
		vec = _sessiondata->GetTopK_Length((int32_t)k);
		break;
	case Settings::GenerationSourcesType::FilterPrimaryScore:
	case Settings::GenerationSourcesType::FilterPrimaryScoreRelative:
		vec = _sessiondata->GetTopK((int32_t)k);
		break;
	case Settings::GenerationSourcesType::FilterSecondaryScore:
	case Settings::GenerationSourcesType::FilterSecondaryScoreRelative:
		vec = _sessiondata->GetTopK_Secondary((int32_t)k);
		break;
	}
	for (int32_t i = 0; i < (int32_t)vec.size(); i++)
	{
		vector[i].id = vec[i]->GetFormID();
		vector[i].length = vec[i]->EffectiveLength();
		vector[i].primaryScore = vec[i]->GetPrimaryScore();
		vector[i].secondaryScore = vec[i]->GetSecondaryScore();
		vector[i].result = (UI::Result)vec[i]->GetOracleResult();
		vector[i].flags = vec[i]->GetFlags();
		vector[i].derivedInputs = vec[i]->GetDerivedInputs();
		vector[i].exectime = vec[i]->GetExecutionTime();
		vector[i].generationNumber = _sessiondata->GetGeneration(vec[i]->GetGenerationID())->GetGenerationNumber();
	}
}

void Session::UI_GetPositiveInputs(std::vector<UI::UIInput>& vector, size_t k)
{
	if (!_loaded)
		return;
	int* c = new int;
	*c = 0;
	std::vector<UI::UIInput>* vec = &vector; 
	auto visitor = [&k, &c, &vec, sessdata = _sessiondata](std::shared_ptr<Input> input) {
		if ((int)vec->size() > *c) {
			(*vec)[*c].id = input->GetFormID();
			(*vec)[*c].length = input->EffectiveLength();
			(*vec)[*c].primaryScore = input->GetPrimaryScore();
			(*vec)[*c].secondaryScore = input->GetSecondaryScore();
			(*vec)[*c].result = (UI::Result)input->GetOracleResult();
			(*vec)[*c].flags = input->GetFlags();
			(*vec)[*c].derivedInputs = input->GetDerivedInputs();
			(*vec)[*c].exectime = input->GetExecutionTime();
			(*vec)[*c].generationNumber = sessdata->GetGeneration(input->GetGenerationID())->GetGenerationNumber();
		}
		else
		{
			UI::UIInput uinp;
			uinp.id = input->GetFormID();
			uinp.length = input->EffectiveLength();
			uinp.primaryScore = input->GetPrimaryScore();
			uinp.secondaryScore = input->GetSecondaryScore();
			uinp.result = (UI::Result)input->GetOracleResult();
			uinp.flags = input->GetFlags();
			uinp.derivedInputs = input->GetDerivedInputs();
			uinp.exectime = input->GetExecutionTime();
			uinp.generationNumber = sessdata->GetGeneration(input->GetGenerationID())->GetGenerationNumber();
			vec->push_back(uinp);
		}
		(*c)++;
		if (k == 0 || *c < k) {
			return true;
		} else
			return false;
	};
	_sessiondata->VisitPositiveInputs(visitor, vector.size());
	delete c;
}


void Session::UI_GetLastRunInputs(std::vector<UI::UIInput>& vector, size_t k)
{
	if (!_loaded)
		return;
	int* c = new int;
	*c = 0;
	std::vector<UI::UIInput>* vec = &vector;
	auto visitor = [&k, &c, &vec, sessdata = _sessiondata](std::shared_ptr<Input> input) {
		if ((int)vec->size() > *c) {
			(*vec)[*c].id = input->GetFormID();
			(*vec)[*c].length = input->EffectiveLength();
			(*vec)[*c].primaryScore = input->GetPrimaryScore();
			(*vec)[*c].secondaryScore = input->GetSecondaryScore();
			(*vec)[*c].result = (UI::Result)input->GetOracleResult();
			(*vec)[*c].flags = input->GetFlags();
			(*vec)[*c].derivedInputs = input->GetDerivedInputs();
			(*vec)[*c].exectime = input->GetExecutionTime();
			(*vec)[*c].generationNumber = sessdata->GetGeneration(input->GetGenerationID())->GetGenerationNumber();
		} else {
			UI::UIInput uinp;
			uinp.id = input->GetFormID();
			uinp.length = input->EffectiveLength();
			uinp.primaryScore = input->GetPrimaryScore();
			uinp.secondaryScore = input->GetSecondaryScore();
			uinp.result = (UI::Result)input->GetOracleResult();
			uinp.flags = input->GetFlags();
			uinp.derivedInputs = input->GetDerivedInputs();
			uinp.exectime = input->GetExecutionTime();
			uinp.generationNumber = sessdata->GetGeneration(input->GetGenerationID())->GetGenerationNumber();
			vec->push_back(uinp);
		}
		(*c)++;
		if (k == 0 || *c < k) {
			return true;
		} else
			return false;
	};
	_sessiondata->VisitLastRun(visitor);
	delete c;
}

UI::UIDeltaDebugging Session::UI_StartDeltaDebugging(FormID inputid)
{
	if (!_loaded)
		return {};
	bool reg = Lua::RegisterThread(_sessiondata);
	UI::UIDeltaDebugging dd;
	if (inputid == 0) {
		auto vec = _sessiondata->GetTopK_Unfinished((int32_t)1);

		if (vec.size() > 0) {
			auto ddcontroller = SessionFunctions::BeginDeltaDebugging(_sessiondata, vec[0], {}, true);
			if (ddcontroller) {
				dd.SetDeltaController(ddcontroller);
				_sessiondata->GetCurrentGeneration()->AddDDController(ddcontroller);
			}
		}
	} else {
		auto input = data->LookupFormID<Input>(inputid);
		auto ddcontroller = SessionFunctions::BeginDeltaDebugging(_sessiondata, input, {}, true);
		if (ddcontroller) {
			dd.SetDeltaController(ddcontroller);
			_sessiondata->GetCurrentGeneration()->AddDDController(ddcontroller);
		}
	}
	if (reg)
		Lua::UnregisterThread();
	return dd;
}


void Session::UI_FindDeltaDebugging(UI::UIDeltaDebugging& dd)
{
	if (!_loaded)
		return;
	auto vec = _sessiondata->data->GetFormArray<DeltaDebugging::DeltaController>();
	if (vec.size() > 0) {
		for (int32_t i = 0; i < (int32_t)vec.size(); i++) {
			if (vec[i]->Finished() == false) {
				dd.SetDeltaController(vec[0]);
				return;
			}
		}
	} else
		dd.SetDeltaController({});
}

void Session::UI_FindAllDeltaDebugging(std::vector<UI::UIDeltaDebugging>& ddvector,size_t& length)
{
	length = 0;
	if (!_loaded)
		return;
	auto vec = _sessiondata->data->GetFormArray<DeltaDebugging::DeltaController>();
	if (vec.size() > 0) {
		for (int32_t i = 0; i < (int32_t)vec.size(); i++) {
			if (vec[i]->Finished() == false) {
				length++;
				if (ddvector.size() < length)
					ddvector.resize(length);
				ddvector[length - 1].SetDeltaController(vec[i]);
			}
		}
	}
}

void Session::UI_GetThreadStatus(std::vector<TaskController::ThreadStatus>& status, std::vector<const char*>& names, std::vector<std::string>& time)
{
	if (!_loaded)
		return;
	_sessiondata->_controller->GetThreadStatus(status, names, time);
}

void Session::UI_GetGenerations(std::vector<std::pair<FormID, int32_t>>& generations, size_t& size)
{
	if (!_loaded)
		return;
	_sessiondata->GetGenerationIDs(generations, size);
}

void Session::UI_GetGeneration(FormID id, UI::UIGeneration& gen)
{
	if (!_loaded)
		return;
	gen.SetGeneration(_sessiondata->GetGeneration(id));
}

void Session::UI_GetCurrentGeneration(UI::UIGeneration& gen)
{
	if (!_loaded)
		return;
	gen.SetGeneration(_sessiondata->GetCurrentGeneration());
}

void Session::UI_GetGenerationByNumber(int32_t genNumber, UI::UIGeneration& gen)
{
	if (!_loaded)
		return;
	gen.SetGeneration(_sessiondata->GetGenerationByNumber(genNumber));
}

uint64_t Session::UI_GetMemoryUsage()
{
	if (!_loaded)
		return 0;
	return _sessiondata->GetUsedMemory();
}

void Session::UI_GetInputInformation(UI::UIInputInformation& info, FormID id)
{
	if (!_loaded)
		return;
	auto input = _sessiondata->data->LookupFormID<Input>(id);
	if (input)
		info.Set(input, _sessiondata);
}

ExecHandlerStatus Session::UI_GetExecHandlerStatus()
{
	if (!_loaded)
		return ExecHandlerStatus::None;
	return _sessiondata->_exechandler->GetThreadStatus();
}

void Session::UI_GetHashmapInformation(size_t& hashmapSize)
{
	if (!_loaded)
		return;
	hashmapSize = _sessiondata->data->GetHashmapSize();
}


void Session::UI_GetTaskController(UI::UITaskController& controller)
{
	if (!_loaded)
		return;
	controller.Set(_sessiondata->_controller);
}

void Session::UI_CheckForAlternatives()
{
	if (!_loaded)
		return;
	_sessiondata->_excltree->CheckForAlternatives(4);
}

void Session::PauseSession()
{
	if (!_loaded)
		return;
	if (data->_actionloadsave)
		return;
	if (!_sessiondata->_controller->IsFrozen()) {
		_sessiondata->_controller->RequestFreeze();
		_sessiondata->_exechandler->Freeze(false);
		_sessiondata->_controller->Freeze();
	}
	_paused = true;
}

void Session::ResumeSession()
{
	if (!_loaded)
		return;
	if (data->_actionloadsave)
		return;
	if (_sessiondata->_controller->IsFrozen())
		_sessiondata->_controller->Thaw();
	if (_sessiondata->_exechandler->IsFrozen())
		_sessiondata->_exechandler->Thaw();
	_paused = false;
}

void Session::UI_GetDatabaseObjectStatus()
{
	StartProfiling;
	int64_t freedObjects = 0, fullObjects = 0;
	Data::SaveStats stats, nums, freed;
	uint64_t unfreed = 0;
	std::function<Data::VisitAction(std::shared_ptr<Form>)> report = [&freedObjects, &fullObjects, &stats, &nums, &freed, &unfreed](std::shared_ptr<Form> form) {
		if (form->Freed())
			freedObjects++;
		else
			fullObjects++;
		switch (form->GetType()) {
		case FormType::Input:
			stats._Input += form->MemorySize();
			nums._Input ++;
			if (form->Freed() == false)
				freed._Input++;
			//if (form->HasFlag(Form::FormFlags::DoNotFree)) {
			//	auto inp = dynamic_pointer_cast<Input>(form);
			//	if (inp->GetSequenceLength() > 0)
			//		unfreed++;
			//}
			break;
		case FormType::Grammar:
			stats._Grammar += form->MemorySize();
			nums._Grammar++;
			if (form->Freed() == false)
				freed._Grammar++;
			break;
		case FormType::DevTree:
			stats._DevTree += form->MemorySize();
			nums._DevTree++;
			if (form->Freed() == false)
				freed._DevTree++;
			//if (form->HasFlag(Form::FormFlags::DoNotFree)) {
			//	auto dev = dynamic_pointer_cast<DerivationTree>(form);
			//	if (dev->_nodes.size() > 0)
			//		unfreed++;
			//}
			break;
		case FormType::ExclTree:
			stats._ExclTree += form->MemorySize();
			nums._ExclTree++;
			if (form->Freed() == false)
				freed._ExclTree++;
			break;
		case FormType::Generator:
			stats._Generator += form->MemorySize();
			nums._Generator++;
			if (form->Freed() == false)
				freed._Generator++;
			break;
		case FormType::Generation:
			stats._Generation += form->MemorySize();
			nums._Generation++;
			if (form->Freed() == false)
				freed._Generation++;
			break;
		case FormType::Session:
			stats._Session += form->MemorySize();
			nums._Session++;
			if (form->Freed() == false)
				freed._Session++;
			break;
		case FormType::Settings:
			stats._Settings += form->MemorySize();
			nums._Settings++;
			if (form->Freed() == false)
				freed._Settings++;
			break;
		case FormType::Test:
			stats._Test += form->MemorySize();
			nums._Test++;
			if (form->Freed() == false)
				freed._Test++;
			break;
		case FormType::TaskController:
			stats._TaskController += form->MemorySize();
			nums._TaskController++;
			if (form->Freed() == false)
				freed._TaskController++;
			break;
		case FormType::ExecutionHandler:
			stats._ExecutionHandler += form->MemorySize();
			nums._ExecutionHandler++;
			if (form->Freed() == false)
				freed._ExecutionHandler++;
			break;
		case FormType::Oracle:
			stats._Oracle += form->MemorySize();
			nums._Oracle++;
			if (form->Freed() == false)
				freed._Oracle++;
			break;
		case FormType::SessionData:
			stats._SessionData += form->MemorySize();
			nums._SessionData++;
			if (form->Freed() == false)
				freed._SessionData++;
			break;
		case FormType::DeltaController:
			stats._DeltaController += form->MemorySize();
			nums._DeltaController++;
			if (form->Freed() == false)
				freed._DeltaController++;
			break;
		}
		stats._Fail += form->MemorySize();
		return Data::VisitAction::None;
	};
	if (!_loaded)
		return;
	data->Visit(report);
	logmessage("Database Status:");
	logmessage("Freed Objects:               {}", freedObjects);
	logmessage("Full Objects:                {}", fullObjects);
	logmessage("Size of Input:              {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._Input, freed._Input, stats._Input, (double)stats._Input * 100 / (double)stats._Fail);
	logmessage("Size of Grammar:            {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._Grammar, freed._Grammar, stats._Grammar, (double)stats._Grammar * 100 / (double)stats._Fail);
	logmessage("Size of DevTree:            {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._DevTree, freed._DevTree, stats._DevTree, (double)stats._DevTree * 100 / (double)stats._Fail);
	logmessage("Size of ExclTree:           {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._ExclTree, freed._ExclTree, stats._ExclTree, (double)stats._ExclTree * 100 / (double)stats._Fail);
	logmessage("Size of Generator:          {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._Generator, freed._Generator, stats._Generator, (double)stats._Generator * 100 / (double)stats._Fail);
	logmessage("Size of Generation:         {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._Generation, freed._Generation, stats._Generation, (double)stats._Generation * 100 / (double)stats._Fail);
	logmessage("Size of Session:            {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._Session, freed._Session, stats._Session, (double)stats._Session * 100 / (double)stats._Fail);
	logmessage("Size of Settings:           {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._Settings, freed._Settings, stats._Settings, (double)stats._Settings * 100 / (double)stats._Fail);
	logmessage("Size of Test:               {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._Test, freed._Test, stats._Test, (double)stats._Test * 100 / (double)stats._Fail);
	logmessage("Size of TaskController:     {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._TaskController, freed._TaskController, stats._TaskController, (double)stats._TaskController * 100 / (double)stats._Fail);
	logmessage("Size of ExecutionHandler:   {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._ExecutionHandler, freed._ExecutionHandler, stats._ExecutionHandler, (double)stats._ExecutionHandler * 100 / (double)stats._Fail);
	logmessage("Size of Oracle:             {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._Oracle, freed._Oracle, stats._Oracle, (double)stats._Oracle * 100 / (double)stats._Fail);
	logmessage("Size of SessionData:        {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._SessionData, freed._SessionData, stats._SessionData, (double)stats._SessionData * 100 / (double)stats._Fail);
	logmessage("Size of DeltaController:    {:>10}, {:>10}, {:>15} B, {:2.3f}%%", nums._DeltaController, freed._DeltaController, stats._DeltaController, (double)stats._DeltaController * 100 / (double)stats._Fail);
	profile(TimeProfiling, "");
}

void Session::WriteResults(std::filesystem::path resultpath, int64_t* total, int64_t* current)
{
	Evaluation eval(data->CreateForm<Session>(), resultpath);
	eval.Evaluate(*total, *current);
}
