#pragma once

#include "SessionData.h"
#include "Input.h"
#include "Generation.h"
#include "Oracle.h"
#include "Settings.h"

class FilterFunctions
{
public:
	template <class T>
	static void FilterSet(std::shared_ptr<SessionData> _sessiondata, std::shared_ptr<Generation> generation, std::set<std::shared_ptr<Input>, FormIDLess<Input>>& targets, double max, double frac, bool checkdeltaflag, bool primary, int targetval)
	{
		std::set<std::shared_ptr<Input>, T> inputs;
		// if the max is 0 just run loop a single time since 0 is the minimal value for primaryScore
		if (max == 0.0f)
			frac = 1.0f;
		while ((int32_t)targets.size() < targetval && frac <= 1.0) {
			loginfo("Iteration with fractal: {}, and maxScore: {}", frac, max);
			inputs.clear();
			// we don't filter for secondary score at this point
			if (primary)
				generation->GetAllInputs<T>(inputs, true, (max - (max * frac)), -1.f, _sessiondata->_settings->generation.allowBacktrackFailedInputs, 1, 2 /*cannot backtrack on length 1*/);
			else
				generation->GetAllInputs<T>(inputs, true, -1.f, (max - (max * frac)), _sessiondata->_settings->generation.allowBacktrackFailedInputs, 1, 2 /*cannot backtrack on length 1*/);
			auto itr = inputs.begin();
			while (itr != inputs.end() && (int32_t)targets.size() < targetval) {
				if (targets.contains(*itr) == false && (_sessiondata->_settings->generation.maxNumberOfFailsPerSource == 0 || _sessiondata->_settings->generation.maxNumberOfFailsPerSource > (*itr)->GetDerivedFails()) && (_sessiondata->_settings->generation.maxNumberOfGenerationsPerSource == 0 || _sessiondata->_settings->generation.maxNumberOfGenerationsPerSource > (*itr)->GetDerivedInputs())) {
					// check that the length of the source is longer than min backtrack, such that it can be extended in the first place
					if (((*itr)->GetOracleResult() == OracleResult::Unfinished &&
								(*itr)->Length() > _sessiondata->_settings->methods.IterativeConstruction_Extension_Backtrack_min ||
							(*itr)->GetOracleResult() == OracleResult::Failing &&
								(int32_t)(*itr)->Length() > _sessiondata->_settings->methods.IterativeConstruction_Backtrack_Backtrack_min) &&
						(checkdeltaflag == false || (*itr)->HasFlag(Input::Flags::DeltaDebugged) == false)) {
						targets.insert(*itr);
						loginfo("Found New Source: {}", Input::PrintForm(*itr));
					}
				}
				itr++;
			}
			frac += _sessiondata->_settings->dd.optimizationLossThreshold;
		}
	}
};
