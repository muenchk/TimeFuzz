#pragma once

#include "SessionData.h"
#include "Input.h"
#include "Generation.h"
#include "Oracle.h"
#include "Settings.h"
#include "DeltaDebugging.h"
#include "Data.h"

#include <set>

class FilterFunctions
{
public:
	template <class T>
	static void FilterSet(std::shared_ptr<SessionData> _sessiondata, std::shared_ptr<Generation> generation, std::set<std::shared_ptr<Input>, FormIDLess<Input>>& targets, double max, double frac, bool checkdeltaflag, bool primary, int targetval)
	{
		std::vector<std::shared_ptr<DeltaDebugging::DeltaController>> controllers;
		generation->GetDDControllers(controllers);
		auto itrus = controllers.begin();
		// remove controllers without results
		while (itrus != controllers.end()) {
			if ((*itrus)->GetResults()->size() > 0) {
				itrus++;
			}
			else
				itrus = controllers.erase(itrus);
		}
		std::vector<int32_t> rootNums;
		std::vector<int32_t> boundaries;
		std::vector<std::vector<std::shared_ptr<Input>>> chosen;
		chosen.resize(controllers.size());
		rootNums.resize(controllers.size());
		boundaries.resize(controllers.size());
		for (size_t i = 0; i < controllers.size(); i++) {
			rootNums[i] = 0;
			boundaries[i] = 0;
		}
		int32_t idx = 0;
		// distribute budget to controllers equally
		std::set<size_t> fixed;
		for (int32_t i = 0; i < targetval; i++) {
			if (fixed.contains(idx)) {
				idx++;
			} else if (idx < (int32_t)controllers.size()) {
				if (boundaries[idx] >= (int32_t)controllers[idx]->GetTestsTotal()) {
					// if the boundary exceeds the number of inputs available for this root, fix the upper ceiling
					fixed.insert(idx);
					idx++;
				} else {
					boundaries[idx]++;
					idx++;
				}
			}
			if (idx == (int32_t)controllers.size())
				idx = 0;
			if (fixed.size() == controllers.size()) // if all roots are already fixed stop calculation boundaries #infiniteloops
				break;
		}

		auto decBudget = [&idx, &boundaries, &chosen, &rootNums, &targets]() {
			if (boundaries.size() > 0) {
				boundaries[idx]--;
				// if now a root has exceeded the new budget, remove one of their inputs from the targets
				if (rootNums[idx] > boundaries[idx]) {
					targets.erase(chosen[idx].back());
					chosen[idx].pop_back();
				}
				idx--;
				if (idx == -1)
					idx = (int32_t)boundaries.size() - 1;
			}
		};

		auto getRoot = [&controllers, &_sessiondata](std::shared_ptr<Input> input) {
			auto traceBack = [&_sessiondata](std::shared_ptr<Input> input, FormID origID) {
				auto tmp = input;
				while (tmp && tmp->GetParentID() != 0)
					if (tmp->GetParentID() == origID)
						return true;
					else
						tmp = _sessiondata->data->LookupFormID<Input>(tmp->GetParentID());
				return false;
			};
			for (size_t i = 0; i < controllers.size(); i++) {
				if (controllers[i]->GetResults()->contains(input))
					return (int32_t)i;
				if (controllers[i]->GetOriginalInput()->GetFormID() == input->GetFormID())
					return (int32_t)i;
				if (traceBack(input, controllers[i]->GetOriginalInput()->GetFormID()))
					return (int32_t)i;
			}
			return -1;
		};

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
						// check if the input has a dd source and whether the max alloted number has been reached
						auto index = getRoot(*itr);
						if (index == -1) {
							targets.insert(*itr);
							decBudget();
							loginfo("Found New Source: {}", Input::PrintForm(*itr));
						} else {
							if (rootNums[index] < boundaries[index])
							{
								targets.insert(*itr);
								chosen[index].push_back(*itr);
								rootNums[index]++;
								loginfo("Found New Source: {}", Input::PrintForm(*itr));
							}
						}
					}
				}
				itr++;
			}
			frac += _sessiondata->_settings->dd.optimizationLossThreshold;
		}
	}
};
