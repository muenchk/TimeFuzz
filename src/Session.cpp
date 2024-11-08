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

void Session::LoadSession_Async(Data* dat, std::string name, int32_t number, LoadSessionArgs* args)
{
	if (number == -1)
		dat->Load(name);
	else
		dat->Load(name, number);
	auto session = dat->CreateForm<Session>();
	session->_self = session;
	session->_loaded = true;
	bool error = false;
	if (args && args->startSession == true)
		session->StartLoadedSession(error, args->reloadSettings, args->settingsPath);
	if (args)
		delete args;
}

std::shared_ptr<Session> Session::LoadSession(std::string name, LoadSessionArgs& loadargs, SessionStatus* status)
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
	dat->SetSavePath(sett->saves.savepath);
	logdebug("Set save path");
	LoadSessionArgs* asyncargs = nullptr;
	if (loadargs.startSession) {
		asyncargs = new LoadSessionArgs;
		asyncargs->startSession = loadargs.startSession;
		asyncargs->reloadSettings = loadargs.reloadSettings;
		asyncargs->settingsPath = loadargs.settingsPath;
		if (loadargs.reloadSettings)
			sett->SkipRead();
	}
	std::thread th(LoadSession_Async, dat, name, -1, asyncargs);
	th.detach();
	logdebug("Started async loader");
	return session;
}

std::shared_ptr<Session> Session::LoadSession(std::string name, int32_t number, LoadSessionArgs& loadargs, SessionStatus* status)
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
	dat->SetSavePath(sett->saves.savepath);
	logdebug("Set save path");
	LoadSessionArgs* asyncargs = nullptr;
	if (loadargs.startSession) {
		asyncargs = new LoadSessionArgs;
		asyncargs->startSession = loadargs.startSession;
		asyncargs->reloadSettings = loadargs.reloadSettings;
		asyncargs->settingsPath = loadargs.settingsPath;
		if (loadargs.reloadSettings)
			sett->SkipRead();
	}
	std::thread th(LoadSession_Async, dat, name, number, asyncargs);
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
		auto tmp = data;
		data = nullptr;
		tmp->Clear();
	}
	_lastError = 0;
	data = nullptr;
	_running = false;
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
	if (_abort == true)
		return;
	logmessage("Stopping session.");
	if (savesession)
		data->Save();

	// set abort -> session controller should automatically stop
	_abort = true;
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
	
	// don't clear any data, we may want to use the data for statistics, etc.
	
	// notify all threads waiting on the session to end, of the sessions end
	End();
}

void Session::DestroySession()
{
	// delete everything. If this isn't called the session is likely to persist until the program ends
	_loaded = false;
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
		_sessiondata->_oracle->Set(_sessiondata->_settings->oracle.oracle, _sessiondata->_settings->oracle.oraclepath);
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

void Session::StartSession(bool& error, bool globalTaskController, bool globalExecutionHandler, std::wstring settingsPath, std::function<void()> callback)
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
	data->_globalTasks = globalTaskController;
	data->_globalExec = globalExecutionHandler;
	// set taskcontroller
	_sessiondata->_controller = data->CreateForm<TaskController>();
	// set executionhandler
	_sessiondata->_exechandler = data->CreateForm<ExecutionHandler>();
	if (callback != nullptr)
		_callback = callback;
	// set save path
	data->SetSavePath(_sessiondata->_settings->saves.savepath);
	data->SetSaveName(_sessiondata->_settings->saves.savename);
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
		_sessiondata->_oracle->Set(_sessiondata->_settings->oracle.oracle, _sessiondata->_settings->oracle.oraclepath);
		// set lua cmdargs scripts
		auto path = std::filesystem::path(_sessiondata->_settings->oracle.lua_path_cmd);
		if (!std::filesystem::exists(path)) {
			logcritical("Lua CmdArgs script cannot be found.");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}
		_sessiondata->_oracle->SetLuaCmdArgs(path);
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
		_sessiondata->_grammar->ParseScala(_sessiondata->_settings->oracle.grammar_path);
		if (!_sessiondata->_grammar->IsValid()) {
			logcritical("Grammar isn't valid.");
			_lastError = ExitCodes::StartupError;
			error = true;
			return;
		}

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
	_sessiondata->_controller->Start(_sessiondata, taskthreads);
	_sessiondata->_exechandler->Init(_self, _sessiondata, _sessiondata->_settings, _sessiondata->_controller, _sessiondata->_settings->general.concurrenttests, _sessiondata->_oracle);
	_sessiondata->_exechandler->StartHandlerAsIs();
	_sessiondata->_excltree = data->CreateForm<ExclusionTree>();
	_sessiondata->_excltree->Init(_sessiondata);

	_running = true;
	data->StartClock();

	// run master control and kick stuff of
	// generates inputs, etc.
	Lua::RegisterThread(_sessiondata);
	SessionFunctions::MasterControl(_sessiondata);
	SessionFunctions::GenerateTests(_sessiondata);
	Lua::UnregisterThread();

	logmessage("Kicked session off.");
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
		
		// ExecutionHandler
		status.exec_running = _sessiondata->_exechandler->GetRunningTests();
		status.exec_waiting = _sessiondata->_exechandler->GetWaitingTests();
		status.exec_stopping = _sessiondata->_exechandler->GetStoppingTests();

		// Generation
		status.gen_generatedInputs = _sessiondata->GetGeneratedInputs();
		status.gen_generatedWithPrefix = _sessiondata->GetGeneratedPrefix();
		status.gen_generationFails = _sessiondata->GetGenerationFails();
		status.gen_failureRate = _sessiondata->GetGenerationFailureRate();
		status.gen_addtestfails = _sessiondata->GetAddTestFails();

		// Tests
		status.exitstats = _sessiondata->GetTestExitStats();
	}
	status.saveload = data->_actionloadsave;
	status.status = data->_status;
	status.record = data->_record;
	status.saveload_max = data->_actionloadsave_max;
	status.saveload_current = data->_actionloadsave_current;
	status.saveload_record_len = data->_actionrecord_len;
	status.saveload_record_current = data->_actionrecord_offset;
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
	// size0x1 is inaccurate due to code changes and cannot be reliably used
	size_t size0x1 = Form::GetDynamicSize()  // form size
	                 + 4                     // version
	                 + 8;                    // grammar id
	                 //+ _sessiondata->GetStaticSize(0x2);  // session data

	size_t size0x2 = Form::GetDynamicSize()  // form size
	                 + 4;                    // version

	switch (version) {
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	default:
		return 0;
	}
}

