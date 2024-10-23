#pragma once

#include <functional>
#include <semaphore>

#include "Generator.h"
#include "Grammar.h"
#include "Oracle.h"
#include "Settings.h"
#include "SessionData.h"
#include "TaskController.h"
#include "Form.h"

class Data;
class SessionFunctions;
class SessionStatistics;
class ExclusionTree;
class Records;
class Lua;

class Session : public Form
{
public:
	/// <summary>
	/// Returns a singleton the the session
	/// </summary>
	/// <returns></returns>
	Session* GetSingleton();

	Session();

	static std::shared_ptr<Session> CreateSession();

	static std::shared_ptr<Session> LoadSession(std::string name, std::wstring settingsPath = L"");
	static std::shared_ptr<Session> LoadSession(std::string name, int32_t number, std::wstring settingsPath = L"");

	~Session();

	/// <summary>
	/// Starts the fuzzing session
	/// </summary>
	/// <param name="error"></param>
	/// <param name="globalTaskController"></param>
	/// <param name="globalExecutionHandler"></param>
	void StartSession(bool& error, bool globalTaskController = false, bool globalExecutionHandler = false, std::wstring settingsPath = L"", std::function<void()> callback = nullptr);

	void StartLoadedSession(bool& error, bool reloadsettings = false, std::wstring settingsPath = L"", std::function<void()> callback = nullptr);

	/// <summary>
	/// Waits for the session to end
	/// </summary>
	void Wait();

	/// <summary>
	/// Waits for the session to end or until the timeut expires
	/// </summary>
	/// <param name="waitfor"></param>
	/// <returns></returns>
	bool Wait(std::chrono::nanoseconds timeout);

	/// <summary>
	/// Starts generation of [negatives] negative inputs, while stopping after [maxiterations] or when [timeout] is exceeded,
	/// even if the number of generated negatives has not reached [negatives]
	/// </summary>
	/// <param name="negatives">number of negatives to generate</param>
	/// <param name="maxiterations">maximum number of iterations to run [set to 0 to disable]</param>
	/// <param name="timeout">maximum amount of time in seconds [set to 0 to disable]</param>
	/// <param name="returnpositives">returns the generated positives as well as the negatives</param>
	std::vector<std::shared_ptr<Input>> GenerateNegatives(int32_t negatives, bool& error, int32_t maxiterations = 0, int32_t timeout = 0, bool returnpositives = false);

	/// <summary>
	/// stops the ongoing session.
	/// </summary>
	/// <param name="savesession"></param>
	void StopSession(bool savesession = true);

	/// <summary>
	/// Deletes session. [MUST BE CALLED]
	/// </summary>
	void DestroySession();

	/// <summary>
	/// Saves the current session status
	/// </summary>
	void Save();

	/// <summary>
	/// Returns true when the session has finished
	/// </summary>
	/// <returns></returns>
	bool Finished() { return _hasFinished; }

	/// <summary>
	/// Returns a string with information about the session
	/// </summary>
	/// <returns></returns>
	std::string PrintStats();

	Data* data = nullptr;

private:

	/// <summary>
	/// Function that is called after the session has finished running, 
	/// alleviates the need to check whether the session has finished
	/// </summary>
	std::function<void()> _callback;

	/// <summary>
	/// The Task used to execute the PUT and retrieve the oracle result
	/// </summary>
	std::shared_ptr<Oracle> _oracle;

	/// <summary>
	/// Task controller for the active session
	/// </summary>
	std::shared_ptr<TaskController> _controller;

	/// <summary>
	/// Executionhandler for the active session
	/// </summary>
	std::shared_ptr<ExecutionHandler> _exechandler;

	/// <summary>
	/// Generator for fuzzing
	/// </summary>
	std::shared_ptr<Generator> _generator;

	/// <summary>
	/// The grammar for the generation
	/// </summary>
	std::shared_ptr<Grammar> _grammar;

	/// <summary>
	/// the settings for this session
	/// </summary>
	std::shared_ptr<Settings> _settings;

	/// <summary>
	/// ExclusionTree of the session
	/// </summary>
	std::shared_ptr<ExclusionTree> _excltree;

	/// <summary>
	/// Self
	/// </summary>
	std::shared_ptr<Session> _self;

	/// <summary>
	/// the runtime data of the session
	/// </summary>
	SessionData sessiondata;

	/// <summary>
	/// the controller for the session
	/// </summary>
	std::thread _sessioncontroller;

	/// <summary>
	/// The last error encountered in the session
	/// </summary>
	uint64_t LastError = 0;

	inline uint64_t GetLastError() { return LastError; }

	/// <summary>
	/// whether to abort the current session
	/// </summary>
	bool abort = false;

	/// <summary>
	/// whether a session is currently running
	/// </summary>
	bool running = false;
	std::mutex runninglock;

	/// <summary>
	/// returns whether the session is running
	/// </summary>
	/// <returns></returns>
	bool IsRunning();

	/// <summary>
	/// attempts to set the current state of the session
	/// </summary>
	/// <param name="state">the new state</param>
	/// <returns>Retruns [true] if the state was set, [false] if the state is already set</returns>
	bool SetRunning(bool state);

	/// <summary>
	/// controls the session
	/// </summary>
	void SessionControl();

	/// <summary>
	/// Starts the session
	/// </summary>
	void StartSessionIntern(bool &error);

	/// <summary>
	/// Completes cleanup and ends session
	/// </summary>
	void End();

	

	/// <summary>
	/// class version
	/// </summary>
	const int32_t classversion = 0x1;

	/// <summary>
	/// Returns the size of the classes static members
	/// </summary>
	/// <param name="version"></param>
	/// <returns></returns>
	size_t GetStaticSize(int32_t version = 0x1) override;
	/// <summary>
	/// Returns the overall size of all members to be saved
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize() override;
	/// <summary>
	/// Writes all class data to the buffer at the given offset
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	/// <summary>
	/// Reads the class data from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="length"></param>
	/// <param name="resolver"></param>
	/// <returns></returns>
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;

	void SetInternals(std::shared_ptr<Oracle> oracle, std::shared_ptr<TaskController> controller, std::shared_ptr<ExecutionHandler> exechandler, std::shared_ptr<Generator> generator, std::shared_ptr<Grammar> grammar, std::shared_ptr<Settings> settings, std::shared_ptr<ExclusionTree> excltree);
	/// <summary>
	/// Returns the class type
	/// </summary>
	/// <returns></returns>
	static int32_t GetTypeStatic()
	{
		return FormType::Session;
	}
	/// <summary>
	/// Returns the class type
	/// </summary>
	/// <returns></returns>
	int32_t GetType() override
	{
		return FormType::Session;
	}
	/// <summary>
	/// Deletes the class data
	/// </summary>
	/// <param name=""></param>
	void Delete(Data*) override;

	/// <summary>
	/// clears all internal data
	/// </summary>
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();

	friend class SessionFunctions;
	friend class Data;
	friend class Records;
	friend class SessionStatistics;
	friend class Lua;


	/// <summary>
	/// whether the session has finished
	/// </summary>
	std::atomic_bool _hasFinished = false;
	/// <summary>
	/// mutex for waiting on the session
	/// </summary>
	std::mutex _waitSessionLock;
	/// <summary>
	/// conditional variable that makes waiting for session end possible
	/// </summary>
	std::condition_variable _waitSessionCond;
};
