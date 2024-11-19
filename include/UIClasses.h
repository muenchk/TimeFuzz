#pragma once

#include "Form.h"

#include <chrono>

namespace DeltaDebugging
{
	class DeltaController;
}

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
			InputAction,
		};

		FormID id;
		size_t length;
		double primaryScore;
		double secondaryScore;
		EnumType flags;
		int32_t generationNumber; 
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
			ExecTimeLast,
			ExecTimeUserMes,
		};
		std::string func = "";
		std::string file = "";
		std::string usermes = "";
		std::chrono::nanoseconds ns;
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
			InputLoss,
			InputLevel,
		};

		FormID id;
		size_t length;
		double primaryScore;
		double secondaryScore;
		UI::Result result;
		double loss;
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
}
