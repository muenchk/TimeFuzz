#pragma once

#include "Form.h"

#include <chrono>

namespace DeltaDebugging
{
	class DeltaController;
}

class Generation;
class Input;
class SessionData;
class TaskController;

typedef uint64_t EnumType;

namespace UI
{
	enum class Result : EnumType
	{
		/// <summary>
		/// Program has not finished
		/// </summary>
		Unfinished = 0b0,
		/// <summary>
		/// The result is passing
		/// </summary>
		Passing = 0b1,
		/// <summary>
		/// The result is failing
		/// </summary>
		Failing = 0b10,
		/// <summary>
		/// Input is still waiting to be run
		/// </summary>
		Running = 0x4000000000000000,
	};

	class UIInput
	{
	public:
		enum ColumnID
		{
			InputID,
			InputLength,
			InputPrimaryScore,
			InputSecondaryScore,
			InputResult,
			InputFlags,
			InputGenerationNum,
			InputDerivedNum,
			InputAction,
		};

		FormID id;
		size_t length;
		double primaryScore;
		double secondaryScore;
		EnumType flags;
		int32_t generationNumber; 
		int64_t derivedInputs;
		UI::Result result;
	};

	class UIExecTime
	{
	public:
		enum ColumnID
		{
			ExecTimeFunction,
			ExecTimeFile,
			ExecTimeNano,
			ExecTimeExecutions,
			ExecTimeAverage,
			ExecTimeLast,
			ExecTimeUserMes,
		};
		std::string func = "";
		std::string file = "";
		std::string usermes = "";
		std::chrono::nanoseconds ns;
		uint64_t executions;
		std::chrono::nanoseconds average;
		std::chrono::steady_clock::time_point time;
		int32_t tmpid;
	};

	class UIDDResult
	{
	public:
		enum ColumnID
		{
			InputID,
			InputLength,
			InputPrimaryScore,
			InputSecondaryScore,
			InputResult,
			InputFlags,
			InputAction,
			InputLossPrimary,
			InputLossSecondary,
			InputLevel,
		};

		FormID id;
		size_t length;
		double primaryScore;
		double secondaryScore;
		UI::Result result;
		double primaryLoss;
		double secondaryLoss;
		int32_t level;
		EnumType flags;
	};

	class UIDeltaDebugging
	{
	public:

		FormID GetFormID();
		std::string GetGoal();
		std::string GetMode();
		int32_t GetTestsRemaining();
		int32_t GetTests();
		int32_t GetTotalTests();
		int32_t GetLevel();
		int32_t GetSkippedTests();
		int32_t GetPrefixTests();
		int32_t GetInvalidTests();
		bool Finished();
		void GetResults(std::vector<UIDDResult>& results, size_t& size);
		void GetOriginalInput(UIInput& input);
		void GetInput(UIInput& input);
		void GetActiveInputs(std::vector<UIInput>& inputs, size_t& size);
		void SetDeltaController(std::shared_ptr<DeltaDebugging::DeltaController> controller);
		bool Initialized();

	private:
		std::shared_ptr<DeltaDebugging::DeltaController> _ddcontroller;
	};

	class UIGeneration
	{
	public:
		FormID GetFormID();
		int64_t GetSize();
		int64_t GetGeneratedSize();
		int64_t GetDDSize();
		int64_t GetTargetSize();
		int64_t GetActiveInputs();
		int64_t GetNumberOfSources();
		void GetSources(std::vector<UIInput>& inputs);
		int64_t GetGenerationNumber();
		void GetDDControllers(std::vector<UIDeltaDebugging>& dd, size_t& size);

		void SetGeneration(std::shared_ptr<Generation> generation);

		bool Initialized();
		void Reset();
	private:
		std::shared_ptr<Generation> _generation;
		std::vector<std::shared_ptr<Input>> sources;
		std::vector<std::shared_ptr<DeltaDebugging::DeltaController>> _ddcontrollers;
		bool hasbegun = false;
		size_t lastddcontrollers = 0;
	};

	class UIInputInformation
	{
	public:
		FormID _inputID;
		std::string _inputconcat;
		char* _inputconcatBuf;
		std::string _inputlist;
		char* _inputlistBuf;
		FormID _generationID;
		uint64_t _derivedInputs;
		std::chrono::nanoseconds _generationTime;
		std::chrono::nanoseconds _executionTime;
		double _primaryScore;
		double _secondaryScore;
		bool _hasfinished;
		bool _trimmed;
		int64_t _naturallength;
		int64_t _trimmedlength;
		int32_t _exitcode;
		EnumType _exitreason;
		FormID _parentInput;
		EnumType _flags;
		std::string _cmdArgs;
		char* _cmdArgsBuf;
		std::string _scriptArgs;
		char* _scriptArgsBuf;
		EnumType _oracleResult;
		std::string _output;

		bool Initialized();
		void Refresh();
		void Set(std::shared_ptr<Input> input, std::shared_ptr<SessionData> sessiondata);
		void Reset();

	private:
		std::shared_ptr<Input> _input;
		std::shared_ptr<SessionData> _sessiondata;
		bool _init = false;
	};

	class UITaskController
	{
	public:
		void Set(std::shared_ptr<TaskController> controller);
		bool Initialized();

		std::unordered_map<std::string, int64_t>::iterator beginExecutedTasks();
		std::unordered_map<std::string, int64_t>::iterator endExecutedTasks();
		void LockExecutedTasks();
		void UnlockExecutedTasks();
	private:
		std::shared_ptr<TaskController> _controller;

	};
}