size_t Session::GetDynamicSize()
{
	return GetStaticSize(classversion);
}

bool Session::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	return true;
}

bool Session::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			FormID _ = Buffer::ReadUInt64(buffer, offset);
			_sessiondata = data->CreateForm<SessionData>();
			_sessiondata->data = data;
			_sessiondata->ReadData(buffer, offset, length, resolver);
			return true;
		}
	case 0x2:
		{
			Form::ReadData(buffer, offset, length, resolver);
			return true;
		}
	default:
		return false;
	}
}


void Session::SetInternals(std::shared_ptr<Oracle> oracle, std::shared_ptr<TaskController> controller, std::shared_ptr<ExecutionHandler> exechandler, std::shared_ptr<Generator> generator, std::shared_ptr<Grammar> grammar, std::shared_ptr<Settings> settings, std::shared_ptr<ExclusionTree> excltree)
{
	_sessiondata->_oracle = oracle;
	_sessiondata->_controller = controller;
	_sessiondata->_exechandler = exechandler;
	_sessiondata->_generator = generator;
	_sessiondata->_grammar = grammar;
	_sessiondata->_settings = settings;
	_sessiondata->_excltree = excltree;
}

void Session::Delete(Data*)
{
	Clear();
}


void Session::Replay(FormID inputid)
{
	auto input = data->LookupFormID<Input>(inputid);
	if (input) {
		bool reg = Lua::RegisterThread(_sessiondata);
		auto newinput = data->CreateForm<Input>();
		input->DeepCopy(newinput);
		auto callback = dynamic_pointer_cast<Functions::ReplayTestCallback>(Functions::ReplayTestCallback::Create());
		callback->_sessiondata = _sessiondata;
		callback->_input = newinput;
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
	auto vec = _sessiondata->GetTopK((int32_t)k);
	for (int32_t i = 0; i < (int32_t)vec.size(); i++)
	{
		vector[i].id = vec[i]->GetFormID();
		vector[i].length = vec[i]->Length();
		vector[i].primaryScore = vec[i]->GetPrimaryScore();
		vector[i].secondaryScore = vec[i]->GetSecondaryScore();
		vector[i].result = (UI::Result)vec[i]->GetOracleResult();
	}
}

UI::UIDeltaDebugging Session::UI_StartDeltaDebugging()
{
	if (!_loaded)
		return {};
	bool reg = Lua::RegisterThread(_sessiondata);
	auto vec = _sessiondata->GetTopK_Unfinished((int32_t)1);
	UI::UIDeltaDebugging dd;
	
	if (vec.size() > 0)
	{
		dd.SetDeltaController(SessionFunctions::BeginDeltaDebugging(_sessiondata, vec[0]));
	}
	if (reg)
		Lua::UnregisterThread();
	return dd;
}


UI::UIDeltaDebugging Session::UI_FindDeltaDebugging()
{
	if (!_loaded)
		return {};
	auto vec = _sessiondata->data->GetFormArray<DeltaDebugging::DeltaController>();
	if (vec.size() > 0) {
		UI::UIDeltaDebugging dd;
		dd.SetDeltaController(vec[0]);
		return dd;
	} else
		return {};
}
