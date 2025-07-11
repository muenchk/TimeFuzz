#include "SessionData.h"
#include "Settings.h"
#include "BufferOperations.h"
#include "Data.h"
#include "SessionFunctions.h"
#include "Generation.h"

#include <exception>
#include <stdexcept>
#include <memory>

void SessionData::SetInsert(std::shared_ptr<InputNode> /*node*/)
{
	/* if (auto ptr = node->input.lock(); ptr)
	{
		ptr->SetFlag(Form::FormFlags::DoNotFree);
		if (ptr->derive)
			ptr->derive->SetFlag(Form::FormFlags::DoNotFree);
	}*/
}
void SessionData::SetRemove(std::shared_ptr<InputNode> /*node*/)
{
	/* if (auto ptr = node->input.lock(); ptr) {
		if (ptr->HasFlag(Form::FormFlags::DoNotFree))
			ptr->UnsetFlag(Form::FormFlags::DoNotFree);
		if (ptr->derive) {
			if (ptr->derive->HasFlag(Form::FormFlags::DoNotFree))
				ptr->derive->SetFlag(Form::FormFlags::DoNotFree);
		}
		ptr->FreeMemory();
		if (ptr->GetGenerated() == false) {
			if (ptr->derive)
				ptr->derive->FreeMemory();
		}
	}*/
}

void SessionData::Init()
{
	switch (this->_settings->generation.sourcesType) {
	case Settings::GenerationSourcesType::FilterLength:
		_negativeInputs.set_ordering(std::SetOrdering::Length);
		_unfinishedInputs.set_ordering(std::SetOrdering::Length);
		//_positiveInputs.set_ordering(std::SetOrdering::Length);
		break;
	case Settings::GenerationSourcesType::FilterPrimaryScore:
	case Settings::GenerationSourcesType::FilterPrimaryScoreRelative:
		_negativeInputs.set_ordering(std::SetOrdering::Primary);
		_unfinishedInputs.set_ordering(std::SetOrdering::Primary);
		//_positiveInputs.set_ordering(std::SetOrdering::Primary);
		break;
	case Settings::GenerationSourcesType::FilterSecondaryScore:
	case Settings::GenerationSourcesType::FilterSecondaryScoreRelative:
		_negativeInputs.set_ordering(std::SetOrdering::Secondary);
		_unfinishedInputs.set_ordering(std::SetOrdering::Secondary);
		//_positiveInputs.set_ordering(std::SetOrdering::Secondary);
		break;
	}
	_defaultGen = data->CreateForm<Generation>();
	_defaultGen->SetTargetSize(INT64_MAX);
	_defaultGen->SetMaxSimultaneuosGeneration(_settings->generation.generationstep);
	_defaultGen->SetMaxActiveInputs(_settings->generation.activeGeneratedInputs);
	_defaultGen->SetMaxDerivedFailingInput(_settings->generation.maxNumberOfFailsPerSource);
	_defaultGen->SetMaxDerivedInput(_settings->generation.maxNumberOfGenerationsPerSource);
	_defaultGen->SetGenerationNumber(0);
	_defaultGen->SetStartTime(data->GetRuntime());

	_topK_primary_Unfinished.SetInsertFunction(SetInsert);
	_topK_primary_Unfinished.SetRemoveFunction(SetRemove);
	_topK_secondary_Unfinished.SetInsertFunction(SetInsert);
	_topK_secondary_Unfinished.SetRemoveFunction(SetRemove);
	_topK_primary.SetInsertFunction(SetInsert);
	_topK_primary.SetRemoveFunction(SetRemove);
	_topK_secondary.SetInsertFunction(SetInsert);
	_topK_secondary.SetRemoveFunction(SetRemove);
	_topK_length.SetInsertFunction(SetInsert);
	_topK_length.SetRemoveFunction(SetRemove);
	_topK_length_Unfinished.SetInsertFunction(SetInsert);
	_topK_length_Unfinished.SetRemoveFunction(SetRemove);
}

SessionData::~SessionData()
{
	Clear();
}

void SessionData::SetMaxGenerationCallbacks(int32_t max)
{
	_max_generation_callbacks = max;
}

void SessionData::Clear()
{
	Form::ClearForm();
	// don't clear this class, our main session class will do this,
	// so we have the opportunity to properly clear up threads etc.
}

void SessionData::AddInput(std::shared_ptr<Input>& input, EnumType list, double optionalweight)
{
	if (!input)
		return;
	switch (list) {
	case OracleResult::Failing:
		{
			std::shared_ptr<InputNode> node = std::make_shared<InputNode>();
			node->input = input;
			node->weight = optionalweight;
			node->primary = input->GetPrimaryScore();
			node->secondary = input->GetSecondaryScore();
			node->length = input->EffectiveLength();
			std::unique_lock<std::shared_mutex> guard(_negativeInputsLock);
			_negativeInputs.insert(node);
			std::unique_lock<std::shared_mutex> guardm(_multiset_lock);
			_topK_primary.insert(node);
			_topK_length.insert(node);
			_topK_secondary.insert(node);
		}
		break;
	case OracleResult::Passing:
		{
			std::unique_lock<std::shared_mutex> guard(_positiveInputsLock);
			//_positiveInputs.insert(input);
			_positiveInputs.push_back(input);
		}
		break;
	case OracleResult::Undefined:
		{
			std::unique_lock<std::shared_mutex> guard(_undefinedInputLock);
			_undefinedInputs.push_back(input->GetFormID());
		}
		break;
	case OracleResult::Unfinished:
		{
			std::shared_ptr<InputNode> node = std::make_shared<InputNode>();
			node->input = input;
			node->weight = optionalweight;
			node->primary = input->GetPrimaryScore();
			node->secondary = input->GetSecondaryScore();
			node->length = input->EffectiveLength();
			std::unique_lock<std::shared_mutex> guard(_unfinishedInputsLock);
			_unfinishedInputs.insert(node);
			std::unique_lock<std::shared_mutex> guardm(_multiset_lock);
			_topK_primary.insert(node);
			_topK_secondary.insert(node);
			_topK_length.insert(node);
			_topK_primary_Unfinished.insert(node);
			_topK_secondary_Unfinished.insert(node);
			_topK_length_Unfinished.insert(node);
			break;
		}
	default:
		break;
	}

	// see cppreference for this section (source)
	while (_lastrunFlag.test_and_set(std::memory_order_acquire))
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
		_lastrunFlag.wait(true, std::memory_order_relaxed)
#endif
			;
	_lastrun.push_back(input);

	// exit flag
	_lastrunFlag.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
	_lastrunFlag.notify_one();
#endif
}

