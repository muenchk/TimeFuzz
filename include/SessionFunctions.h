#pragma once

/* This class is a bit special in its purpose and the reason why it exists in the first place.
* Due to the basic architecture of this project, functions aren't executed directly, but due to callbacks.
* This include almost all top-level functions including session management, data management, etc.
* Thus a lot of callbacks will need to be able to call certain top-level functions for regular management,
* which enables the need for universally available functions that have access to all internal data,
* but do not expose any directly functionality to the outside through the session class.
* 
* This class is a friend of session and only features static function, so no internal state exists
* or needs to be saved. All top-level functions related to session management (saving, data consolidation,
* etc.), generation, end conditions are collected here.
* */

#include <memory>
#include <exception>
#include <stdexcept>
#include <chrono>

#include "Function.h"

// external class definitions
class Session;
class Input;
class SessionData;

namespace Functions
{
	class MasterGenerationCallback : public BaseFunction
	{
	public:
		std::shared_ptr<SessionData> _sessiondata;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'MGEN'; }
		uint64_t GetType() override { return 'MGEN'; }

		FunctionType GetFunctionType() override { return FunctionType::Heavy; }

		bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);
		bool WriteData(unsigned char* buffer, size_t& offset);

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<MasterGenerationCallback>()); }
		void Dispose();
		size_t GetLength();
	};
}

class SessionFunctions
{
public:
	/// <summary>
	/// This function generates a new input and adds it to the execution queue
	/// </summary>
	static std::shared_ptr<Input> GenerateInput(std::shared_ptr<SessionData>& sessiondata);

	/// <summary>
	/// This function generates a new input and adds it to the execution queue
	/// </summary>
	static void GenerateInput(std::shared_ptr<Input>& input, std::shared_ptr<SessionData>& sessiondata);

	/// <summary>
	/// This function checks whether we need to save the session and performs the save
	/// </summary>
	static void SaveCheck(std::shared_ptr<SessionData>& sessiondata);

	/// <summary>
	/// This function checks whether we have achieved our goals and initiates the end of the session
	/// </summary>
	/// <returns>Wether session is being ended</returns>
	static bool EndCheck(std::shared_ptr<SessionData>& sessiondata);

	/// <summary>
	/// Attempts to free up as much memory as possible
	/// </summary>
	/// <param name="sessiondata"></param>
	static void ReclaimMemory(std::shared_ptr<SessionData>& sessiondata);

	/// <summary>
	/// Master function that checks for necessary management actions and executes them
	/// </summary>
	/// <param name="session"></param>
	static void MasterControl(std::shared_ptr<SessionData>& sessiondata, bool forceexecute = false);

	/// <summary>
	/// This function performs data movement and processing associated with the end of a testcase
	/// </summary>
	/// <param name="session"></param>
	/// <param name="input"></param>
	/// <param name="test"></param>
	static void TestEnd(std::shared_ptr<SessionData>& sessiondata, std::shared_ptr<Input> input, bool replay = false);

	static void AddTestExitReason(std::shared_ptr<SessionData>& sessiondata, int32_t reason);

	static void GenerateTests(std::shared_ptr<SessionData>& sessiondata);

private:
	/// <summary>
	/// Async Function that will end the session
	/// </summary>
	static void EndSession_Async(std::shared_ptr<SessionData> sessiondata);

	/// <summary>
	/// Async Function that will save the session, since we are likely operating from inside the taskcontroller
	/// </summary>
	static void SaveSession_Async(std::shared_ptr<SessionData> sessiondata);
};

class SessionStatistics
{
public:
	static uint64_t TestsExecuted(std::shared_ptr<SessionData>&);
	static uint64_t PositiveTestsGenerated(std::shared_ptr<SessionData>&);
	static uint64_t NegativeTestsGenerated(std::shared_ptr<SessionData>&);
	static uint64_t UnfinishedTestsGenerated(std::shared_ptr<SessionData>&);
	static uint64_t TestsPruned(std::shared_ptr<SessionData>&);
	static std::chrono::nanoseconds Runtime(std::shared_ptr<SessionData>& sessiondata);
};
