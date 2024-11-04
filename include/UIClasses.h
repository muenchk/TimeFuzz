#pragma once

#include "Form.h"

#include <chrono>

namespace UI
{
	enum class Result
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
			InputAction,
		};

		FormID id;
		size_t length;
		double primaryScore;
		double secondaryScore;
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
}