std::shared_ptr<Generation> SessionData::GetGen()
{
	Utility::SpinLock guard(_generationFlag);
	return _generation;
}

void SessionData::SetGen(std::shared_ptr<Generation> gen)
{
	Utility::SpinLock guard(_generationFlag);
	_generation = gen;
}

std::shared_ptr<Generation> SessionData::ExchangeGen(std::shared_ptr<Generation> newgen)
{
	Utility::SpinLock guard(_generationFlag);
	auto gen = _generation;
	_generation = newgen;
	return gen;
}

int64_t SessionData::GetGenerationFails() 
{
	return _generationfails;
}

int64_t SessionData::GetGeneratedInputs() 
{
	return _generatedinputs;
}

int64_t SessionData::GetGeneratedPrefix() 
{
	return _generatedWithPrefix;
}

int64_t SessionData::GetAddTestFails()
{
	return _addtestFails;
}

int64_t SessionData::GetExcludedApproximation()
{
	return _generatedExcludedApproximation;
}

double SessionData::GetGenerationFailureRate() 
{
	return _failureRate;
}

TestExitStats& SessionData::GetTestExitStats()
{
	return exitstats;
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK_Length(int32_t k, size_t min_length_unfinished, size_t min_length_failing)
{
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	std::shared_lock<std::shared_mutex> guardm(_multiset_lock);
	auto itr = _topK_length.begin();
	while (count < k && itr != _topK_length.end()) {
		auto input = (*itr)->input.lock();
		if (input) [[likely]] {
			if (input->GetOracleResult() == OracleResult::Failing && (*itr)->length >= min_length_failing ||
				input->GetOracleResult() == OracleResult::Unfinished && (*itr)->length >= min_length_unfinished)
				ret.push_back((*itr)->input.lock());
		}
		itr++;
		count++;
	}
	return ret;
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK_Length_Unfinished(int32_t k, size_t min_length)
{
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	std::shared_lock<std::shared_mutex> guardm(_multiset_lock);
	auto itr = _topK_length_Unfinished.begin();
	while (count < k && itr != _topK_length_Unfinished.end()) {
		if ((*itr)->length >= min_length)
			ret.push_back((*itr)->input.lock());
		itr++;
		count++;
	}
	return ret;
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK(int32_t k, size_t min_length_unfinished, size_t min_length_failing)
{
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	std::shared_lock<std::shared_mutex> guardm(_multiset_lock);
	auto itr = _topK_primary.begin();
	while (count < k && itr != _topK_primary.end()) {
		auto input = (*itr)->input.lock();
		if (input) [[likely]] {
			if (input->GetOracleResult() == OracleResult::Failing && (*itr)->length >= min_length_failing ||
				input->GetOracleResult() == OracleResult::Unfinished && (*itr)->length >= min_length_unfinished)
				ret.push_back((*itr)->input.lock());
		}
		itr++;
		count++;
	}
	return ret;
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK_Unfinished(int32_t k, size_t min_length)
{
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	std::shared_lock<std::shared_mutex> guardm(_multiset_lock);
	auto itr = _topK_primary_Unfinished.begin();
	while (count < k && itr != _topK_primary_Unfinished.end()) {
		if ((*itr)->length >= min_length)
			ret.push_back((*itr)->input.lock());
		itr++;
		count++;
	}
	return ret;
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK_Secondary(int32_t k, size_t min_length_unfinished, size_t min_length_failing)
{
	std::shared_lock<std::shared_mutex> guardm(_multiset_lock);
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	auto itr = _topK_secondary.begin();
	while (count < k && itr != _topK_secondary.end()) {
		auto input = (*itr)->input.lock();
		if (input) [[likely]] {
			if (input->GetOracleResult() == OracleResult::Failing && (*itr)->length >= min_length_failing ||
				input->GetOracleResult() == OracleResult::Unfinished && (*itr)->length >= min_length_unfinished)
				ret.push_back((*itr)->input.lock());
		}
		itr++;
		count++;
	}
	return ret;
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK_Secondary_Unfinished(int32_t k, size_t min_length)
{
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	std::shared_lock<std::shared_mutex> guardm(_multiset_lock);
	auto itr = _topK_secondary_Unfinished.begin();
	while (count < k && itr != _topK_secondary.end()) {
		if ((*itr)->length >= min_length)
			ret.push_back((*itr)->input.lock());
		itr++;
		count++;
	}
	return ret;
}


std::vector<std::shared_ptr<Input>> SessionData::FindKSources(int32_t k, std::set<std::shared_ptr<Input>> exclusionlist, bool allowFailing, size_t min_length_unfinished, size_t min_length_failing)
{
	std::vector<std::shared_ptr<Input>> ret;
	auto itr = _unfinishedInputs.begin();
	while ((int32_t)ret.size() < k && itr != _unfinishedInputs.end())
	{
		auto input = (*itr)->input.lock();
		if (input) {
			if (!allowFailing || (*itr)->length < min_length_failing) {
				itr++;
				continue;
			}
			if ((_settings->generation.maxNumberOfFailsPerSource > input->GetDerivedFails() || _settings->generation.maxNumberOfFailsPerSource == 0) && (_settings->generation.maxNumberOfGenerationsPerSource == 0 || _settings->generation.maxNumberOfGenerationsPerSource > input->GetDerivedInputs()) && exclusionlist.contains(input) == false)
				ret.push_back(input);
		}
		itr++;
	}
	auto itra = _negativeInputs.begin();
	while ((int32_t)ret.size() < k && itra != _negativeInputs.end()) {
		auto input = (*itra)->input.lock();
		if (input) {
			if ((*itra)->length < min_length_unfinished) {
				itra++;
				continue;
			}
			if ((_settings->generation.maxNumberOfFailsPerSource > input->GetDerivedFails() || _settings->generation.maxNumberOfFailsPerSource == 0) && (_settings->generation.maxNumberOfGenerationsPerSource == 0 || _settings->generation.maxNumberOfGenerationsPerSource > input->GetDerivedInputs()) && exclusionlist.contains(input) == false)
				ret.push_back(input);
		}
		itra++;
	}
	return ret;
}

void SessionData::GetPositiveInputs(int32_t k, std::vector<std::shared_ptr<Input>>& posinputs)
{
	std::shared_lock<std::shared_mutex> guard(_positiveInputsLock);
	if (k != 0 && posinputs.size() < k)
		posinputs.resize(k);
	else if (posinputs.size() < _positiveInputs.size())
		posinputs.resize(_positiveInputs.size());
	auto itr = _positiveInputs.begin();
	int32_t count = 0;
	while ((k == 0 || count < k) && itr != _positiveInputs.end())
	{
		posinputs[count] = *itr;
		count++;
		itr++;
	}
}

void SessionData::VisitPositiveInputs(std::function<bool(std::shared_ptr<Input>)> visitor, size_t begin)
{
	std::shared_lock<std::shared_mutex> guard(_positiveInputsLock);
	for (int64_t i = (int64_t)begin; i < (int64_t)_positiveInputs.size(); i++) {
		if (visitor(_positiveInputs[i]) == false)
			break;
	}
	/* auto itr = _positiveInputs.begin();
	while (itr != _positiveInputs.end())
	{
		if (visitor(*itr) == false)
			break;
		itr++;
	}*/
}

void SessionData::VisitLastRun(std::function<bool(std::shared_ptr<Input>)> visitor)
{
	// see cppreference for this section (source)
	while (_lastrunFlag.test_and_set(std::memory_order_acquire))
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
		_lastrunFlag.wait(true, std::memory_order_relaxed)
#endif
			;

	auto itr = _lastrun.begin();
	while (itr != _lastrun.end())
	{
		if (auto ptr = (*itr).lock(); ptr) {
			if (visitor(ptr) == false)
				break;
		}
		itr++;
	}

	// exit flag
	_lastrunFlag.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
	_lastrunFlag.notify_one();
#endif
}


std::shared_ptr<Generation> SessionData::GetCurrentGeneration()
{
	if (_settings->generation.generationalMode)
		return GetGen();
	else {
		return _defaultGen;
	}
}

std::shared_ptr<Generation> SessionData::GetGeneration(FormID generationID)
{
	if (_settings->generation.generationalMode) {
		std::shared_lock<std::shared_mutex> guard(_generationsLock);
		auto itr = _generations.find(generationID);
		if (itr != _generations.end()) {
			return itr->second;
		}else {
			static std::shared_ptr<Generation> genptr = std::make_shared<Generation>();
			return genptr;
		}
	} else {
		return _defaultGen;
	}
}

std::shared_ptr<Generation> SessionData::GetGenerationByNumber(int32_t generationNumber)
{
	if (_settings->generation.generationalMode)
	{
		auto itr = _generations.begin();
		while (itr != _generations.end())
		{
			if (itr->second->GetGenerationNumber() == generationNumber)
				return itr->second;
		}
	} 
	return _defaultGen;
}

std::shared_ptr<Generation> SessionData::GetLastGeneration()
{
	return GetGeneration(_lastGenerationID);
}

void SessionData::GetGenerationIDs(std::vector<std::pair<FormID, int32_t>>& gens, size_t& size)
{
	std::shared_lock<std::shared_mutex> guard(_generationsLock);
	if (_generations.size() > gens.size())
		gens.resize(_generations.size());
	size = _generations.size();
	int i = 0;
	auto itr = _generations.begin();
	while (itr != _generations.end() && (size_t)i < size)
	{
		gens[i] = { itr->first, itr->second->GetGenerationNumber() };
		itr++;
		i++;
	}
}

void SessionData::SetNewGeneration(bool force)
{
	if (force || _settings->generation.generationalMode) {
		std::shared_ptr<Generation> oldgen;
		std::shared_ptr<Generation> newgen = data->CreateForm<Generation>();
		// add to generations map
		{
			std::unique_lock<std::shared_mutex> guard(_generationsLock);
			_generations.insert_or_assign(newgen->GetFormID(), newgen);
		}
		// init generation data
		newgen->SetTargetSize(_settings->generation.generationsize);
		newgen->SetMaxSimultaneuosGeneration(_settings->generation.generationstep);
		newgen->SetMaxActiveInputs(_settings->generation.activeGeneratedInputs);
		newgen->SetMaxDerivedFailingInput(_settings->generation.maxNumberOfFailsPerSource);
		newgen->SetMaxDerivedInput(_settings->generation.maxNumberOfGenerationsPerSource);
		oldgen = ExchangeGen(newgen);
		if (oldgen) {
			newgen->SetGenerationNumber(oldgen->GetGenerationNumber() + 1);  // increment generation
			_lastGenerationID = oldgen->GetFormID();
		}
		else
			newgen->SetGenerationNumber(1);  // first generation
		newgen->SetStartTime(data->GetRuntime());
		_generationEnding = false;
		_generationID = newgen->GetFormID();
	}
}

bool SessionData::CheckGenerationEnd(bool toomanyfails)
{
	if (_settings->generation.generationalMode) {
		// checks whether the current session should end and prepares to do so
		bool exp = false;
		auto gen = GetGen();
		if ((gen->IsActive() == false || !gen->NeedsGeneration() && _exechandler->WaitingTasks() == 0 && _exechandler->GetRunningTests() == 0) && _generationEnding.compare_exchange_strong(exp, true) /*if gen is inactive and genending is false, set it to true and execute this block*/) {
			// generation has finished
			auto call = dynamic_pointer_cast<Functions::GenerationEndCallback>(Functions::GenerationEndCallback::Create());
			call->_sessiondata = data->CreateForm<SessionData>();  // self
			_controller->AddTask(call);
			return true;
		} else if ((_settings->generation.maxNumberOfFailsPerSource != 0 || _settings->generation.maxNumberOfGenerationsPerSource != 0) && gen->CheckSourceValidity([this](std::shared_ptr<Input> input) {
					   if ((_settings->generation.maxNumberOfFailsPerSource == 0 || input->GetDerivedFails() < _settings->generation.maxNumberOfFailsPerSource) &&
						   (_settings->generation.maxNumberOfGenerationsPerSource == 0 || input->GetDerivedInputs() < _settings->generation.maxNumberOfGenerationsPerSource))
						   return true;
					   else
						   return false;
				   }) == false &&
				   _generationEnding.compare_exchange_strong(exp, true)) {
			// generation is being forcefully ended
			auto call = dynamic_pointer_cast<Functions::GenerationEndCallback>(Functions::GenerationEndCallback::Create());
			call->_sessiondata = data->CreateForm<SessionData>();  // self
			_controller->AddTask(call);
			return true;
		} else if (toomanyfails && _generationEnding.compare_exchange_strong(exp, true)) {
			// generation is being forcefully ended
			auto call = dynamic_pointer_cast<Functions::GenerationEndCallback>(Functions::GenerationEndCallback::Create());
			call->_sessiondata = data->CreateForm<SessionData>();  // self
			_controller->AddTask(call);
			return true;
		}
		return _generationEnding.load();
	} else {
		return false;
	}
}

bool SessionData::GetGenerationEnding()
{
	return _generationEnding.load();
}

bool SessionData::GetGenerationFinishing()
{
	return _generationFinishing.load();
}

int64_t SessionData::GetNumberInputsToGenerate()
{
	if (_settings->generation.generationalMode)
	{
		auto [res, num] = GetGen()->CanGenerate();
		if (res)
			return num;
		else
			return 0;
	} else {
		//return _settings->generation.generationsize - (int64_t)_exechandler->WaitingTasks();
		auto [res, num] = _defaultGen->CanGenerate();
		if (res)
			return num;
		else
			return 0;
	}
}

int64_t SessionData::CheckNumberInputsToGenerate(int64_t generated, int64_t /*failcount*/, int64_t togenerate)
{
	//if (_settings->generation.generationalMode)
		return togenerate - generated;
	//else
	//	return _settings->generation.activeGeneratedInputs > (int64_t)_exechandler->WaitingTasks() && failcount < (int64_t)GENERATION_RETRIES && generated < _settings->generation.generationstep;
}

void SessionData::FailNumberInputsToGenerate(int64_t fails, int64_t)
{
	if (_settings->generation.generationalMode) 
		GetGen()->FailGeneration(fails);
}

uint64_t SessionData::GetUsedMemory()
{
	return _memory_mem;
}

void SessionData::Acquire_InputGenerationReadersLock()
{
	_InputGenerationLock.lock_shared();
}

void SessionData::Release_InputGenerationReadersLock()
{
	_InputGenerationLock.unlock_shared();
}

void SessionData::Acquire_InputGenerationWritersLock()
{
	_InputGenerationLock.lock();
}

void SessionData::Release_InputGenerationWritersLock()
{
	_InputGenerationLock.unlock();
}

bool SessionData::BlockInputGeneration()
{
	return _blockInputGeneration.exchange(true);
}

bool SessionData::CanGenerate()
{
	return !_blockInputGeneration.load();
}

#pragma region FORM

size_t SessionData::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4     // version
	                        + 8   // _positiveInputNumbers
	                        + 8   // _negativeInputNumbers
	                        + 8   // _unfinishedInputNumbers
	                        + 8   // _undefinedInputnNumbers
	                        + 8   // listsize _positiveInputs
	                        + 8   // listsize _negativeInputs
	                        + 8   // listsize _unfinishedInputs
	                        + 8   // listsize _undefinedInputs
	                        + 8   // buffer size _recentfails
	                        + 8   // _generationfails
	                        + 8   // _generatedInputs
	                        + 8   // _generatedWithPrefix
	                        + 8   // _generatedExcludedApproximation
	                        + 8   // lastchecks
	                        + 8   // exitstats.natural
	                        + 8   // exitstats.lastinput
	                        + 8   // exitstats.terminated
	                        + 8   // exitstats.timeout
	                        + 8   // exitstats.fragmenttimeout
	                        + 8   // exitstats.memory
	                        + 8   // exitstats.initerror
	                        + 8   // failureRate
	                        + 8   // addTestFails
	                        + 8   // grammar id
	                        + 8   // generation ID
	                        + 8;  // lastGenerationID

	static size_t size0x2 = size0x1  // last size
	                        + 8      // listsize _generations
	                        + 8      // exitstats.pipe
	                        + 1      // _generationEnding
	                        + 1      // _generationFinishing
	                        + 8;     // _defaultGen

	switch (version)
	{
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	default:
		return 0;
	}
}

size_t SessionData::GetDynamicSize()
{
	return Form::GetDynamicSize()                    // form stuff
	       + GetStaticSize(classversion) +           // static class size
	       _positiveInputs.size() * 8                // list elements are uint64_t
	       + _negativeInputs.size() * 16             // two list elements: uint64_t, double
	       + _unfinishedInputs.size() * 16           // two list elements: uint64_t, double
	       + _undefinedInputs.size() * 8             // list elements are uint64_t
	       + _recentfailes.size()                    // actual number of elements in the list * size of byte
	       + _generations.size() * 8;                // list elements are uint64_t
}

bool SessionData::WriteData(std::ostream* buffer, size_t& offset, size_t length)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset, length);

	Buffer::Write(_positiveInputNumbers, buffer, offset);
	Buffer::Write(_negativeInputNumbers, buffer, offset);
	Buffer::Write(_unfinishedInputNumbers, buffer, offset);
	Buffer::Write(_undefinedInputNumbers, buffer, offset);
	// positive inputs
	Buffer::WriteSize(_positiveInputs.size(), buffer, offset);
	auto itran = _positiveInputs.begin();
	while (itran != _positiveInputs.end()) {
		Buffer::Write((*itran)->GetFormID(), buffer, offset);
		itran++;
	}
	// negative inputs
	Buffer::WriteSize(_negativeInputs.size(), buffer, offset);
	auto itra = _negativeInputs.begin();
	while (itra != _negativeInputs.end()) {
		if (auto shared = (*itra)->input.lock(); shared) {
			Buffer::Write(shared->GetFormID(), buffer, offset);
			Buffer::Write((*itra)->weight, buffer, offset);
		} else {
			Buffer::Write((uint64_t)0x0, buffer, offset);
			Buffer::Write((double)0.0f, buffer, offset);
		}
		itra++;
	}
	// unfinished inputs
	Buffer::WriteSize(_unfinishedInputs.size(), buffer, offset);
	itra = _unfinishedInputs.begin();
	while (itra != _unfinishedInputs.end()) {
		if (auto shared = (*itra)->input.lock(); shared) {
			Buffer::Write(shared->GetFormID(), buffer, offset);
			Buffer::Write((*itra)->weight, buffer, offset);
		} else {
			Buffer::Write((uint64_t)0x0, buffer, offset);
			Buffer::Write((double)0.0f, buffer, offset);
		}
		itra++;
	}
	// undefined inputs
	Buffer::WriteSize(_undefinedInputs.size(), buffer, offset);
	auto itr = _undefinedInputs.begin();
	while (itr != _undefinedInputs.end()) {
		Buffer::Write(*itr, buffer, offset);
		itr++;
	}
	// recent fails
	Buffer::WriteSize(_recentfailes.size(), buffer, offset);
	auto itre = _recentfailes.begin();
	while (itre != _recentfailes.end())
	{
		Buffer::Write(*itre, buffer, offset);
		itre++;
	}
	// generations
	Buffer::WriteSize(_generations.size(), buffer, offset);
	for (auto [id, _] : _generations){
		Buffer::Write(id, buffer, offset);
		itre++;
	}
	// rest
	Buffer::Write(_generationfails, buffer, offset);
	Buffer::Write(_generatedinputs, buffer, offset);
	Buffer::Write(_generatedWithPrefix, buffer, offset);
	Buffer::Write(_generatedExcludedApproximation, buffer, offset);
	Buffer::Write(_lastchecks, buffer, offset);
	Buffer::Write(exitstats.natural, buffer, offset);
	Buffer::Write(exitstats.lastinput, buffer, offset);
	Buffer::Write(exitstats.terminated, buffer, offset);
	Buffer::Write(exitstats.timeout, buffer, offset);
	Buffer::Write(exitstats.fragmenttimeout, buffer, offset);
	Buffer::Write(exitstats.memory, buffer, offset);
	Buffer::Write(exitstats.pipe, buffer, offset);
	Buffer::Write(exitstats.initerror, buffer, offset);
	Buffer::Write(_failureRate, buffer, offset);
	Buffer::Write(_addtestFails, buffer, offset);
	if (_grammar)
		Buffer::Write(_grammar->GetFormID(), buffer, offset);
	else
		Buffer::Write((uint64_t)0, buffer, offset);
	if (GetGen())
		Buffer::Write(GetGen()->GetFormID(), buffer, offset);
	else
		Buffer::Write((FormID)0, buffer, offset);
	Buffer::Write(_lastGenerationID, buffer, offset);
	Buffer::Write(_generationEnding.load(), buffer, offset);
	Buffer::Write(_generationFinishing.load(), buffer, offset);
	if (_defaultGen)
		Buffer::Write(_defaultGen->GetFormID(), buffer, offset);
	else
		Buffer::Write((uint64_t)0, buffer, offset);
	return true;
}

bool SessionData::ReadData0x1(std::istream* buffer, size_t& offset, size_t /*length*/, LoadResolver* /*resolver*/)
{
	_positiveInputNumbers = Buffer::ReadInt64(buffer, offset);
	_negativeInputNumbers = Buffer::ReadInt64(buffer, offset);
	_unfinishedInputNumbers = Buffer::ReadInt64(buffer, offset);
	_undefinedInputNumbers = Buffer::ReadInt64(buffer, offset);
	// positive inputs
	_positiveInputs.clear();
	size_t sizepos = Buffer::ReadSize(buffer, offset);
	_loadData->posInp.resize(sizepos);
	for (int64_t i = 0; i < (int64_t)sizepos; i++) {
		_loadData->posInp.push_back(Buffer::ReadUInt64(buffer, offset));
	}
	// negative inputs
	_negativeInputs.clear();
	size_t size = Buffer::ReadSize(buffer, offset);
	_loadData->negInp.resize(size);
	FormID tmp = 0;
	for (int64_t i = 0; i < (int64_t)size; i++) {
		tmp = Buffer::ReadUInt64(buffer, offset);
		_loadData->negInp[i] = { tmp, Buffer::ReadDouble(buffer, offset) };
	}
	// unfinished inputs
	_unfinishedInputs.clear();
	size = Buffer::ReadSize(buffer, offset);
	_loadData->unfInp.resize(size);
	for (int64_t i = 0; i < (int64_t)size; i++) {
		tmp = Buffer::ReadUInt64(buffer, offset);
		_loadData->unfInp[i] = { tmp, Buffer::ReadDouble(buffer, offset) };
	}
	// undefined inputs
	_undefinedInputs.clear();
	sizepos = Buffer::ReadSize(buffer, offset);
	for (int64_t i = 0; i < (int64_t)sizepos; i++) {
		_undefinedInputs.push_back(Buffer::ReadUInt64(buffer, offset));
	}
	// recentfails
	_recentfailes.clear();
	size = Buffer::ReadSize(buffer, offset);
	for (int64_t i = 0; i < (int64_t)size; i++) {
		_recentfailes.push_back(Buffer::ReadUChar(buffer, offset));
	}
	// rest
	_generationfails = Buffer::ReadInt64(buffer, offset);
	_generatedinputs = Buffer::ReadInt64(buffer, offset);
	_generatedWithPrefix = Buffer::ReadInt64(buffer, offset);
	_generatedExcludedApproximation = Buffer::ReadInt64(buffer, offset);
	_lastchecks = Buffer::ReadTime(buffer, offset);
	return true;
}

bool SessionData::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	if (_loadData)
		delete _loadData;
	_loadData = new LoadData();

	SetChanged();
	int32_t version = Buffer::ReadInt32(buffer, offset);
	_loadData->version = version;
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			bool res = ReadData0x1(buffer, offset, length, resolver);
			if (!res)
				return res;
			exitstats.natural = Buffer::ReadUInt64(buffer, offset);
			exitstats.lastinput = Buffer::ReadUInt64(buffer, offset);
			exitstats.terminated = Buffer::ReadUInt64(buffer, offset);
			exitstats.timeout = Buffer::ReadUInt64(buffer, offset);
			exitstats.fragmenttimeout = Buffer::ReadUInt64(buffer, offset);
			exitstats.memory = Buffer::ReadUInt64(buffer, offset);
			exitstats.initerror = Buffer::ReadUInt64(buffer, offset);
			_failureRate = Buffer::ReadDouble(buffer, offset);
			_addtestFails = Buffer::ReadInt64(buffer, offset);
			_loadData->grammarid = Buffer::ReadUInt64(buffer, offset);
			_loadData->generationid = Buffer::ReadUInt64(buffer, offset);
			_lastGenerationID = Buffer::ReadUInt64(buffer, offset);

			_topK_primary_Unfinished.SetInsertFunction(SetInsert);
			_topK_primary_Unfinished.SetRemoveFunction(SetRemove);
			_topK_secondary_Unfinished.SetInsertFunction(SetInsert);
			_topK_secondary_Unfinished.SetRemoveFunction(SetRemove);
			_topK_primary.SetInsertFunction(SetInsert);
			_topK_primary.SetRemoveFunction(SetRemove);
			_topK_secondary.SetInsertFunction(SetInsert);
			_topK_secondary.SetRemoveFunction(SetRemove);
			_topK_length.SetInsertFunction(SetInsert);
			_topK_length.SetRemoveFunction(SetRemove);
			_topK_length_Unfinished.SetInsertFunction(SetInsert);
			_topK_length_Unfinished.SetRemoveFunction(SetRemove);
			return true;
		}
	case 0x2:
		{
			Form::ReadData(buffer, offset, length, resolver);
			_positiveInputNumbers = Buffer::ReadInt64(buffer, offset);
			_negativeInputNumbers = Buffer::ReadInt64(buffer, offset);
			_unfinishedInputNumbers = Buffer::ReadInt64(buffer, offset);
			_undefinedInputNumbers = Buffer::ReadInt64(buffer, offset);
			// positive inputs
			_positiveInputs.clear();
			size_t sizepos = Buffer::ReadSize(buffer, offset);
			_loadData->negInp.resize(sizepos);
			for (int64_t i = 0; i < (int64_t)sizepos; i++) {
				_loadData->posInp.push_back(Buffer::ReadUInt64(buffer, offset));
			}
			// negative inputs
			_negativeInputs.clear();
			size_t size = Buffer::ReadSize(buffer, offset);
			_loadData->negInp.resize(size);
			FormID tmp = 0;
			for (int64_t i = 0; i < (int64_t)size; i++) {
				tmp = Buffer::ReadUInt64(buffer, offset);
				_loadData->negInp[i] = { tmp, Buffer::ReadDouble(buffer, offset) };
			}
			// unfinished inputs
			_unfinishedInputs.clear();
			size = Buffer::ReadSize(buffer, offset);
			_loadData->unfInp.resize(size);
			for (int64_t i = 0; i < (int64_t)size; i++) {
				tmp = Buffer::ReadUInt64(buffer, offset);
				_loadData->unfInp[i] = { tmp, Buffer::ReadDouble(buffer, offset) };
			}
			// undefined inputs
			_undefinedInputs.clear();
			sizepos = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)sizepos; i++) {
				_undefinedInputs.push_back(Buffer::ReadUInt64(buffer, offset));
			}
			// recentfails
			_recentfailes.clear();
			size = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)size; i++) {
				_recentfailes.push_back(Buffer::ReadUChar(buffer, offset));
			}
			// _generations
			size = Buffer::ReadSize(buffer, offset);
			for (int64_t i = 0; i < (int64_t)size; i++)
				_loadData->gens.push_back(Buffer::ReadUInt64(buffer, offset));
			// rest
			_generationfails = Buffer::ReadInt64(buffer, offset);
			_generatedinputs = Buffer::ReadInt64(buffer, offset);
			_generatedWithPrefix = Buffer::ReadInt64(buffer, offset);
			_generatedExcludedApproximation = Buffer::ReadInt64(buffer, offset);
			_lastchecks = Buffer::ReadTime(buffer, offset);
			exitstats.natural = Buffer::ReadUInt64(buffer, offset);
			exitstats.lastinput = Buffer::ReadUInt64(buffer, offset);
			exitstats.terminated = Buffer::ReadUInt64(buffer, offset);
			exitstats.timeout = Buffer::ReadUInt64(buffer, offset);
			exitstats.fragmenttimeout = Buffer::ReadUInt64(buffer, offset);
			exitstats.memory = Buffer::ReadUInt64(buffer, offset);
			exitstats.pipe = Buffer::ReadUInt64(buffer, offset);
			exitstats.initerror = Buffer::ReadUInt64(buffer, offset);
			_failureRate = Buffer::ReadDouble(buffer, offset);
			_addtestFails = Buffer::ReadInt64(buffer, offset);
			_loadData->grammarid = Buffer::ReadUInt64(buffer, offset);
			_loadData->generationid = Buffer::ReadUInt64(buffer, offset);
			_lastGenerationID = Buffer::ReadUInt64(buffer, offset);
			_generationEnding = Buffer::ReadBool(buffer, offset);
			_generationFinishing = Buffer::ReadBool(buffer, offset);
			_loadData->defaultID = Buffer::ReadUInt64(buffer, offset);

			_topK_primary_Unfinished.SetInsertFunction(SetInsert);
			_topK_primary_Unfinished.SetRemoveFunction(SetRemove);
			_topK_secondary_Unfinished.SetInsertFunction(SetInsert);
			_topK_secondary_Unfinished.SetRemoveFunction(SetRemove);
			_topK_primary.SetInsertFunction(SetInsert);
			_topK_primary.SetRemoveFunction(SetRemove);
			_topK_secondary.SetInsertFunction(SetInsert);
			_topK_secondary.SetRemoveFunction(SetRemove);
			_topK_length.SetInsertFunction(SetInsert);
			_topK_length.SetRemoveFunction(SetRemove);
			_topK_length_Unfinished.SetInsertFunction(SetInsert);
			_topK_length_Unfinished.SetRemoveFunction(SetRemove);
			return true;
		}
		break;
	default:
		return false;
	}
}

