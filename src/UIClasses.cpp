#include "UIClasses.h"
#include "DeltaDebugging.h"
#include "Generation.h"
#include "Input.h"
#include "Test.h"
#include "LuaEngine.h"
#include "Oracle.h"
#include "SessionData.h"
#include "SessionFunctions.h"

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
	case DDGoal::MaximizeBothScores:
		return "Maximize Both Scores";
	}
	return "Unknown";
}

std::string UIDeltaDebugging::GetMode()
{
	switch (_ddcontroller->GetMode())
	{
	case DDMode::Standard:
		return "Standard";
	case DDMode::ScoreProgress:
		return "ScoreProgress";
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


int32_t UIDeltaDebugging::GetSkippedTests()
{
	return _ddcontroller->GetSkippedTests();
}

int32_t UIDeltaDebugging::GetPrefixTests()
{
	return _ddcontroller->GetPrefixTests();
}

int32_t UIDeltaDebugging::GetInvalidTests()
{
	return _ddcontroller->GetInvalidTests();
}

uint64_t UIDeltaDebugging::GetBatchIdent()
{
	return _ddcontroller->GetBatchIdent();
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
			auto [primloss, seconloss, level] = itr->second;
			results[count].id = input->GetFormID();
			results[count].length = input->Length();
			results[count].primaryScore = input->GetPrimaryScore();
			results[count].secondaryScore = input->GetSecondaryScore();
			results[count].flags = input->GetFlags();
			results[count].result = (UI::Result)input->GetOracleResult();
			results[count].primaryLoss = primloss;
			results[count].secondaryLoss = seconloss;
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
	if (!_ddcontroller->IsDeleted()) {
		auto inp = _ddcontroller->GetOriginalInput();
		if (inp) {
			input.id = inp->GetFormID();
			input.length = inp->Length();
			input.primaryScore = inp->GetPrimaryScore();
			input.secondaryScore = inp->GetSecondaryScore();
			input.result = (UI::Result)inp->GetOracleResult();
			input.flags = inp->GetFlags();
		}
	}
}

void UIDeltaDebugging::GetInput(UIInput& input)
{
	if (!_ddcontroller->IsDeleted()) {
		auto inp = _ddcontroller->GetInput();
		if (inp) {
			input.id = inp->GetFormID();
			input.length = inp->Length();
			input.primaryScore = inp->GetPrimaryScore();
			input.secondaryScore = inp->GetSecondaryScore();
			input.result = (UI::Result)inp->GetOracleResult();
			input.flags = inp->GetFlags();
		}
	}
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

std::chrono::nanoseconds UIDeltaDebugging::GetRuntime()
{
	return _ddcontroller->GetRunTime();
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
	return sources.size();
}
void UIGeneration::GetSources(std::vector<UIInput>& inputs)
{ 
	if (hasbegun)
	{
		if (gotsources == false) {
			_generation->GetSources(sources);
			gotsources = true;
		}
		if (inputs.size() < sources.size())
			inputs.resize(sources.size());
		for (size_t i = 0; i < sources.size(); i++) {
			inputs[i].id = sources[i]->GetFormID();
			inputs[i].length = sources[i]->Length();
			inputs[i].primaryScore = sources[i]->GetPrimaryScore();
			inputs[i].secondaryScore = sources[i]->GetSecondaryScore();
			inputs[i].result = (UI::Result)sources[i]->GetOracleResult();
			inputs[i].flags = sources[i]->GetFlags();
			inputs[i].generationNumber = _generation->GetGenerationNumber();
			inputs[i].derivedInputs = sources[i]->GetDerivedInputs();
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
	lastddcontrollers = _generation->TryGetDDControllers(_ddcontrollers);
	size = lastddcontrollers;
	if (dd.size() < _ddcontrollers.size())
		dd.resize(_ddcontrollers.size());
	for (size_t i = 0; i < _ddcontrollers.size(); i++)
	{
		dd[i].SetDeltaController(_ddcontrollers[i]);
	}
}

std::chrono::nanoseconds UIGeneration::GetRuntime()
{
	return _generation->GetRunTime();
}

void UIGeneration::SetGeneration(std::shared_ptr<Generation> generation) 
{
	_generation = generation;
}

bool UIGeneration::Initialized()
{
	return _generation.operator bool();
}

void UIGeneration::Reset()
{
	_generation.reset();
}

bool UIInputInformation::Initialized()
{
	return _init;
}

void UIInputInformation::Set(std::shared_ptr<Input> input, std::shared_ptr<SessionData> sessiondata)
{
	_input = input;
	_sessiondata = sessiondata;
	Refresh();
	_init = true;
}

void UIInputInformation::Reset()
{
	_init = false;
	_input.reset();
	delete[] _cmdArgsBuf;
	delete[] _scriptArgsBuf;
	delete[] _inputconcatBuf;
	delete[] _inputlistBuf;
	_sessiondata.reset();
}

void UIInputInformation::Refresh()
{
	_inputID = _input->GetFormID();
	_generationID = _input->GetGenerationID();
	_derivedInputs = _input->GetDerivedInputs();
	_generationTime = _input->GetGenerationTime();
	_executionTime = _input->GetExecutionTime();
	_primaryScore = _input->GetPrimaryScore();
	_secondaryScore = _input->GetSecondaryScore();
	_hasfinished = _input->Finished();
	_trimmed = _input->IsTrimmed();
	_naturallength = _input->GetTargetLength();
	_trimmedlength = _input->GetTrimmedLength();
	_exitcode = _input->GetExitCode();
	_parentInput = _input->GetParentID();
	_flags = _input->GetFlags();
	_oracleResult = _input->GetOracleResult();

	FlagHolder flag(_input, Form::FormFlags::DoNotFree);
	if (_input->GetGenerated() == false) {
		// we are trying to add an _input that hasn't been generated or regenerated
		// try the generate it and if it succeeds add the test
		SessionFunctions::GenerateInput(_input, _sessiondata);
	}
	if (_input->test) {
		_exitreason = _input->test->_exitreason;
		_cmdArgs = _input->test->_cmdArgs;
		_scriptArgs = _input->test->_scriptArgs;
		bool stateerror = false;
		if (_cmdArgs == "")
			_cmdArgs = Lua::GetCmdArgs(std::bind(&Oracle::GetCmdArgs, _sessiondata->_oracle, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), _input->test, stateerror, false);
		if (_scriptArgs == "" && _sessiondata->_oracle->GetOracletype() == Oracle::PUTType::Script)
			_input->test->_scriptArgs = Lua::GetScriptArgs(std::bind(&Oracle::GetScriptArgs, _sessiondata->_oracle, std::placeholders::_1, std::placeholders::_2), _input->test, stateerror);
		_output = _input->test->_output;
	}
	// get input list
	_inputconcat = "";
	_inputlist = "";
	if (_input->GetGenerated()) {
		auto itr = _input->begin();
		if (itr != _input->end()) {
			_inputconcat += *itr;
			_inputlist += "\"" + *itr + "\"";
			itr++;
		}
		while (itr != _input->end()) {
			_inputconcat += *itr;
			_inputlist += ", \"" + *itr + "\"";
			itr++;
		}
	}

	_cmdArgsBuf = new char[_cmdArgs.size()];
	strncpy(_cmdArgsBuf, _cmdArgs.c_str(), _cmdArgs.size());
	_scriptArgsBuf = new char[_scriptArgs.size()];
	strncpy(_scriptArgsBuf, _scriptArgs.c_str(), _scriptArgs.size());
	_inputconcatBuf = new char[_inputconcat.size()];
	strncpy(_inputconcatBuf, _inputconcat.c_str(), _inputconcat.size());
	_inputlistBuf = new char[_inputlist.size()];
	strncpy(_inputlistBuf, _inputlist.c_str(), _inputlist.size());
}


void UITaskController::Set(std::shared_ptr<TaskController> controller)
{
	_controller = controller;
}

bool UITaskController::Initialized()
{
	return _controller.operator bool();
}


std::unordered_map<std::string, int64_t>::iterator UITaskController::beginExecutedTasks()
{
	return _controller->BeginExecutedTasks();
}
std::unordered_map<std::string, int64_t>::iterator UITaskController::endExecutedTasks()
{
	return _controller->EndExecutedTasks();
}
void UITaskController::LockExecutedTasks()
{
	_controller->LockExecutedTasks();
}
void UITaskController::UnlockExecutedTasks()
{
	_controller->UnlockExecutedTasks();
}
