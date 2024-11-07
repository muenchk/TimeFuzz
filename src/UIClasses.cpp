#include "UIClasses.h"
#include "DeltaDebugging.h"

#include <vector>

using namespace DeltaDebugging;
using namespace UI;

bool UIDeltaDebugging::Initialized()
{
	return _ddcontroller ? true : false;
}

std::string UIDeltaDebugging::GetGoal()
{
	switch (_ddcontroller->GetGoal())
	{
	case DDGoal::None:
		return "None";
	case DDGoal::ReproduceResult:
		return "Reproduce Result";
	case DDGoal::MaximizePrimaryScore:
		return "Maximize Primary Score";
	case DDGoal::MaximizeSecondaryScore:
		return "Maximize Secondary Score";
	}
	return "Unknown";
}

std::string UIDeltaDebugging::GetMode()
{
	switch (_ddcontroller->GetMode())
	{
	case DDMode::Standard:
		return "Standard";
	}
	return "Unknown";
}

int32_t UIDeltaDebugging::GetTestsRemaining()
{
	return _ddcontroller->GetTestsRemaining();
}
int32_t UIDeltaDebugging::GetTests()
{
	return _ddcontroller->GetTests();
}

int32_t UIDeltaDebugging::GetTotalTests()
{
	return _ddcontroller->GetTestsTotal();
}

int32_t UIDeltaDebugging::GetLevel()
{
	return _ddcontroller->GetLevel();
}

bool UIDeltaDebugging::Finished()
{
	return _ddcontroller->Finished();
}

void UIDeltaDebugging::GetResults(std::vector<UIDDResult>& results, size_t& size)
{
	auto res = _ddcontroller->GetResults();
	size_t sz = res->size();
	size_t count = 0;
	if (sz > results.size())
		results.resize(sz);
	auto itr = res->begin();
	while (itr != res->end() && count < sz)
	{
		auto input = itr->first;
		auto [loss, level] = itr->second;
		results[count].id = input->GetFormID();
		results[count].length = input->Length();
		results[count].primaryScore = input->GetPrimaryScore();
		results[count].secondaryScore = input->GetSecondaryScore();
		results[count].result = (UI::Result)input->GetOracleResult();
		results[count].loss = loss;
		results[count].level = level;
		itr++;
		count++;
	}
	size = count;
}

void UIDeltaDebugging::GetOriginalInput(UIInput& input)
{
	auto inp = _ddcontroller->GetOriginalInput();
	input.id = inp->GetFormID();
	input.length = inp->Length();
	input.primaryScore = inp->GetPrimaryScore();
	input.secondaryScore = inp->GetSecondaryScore();
	input.result = (UI::Result)inp->GetOracleResult();
}

void UIDeltaDebugging::GetInput(UIInput& input)
{
	auto inp = _ddcontroller->GetInput();
	input.id = inp->GetFormID();
	input.length = inp->Length();
	input.primaryScore = inp->GetPrimaryScore();
	input.secondaryScore = inp->GetSecondaryScore();
	input.result = (UI::Result)inp->GetOracleResult();
}

void UIDeltaDebugging::GetActiveInputs(std::vector<UIInput>& inputs, size_t& size)
{
	auto act = _ddcontroller->GetActiveInputs();
	size_t sz = act->size();
	size_t count = 0;
	if (sz > inputs.size())
		inputs.resize(sz);
	auto itr = act->begin();
	while (itr != act->end() && count < sz) {
		auto input = *itr;
		inputs[count].id = input->GetFormID();
		inputs[count].length = input->Length();
		inputs[count].primaryScore = input->GetPrimaryScore();
		inputs[count].secondaryScore = input->GetSecondaryScore();
		inputs[count].result = (UI::Result)input->GetOracleResult();
		itr++;
		count++;
	}
	size = count;
}

void UIDeltaDebugging::SetDeltaController(std::shared_ptr<DeltaController> controller)
{
	_ddcontroller = controller;
}