void SessionData::InitializeEarly(LoadResolver* resolver)
{
	if (_loadData) {
		switch (_loadData->version) {
		case 0x1:
			{
				resolver->current = "SessionData";
				this->_oracle = resolver->ResolveFormID<Oracle>(Data::StaticFormIDs::Oracle);
				this->_controller = resolver->ResolveFormID<TaskController>(Data::StaticFormIDs::TaskController);
				this->_exechandler = resolver->ResolveFormID<ExecutionHandler>(Data::StaticFormIDs::ExecutionHandler);
				this->_generator = resolver->ResolveFormID<Generator>(Data::StaticFormIDs::Generator);
				this->_grammar = resolver->ResolveFormID<Grammar>(_loadData->grammarid);
				this->_settings = resolver->ResolveFormID<Settings>(Data::StaticFormIDs::Settings);
				this->_excltree = resolver->ResolveFormID<ExclusionTree>(Data::StaticFormIDs::ExclusionTree);
				this->SetGen(resolver->ResolveFormID<Generation>(_loadData->generationid));
				switch (this->_settings->generation.sourcesType) {
				case Settings::GenerationSourcesType::FilterLength:
					_negativeInputs.set_ordering(std::SetOrdering::Length);
					_unfinishedInputs.set_ordering(std::SetOrdering::Length);
					//_positiveInputs.set_ordering(std::SetOrdering::Length);
					break;
				case Settings::GenerationSourcesType::FilterPrimaryScore:
				case Settings::GenerationSourcesType::FilterPrimaryScoreRelative:
					_negativeInputs.set_ordering(std::SetOrdering::Primary);
					_unfinishedInputs.set_ordering(std::SetOrdering::Primary);
					//_positiveInputs.set_ordering(std::SetOrdering::Primary);
					break;
				case Settings::GenerationSourcesType::FilterSecondaryScore:
				case Settings::GenerationSourcesType::FilterSecondaryScoreRelative:
					_negativeInputs.set_ordering(std::SetOrdering::Secondary);
					_unfinishedInputs.set_ordering(std::SetOrdering::Secondary);
					//_positiveInputs.set_ordering(std::SetOrdering::Secondary);
					break;
				}
				if (this->GetGen())
					this->_generationID = GetGen()->GetFormID();
				// this is redundant and has already been set
				this->data = resolver->_data;
				auto gens = resolver->_data->GetFormArray<Generation>();
				for (auto gen : gens)
					this->_generations.insert_or_assign(gen->GetFormID(), gen);
			}
			break;
		case 0x2:
			{
				resolver->current = "SessionData";
				for (int64_t i = 0; i < (int64_t)_loadData->gens.size(); i++) {
					auto shared = resolver->ResolveFormID<Generation>(_loadData->gens[i]);
					if (shared)
						_generations.insert_or_assign(_loadData->gens[i], shared);
				}
				// rest
				this->_oracle = resolver->ResolveFormID<Oracle>(Data::StaticFormIDs::Oracle);
				this->_controller = resolver->ResolveFormID<TaskController>(Data::StaticFormIDs::TaskController);
				this->_exechandler = resolver->ResolveFormID<ExecutionHandler>(Data::StaticFormIDs::ExecutionHandler);
				this->_generator = resolver->ResolveFormID<Generator>(Data::StaticFormIDs::Generator);
				this->_grammar = resolver->ResolveFormID<Grammar>(_loadData->grammarid);
				this->_settings = resolver->ResolveFormID<Settings>(Data::StaticFormIDs::Settings);
				this->_excltree = resolver->ResolveFormID<ExclusionTree>(Data::StaticFormIDs::ExclusionTree);
				this->SetGen(resolver->ResolveFormID<Generation>(_loadData->generationid));
				switch (this->_settings->generation.sourcesType) {
				case Settings::GenerationSourcesType::FilterLength:
					_negativeInputs.set_ordering(std::SetOrdering::Length);
					_unfinishedInputs.set_ordering(std::SetOrdering::Length);
					//_positiveInputs.set_ordering(std::SetOrdering::Length);
					break;
				case Settings::GenerationSourcesType::FilterPrimaryScore:
				case Settings::GenerationSourcesType::FilterPrimaryScoreRelative:
					_negativeInputs.set_ordering(std::SetOrdering::Primary);
					_unfinishedInputs.set_ordering(std::SetOrdering::Primary);
					//_positiveInputs.set_ordering(std::SetOrdering::Primary);
					break;
				case Settings::GenerationSourcesType::FilterSecondaryScore:
				case Settings::GenerationSourcesType::FilterSecondaryScoreRelative:
					_negativeInputs.set_ordering(std::SetOrdering::Secondary);
					_unfinishedInputs.set_ordering(std::SetOrdering::Secondary);
					//_positiveInputs.set_ordering(std::SetOrdering::Secondary);
					break;
				}
				if (this->GetGen())
					this->_generationID = GetGen()->GetFormID();
				// this is redundant and has already been set
				this->data = resolver->_data;
				auto gens = resolver->_data->GetFormArray<Generation>();
				for (auto gen : gens)
					this->_generations.insert_or_assign(gen->GetFormID(), gen);
				// defaultgen
				this->_defaultGen = resolver->ResolveFormID<Generation>(_loadData->defaultID);
			}
			break;
		}
	}
}

