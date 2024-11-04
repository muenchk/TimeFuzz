#pragma once

#include "Form.h"

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
}
