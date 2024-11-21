#include "UIClasses.h"
#include "DeltaDebugging.h"
#include "Generation.h"

#include <vector>

using namespace DeltaDebugging;
using namespace UI;

FormID UIDeltaDebugging::GetFormID()
{
	if (_ddcontroller)
		return _ddcontroller->GetFormID();
	else
		return 0;
}

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
	if (_ddcontroller->TryLockRead()) {
		auto res = _ddcontroller->GetResults();
		size_t sz = res->size();
		size_t count = 0;
		if (sz > results.size())
			results.resize(sz);
		auto itr = res->begin();
		while (itr != res->end() && count < sz) {
			auto input = itr->first;
			auto [loss, level] = itr->second;
			results[count].id = input->GetFormID();
			results[count].length = input->Length();
			results[count].primaryScore = input->GetPrimaryScore();
			results[count].secondaryScore = input->GetSecondaryScore();
			results[count].flags = input->GetFlags();
			results[count].result = (UI::Result)input->GetOracleResult();
			results[count].loss = loss;
			results[count].level = level;
			itr++;
			count++;
		}
		_ddcontroller->UnlockRead();
		size = count;
	}
}

void UIDeltaDebugging::GetOriginalInput(UIInput& input)
{
	auto inp = _ddcontroller->GetOriginalInput();
	input.id = inp->GetFormID();
	input.length = inp->Length();
	input.primaryScore = inp->GetPrimaryScore();
	input.secondaryScore = inp->GetSecondaryScore();
	input.result = (UI::Result)inp->GetOracleResult();
	input.flags = inp->GetFlags();
}

void UIDeltaDebugging::GetInput(UIInput& input)
{
	auto inp = _ddcontroller->GetInput();
	input.id = inp->GetFormID();
	input.length = inp->Length();
	input.primaryScore = inp->GetPrimaryScore();
	input.secondaryScore = inp->GetSecondaryScore();
	input.result = (UI::Result)inp->GetOracleResult();
	input.flags = inp->GetFlags();
}

void UIDeltaDebugging::GetActiveInputs(std::vector<UIInput>& inputs, size_t& size)
{
	if (_ddcontroller->TryLockRead()) {
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
			inputs[count].flags = input->GetFlags();
			itr++;
			count++;
		}
		_ddcontroller->UnlockRead();
		size = count;
	}
}

void UIDeltaDebugging::SetDeltaController(std::shared_ptr<DeltaController> controller)
{
	_ddcontroller = controller;
}

FormID UIGeneration::GetFormID()
{
	return _generation->GetFormID();
}

int64_t UIGeneration::GetSize()
{
	return _generation->GetSize();
}

int64_t UIGeneration::GetGeneratedSize()
{
	return _generation->GetGeneratedSize();
}

int64_t UIGeneration::GetDDSize()
{
	return _generation->GetDDSize();
}


int64_t UIGeneration::GetTargetSize() 
{
	return _generation->GetTargetSize();
}
int64_t UIGeneration::GetActiveInputs() 
{
	int64_t active = _generation->GetActiveInputs();
	// sources are added before a generation actually begins
	if (!hasbegun && active > 0)
		hasbegun = true;
	return active;
}
int64_t UIGeneration::GetNumberOfSources()
{
	return _generation->GetNumberOfSources();
}
void UIGeneration::GetSources(std::vector<UIInput>& inputs)
{ 
	if (!hasbegun)
	{
		_generation->GetSources(sources);
		if (inputs.size() < sources.size())
			inputs.resize(sources.size());
		for (size_t i = 0; i < sources.size(); i++) {
			inputs[i].id = sources[i]->GetFormID();
			inputs[i].length = sources[i]->Length();
			inputs[i].primaryScore = sources[i]->GetPrimaryScore();
			inputs[i].secondaryScore = sources[i]->GetSecondaryScore();
			inputs[i].result = (UI::Result)sources[i]->GetOracleResult();
			inputs[i].flags = sources[i]->GetFlags();
		}
	}
}
int64_t UIGeneration::GetGenerationNumber() 
{
	return _generation->GetGenerationNumber();
}
void UIGeneration::GetDDControllers(std::vector<UIDeltaDebugging>& dd, size_t& size) 
{
	if (lastddcontrollers == _generation->GetNumberOfDDControllers())
		return;
	_generation->GetDDControllers(_ddcontrollers);
	lastddcontrollers = _generation->GetNumberOfDDControllers();
	size = lastddcontrollers;
	if (dd.size() < _ddcontrollers.size())
		dd.resize(_ddcontrollers.size());
	for (size_t i = 0; i < _ddcontrollers.size(); i++)
	{
		dd[i].SetDeltaController(_ddcontrollers[i]);
	}
}

void UIGeneration::SetGeneration(std::shared_ptr<Generation> generation) 
{
	_generation = generation;
}

bool UIGeneration::Initialized()
{
	return _generation.operator bool();
}