void SessionData::InitializeLate(LoadResolver* resolver)
{
	if (_loadData) {
		switch (_loadData->version) {
		case 0x1:
			for (int64_t i = 0; i < (int64_t)_loadData->negInp.size(); i++) {
				auto shared = resolver->ResolveFormID<Input>(_loadData->negInp[i].first);
				if (shared && shared->derive && shared->test) {
					auto node = std::make_shared<InputNode>();
					node->input = shared;
					node->weight = _loadData->negInp[i].second;
					node->primary = shared->GetPrimaryScore();
					node->secondary = shared->GetSecondaryScore();
					node->length = shared->Length();
					_negativeInputs.insert(node);
					_topK_primary.insert(node);
					_topK_secondary.insert(node);
					_topK_length.insert(node);
				}
			}
			for (int64_t i = 0; i < (int64_t)_loadData->posInp.size(); i++) {
				auto shared = resolver->ResolveFormID<Input>(_loadData->posInp[i]);
				if (shared)
					_positiveInputs.push_back(shared);
				//_positiveInputs.insert(shared);
			}
			for (int64_t i = 0; i < (int64_t)_loadData->unfInp.size(); i++) {
				auto shared = resolver->ResolveFormID<Input>(_loadData->unfInp[i].first);
				if (shared && shared->derive && shared->test) {
					auto node = std::make_shared<InputNode>();
					node->input = shared;
					node->weight = _loadData->unfInp[i].second;
					node->primary = shared->GetPrimaryScore();
					node->secondary = shared->GetSecondaryScore();
					node->length = shared->Length();
					_unfinishedInputs.insert(node);
					_topK_primary.insert(node);
					_topK_secondary.insert(node);
					_topK_length.insert(node);
					_topK_primary_Unfinished.insert(node);
					_topK_secondary_Unfinished.insert(node);
					_topK_length_Unfinished.insert(node);
				}
			}
			break;
		case 0x2:
			resolver->current = "SessionData";
			for (int64_t i = 0; i < (int64_t)_loadData->negInp.size(); i++) {
				auto shared = resolver->ResolveFormID<Input>(_loadData->negInp[i].first);
				if (shared && shared->derive && shared->test) {
					auto node = std::make_shared<InputNode>();
					node->input = shared;
					node->weight = _loadData->negInp[i].second;
					node->primary = shared->GetPrimaryScore();
					node->secondary = shared->GetSecondaryScore();
					node->length = shared->Length();
					_negativeInputs.insert(node);
					_topK_primary.insert(node);
					_topK_secondary.insert(node);
					_topK_length.insert(node);
				}
			}
			for (int64_t i = 0; i < (int64_t)_loadData->posInp.size(); i++) {
				auto shared = resolver->ResolveFormID<Input>(_loadData->posInp[i]);
				if (shared)
					_positiveInputs.push_back(shared);
				//_positiveInputs.insert(shared);
			}
			for (int64_t i = 0; i < (int64_t)_loadData->unfInp.size(); i++) {
				auto shared = resolver->ResolveFormID<Input>(_loadData->unfInp[i].first);
				if (shared && shared->derive && shared->test) {
					auto node = std::make_shared<InputNode>();
					node->input = shared;
					node->weight = _loadData->unfInp[i].second;
					node->primary = shared->GetPrimaryScore();
					node->secondary = shared->GetSecondaryScore();
					node->length = shared->Length();
					_unfinishedInputs.insert(node);
					_topK_primary.insert(node);
					_topK_secondary.insert(node);
					_topK_length.insert(node);
					_topK_primary_Unfinished.insert(node);
					_topK_secondary_Unfinished.insert(node);
					_topK_length_Unfinished.insert(node);
				}
			}
			break;
		}
		delete _loadData;
		_loadData = nullptr;
	}
}

void SessionData::ClearChanged()
{
	Form::ClearChanged();
	SetChanged();
}
void SessionData::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
		Functions::RegisterFactory(Functions::MasterGenerationCallback::GetTypeStatic(), Functions::MasterGenerationCallback::Create);
		Functions::RegisterFactory(Functions::GenerationEndCallback::GetTypeStatic(), Functions::GenerationEndCallback::Create);
		Functions::RegisterFactory(Functions::GenerationFinishedCallback::GetTypeStatic(), Functions::GenerationFinishedCallback::Create);
	}
}

size_t SessionData::MemorySize()
{
	return sizeof(SessionData) + sizeof(boost::circular_buffer<unsigned char>) + sizeof(unsigned char) * GENERATION_WEIGHT_BUFFER_SIZE + sizeof(std::pair<FormID, std::shared_ptr<Generation>>) * _generations.size() + sizeof(std::pair<std::shared_ptr<InputNode>, InputNodeLess>) * 600 + sizeof(InputNode) * (_negativeInputs.size() + _unfinishedInputs.size()) + sizeof(FormID) * (_undefinedInputs.size() + _positiveInputs.size());
}

#pragma endregion
