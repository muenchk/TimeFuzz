#include "SessionData.h"
#include "Settings.h"
#include "BufferOperations.h"
#include "Data.h"
#include "SessionFunctions.h"
#include "Generation.h"

#include <exception>
#include <stdexcept>
#include <memory>

SessionData::~SessionData()
{
	Clear();
}

void SessionData::Clear()
{
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
			std::unique_lock<std::shared_mutex> guard(_negativeInputsLock);
			_negativeInputs.insert(node);
			_topK_primary.insert(node);
			_topK_secondary.insert(node);
		}
		break;
	case OracleResult::Passing:
		{
			std::unique_lock<std::shared_mutex> guard(_positiveInputsLock);
			_positiveInputs.push_back(input->GetFormID());
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
			std::unique_lock<std::shared_mutex> guard(_unfinishedInputsLock);
			_unfinishedInputs.insert(node);
			_topK_primary.insert(node);
			_topK_secondary.insert(node);
			_topK_primary_Unfinished.insert(node);
			_topK_secondary_Unfinished.insert(node);
			break;
		}
	default:
		break;
	}
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

	switch (version) {
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t SessionData::GetDynamicSize()
{
	return Form::GetDynamicSize()           // form stuff
	       + GetStaticSize(classversion) +  // static class size
	       _positiveInputs.size() * 8       // list elements are uint64_t
	       + _negativeInputs.size() * 16    // two list elements: uint64_t, double
	       + _unfinishedInputs.size() * 16  // two list elements: uint64_t, double
	       + _undefinedInputs.size() * 8    // list elements are uint64_t
	       + _recentfailes.size();          // actual number of elements in the list * size of byte
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK(int32_t k)
{
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	auto itr = _topK_primary.begin();
	while (count < k && itr != _topK_primary.end()) {
		ret.push_back((*itr)->input.lock());
		itr++;
		count++;
	}
	return ret;
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK_Unfinished(int32_t k)
{
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	auto itr = _topK_primary_Unfinished.begin();
	while (count < k && itr != _topK_primary_Unfinished.end()) {
		ret.push_back((*itr)->input.lock());
		itr++;
		count++;
	}
	return ret;
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK_Secondary(int32_t k)
{
	if (k == 1)
	{
		/*auto itr = std::max_element(_unfinishedInputs.begin(), _unfinishedInputs.end(), [](std::shared_ptr<InputNode> lhs, std::shared_ptr<InputNode> rhs) {
			if (lhs->secondary == rhs->secondary)
				return lhs->primary > rhs->primary;
			else
				return lhs->secondary > rhs->secondary;
			});
		auto itra = std::max_element(_negativeInputs.begin(), _negativeInputs.end(), [](std::shared_ptr<InputNode> lhs, std::shared_ptr<InputNode> rhs) {
			if (lhs->secondary == rhs->secondary)
				return lhs->primary > rhs->primary;
			else
				return lhs->secondary > rhs->secondary;
		});
		if (itr == _unfinishedInputs.end() && itra == _negativeInputs.end())
			return {};
		else if (itr == _unfinishedInputs.end())
			return { (*itra)->input.lock() };
		else if (itra == _negativeInputs.end())
			return { (*itr)->input.lock() };
		else
			return {
				std::max(*itr, *itra, [](std::shared_ptr<InputNode> lhs, std::shared_ptr<InputNode> rhs) {
					if (lhs->secondary == rhs->secondary)
						return lhs->primary > rhs->primary;
					else
						return lhs->secondary > rhs->secondary;
				})->input.lock()
			};*/
		auto itr = _topK_secondary.begin();
		if (itr != _topK_secondary.end())
			return { (*itr)->input.lock() };
		else
			return {};
	}
	else
	{
		std::vector<std::shared_ptr<Input>> ret;
		int32_t count = 0;
		auto itr = _topK_secondary.begin();
		while (count < k && itr != _topK_secondary.end()) {
			ret.push_back((*itr)->input.lock());
			itr++;
			count++;
		}
		return ret;
	}
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK_Secondary_Unfinished(int32_t k)
{
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	auto itr = _topK_secondary_Unfinished.begin();
	while (count < k && itr != _topK_secondary.end()) {
		ret.push_back((*itr)->input.lock());
		itr++;
		count++;
	}
	return ret;
}

std::shared_ptr<Generation> SessionData::GetCurrentGeneration()
{
	if (_settings->generation.generationalMode)
		return _generation.load();
	else {
		static Generation gen;
		static std::shared_ptr<Generation> genptr = std::shared_ptr<Generation>(std::addressof(gen));
		return genptr;
	}
}

std::shared_ptr<Generation> SessionData::GetGeneration(FormID generationID)
{
	if (_settings->generation.generationalMode) {
		try {
			return _generations.at(generationID);
		} catch (std::exception&) {
			static Generation gen;
			static std::shared_ptr<Generation> genptr = std::shared_ptr<Generation>(std::addressof(gen));
			return genptr;
		}
	}
	else {
		static Generation gen;
		static std::shared_ptr<Generation> genptr = std::shared_ptr<Generation>(std::addressof(gen));
		return genptr;
	}
}

std::shared_ptr<Generation> SessionData::GetGenerationByNumber(FormID generationNumber)
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
	static Generation gen;
	static std::shared_ptr<Generation> genptr = std::shared_ptr<Generation>(std::addressof(gen));
	return genptr;
}

std::shared_ptr<Generation> SessionData::GetLastGeneration()
{
	return GetGeneration(_lastGenerationID);
}

void SessionData::GetGenerationIDs(std::vector<std::pair<FormID, FormID>>& gens, size_t& size)
{
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

void SessionData::SetNewGeneration()
{
	if (_settings->generation.generationalMode) {
		std::shared_ptr<Generation> oldgen;
		std::shared_ptr<Generation> newgen = data->CreateForm<Generation>();
		// add to generations map
		_generations.insert_or_assign(newgen->GetFormID(), newgen);
		// init generation data
		newgen->SetTargetSize(_settings->generation.generationsize);
		newgen->SetMaxSimultaneuosGeneration(_settings->generation.generationstep);
		newgen->SetMaxActiveInputs(_settings->generation.activeGeneratedInputs);
		oldgen = _generation.exchange(newgen);
		if (oldgen) {
			newgen->SetGenerationNumber(oldgen->GetGenerationNumber() + 1);  // increment generation
			_lastGenerationID = oldgen->GetFormID();
		}
		else
			newgen->SetGenerationNumber(1);  // first generation
		_generationEnding = false;
		_generationID = newgen->GetFormID();
	}
}

void SessionData::CheckGenerationEnd()
{
	// checks whether the current session should end and prepares to do so
	bool exp = false;
	if (_generation.load()->IsActive() == false && _generationEnding.compare_exchange_strong(exp, true) /*if gen is inactive and genending is false, set it to true and execute this block*/) {
		// generation has finished
		auto call = dynamic_pointer_cast<Functions::GenerationEndCallback>(Functions::GenerationEndCallback::Create());
		call->_sessiondata = data->CreateForm<SessionData>(); // self
		_controller->AddTask(call);
	}
}

int64_t SessionData::GetNumberInputsToGenerate()
{
	if (_settings->generation.generationalMode)
	{
		auto [res, num] = _generation.load()->CanGenerate();
		if (res)
			return num;
		else
			return 0;
	} else
		return _settings->generation.generationsize - (int64_t)_exechandler->WaitingTasks();
}

int64_t SessionData::CheckNumberInputsToGenerate(int64_t generated, int64_t failcount, int64_t togenerate)
{
	if (_settings->generation.generationalMode)
		return togenerate - generated;
	else
		return _settings->generation.activeGeneratedInputs > (int64_t)_exechandler->WaitingTasks() && failcount < (int64_t)GENERATION_RETRIES && generated < _settings->generation.generationstep;
}

void SessionData::FailNumberInputsToGenerate(int64_t fails, int64_t)
{
	if (_settings->generation.generationalMode)
		_generation.load()->FailGeneration(fails);
}

uint64_t SessionData::GetUsedMemory()
{
	return _memory_mem;
}

bool SessionData::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);

	Buffer::Write(_positiveInputNumbers, buffer, offset);
	Buffer::Write(_negativeInputNumbers, buffer, offset);
	Buffer::Write(_unfinishedInputNumbers, buffer, offset);
	Buffer::Write(_undefinedInputNumbers, buffer, offset);
	// positive inputs
	Buffer::WriteSize(_positiveInputs.size(), buffer, offset);
	auto itr = _positiveInputs.begin();
	while (itr != _positiveInputs.end()) {
		Buffer::Write(*itr, buffer, offset);
		itr++;
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
	itr = _undefinedInputs.begin();
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
	Buffer::Write(exitstats.initerror, buffer, offset);
	Buffer::Write(_failureRate, buffer, offset);
	Buffer::Write(_addtestFails, buffer, offset);
	if (_grammar)
		Buffer::Write(_grammar->GetFormID(), buffer, offset);
	else
		Buffer::Write((uint64_t)0, buffer, offset);
	if (_generation.load())
		Buffer::Write(_generation.load()->GetFormID(), buffer, offset);
	else
		Buffer::Write((FormID)0, buffer, offset);
	Buffer::Write(_lastGenerationID, buffer, offset);
	return true;
}

bool SessionData::ReadData0x1(unsigned char* buffer, size_t& offset, size_t /*length*/, LoadResolver* resolver)
{
	_positiveInputNumbers = Buffer::ReadInt64(buffer, offset);
	_negativeInputNumbers = Buffer::ReadInt64(buffer, offset);
	_unfinishedInputNumbers = Buffer::ReadInt64(buffer, offset);
	_undefinedInputNumbers = Buffer::ReadInt64(buffer, offset);
	// positive inputs
	_positiveInputs.clear();
	size_t sizepos = Buffer::ReadSize(buffer, offset);
	for (int64_t i = 0; i < (int64_t)sizepos; i++) {
		_positiveInputs.push_back(Buffer::ReadUInt64(buffer, offset));
	}
	// negative inputs
	_negativeInputs.clear();
	size_t size = Buffer::ReadSize(buffer, offset);
	std::vector<std::pair<FormID, double>> negInp(size);
	FormID tmp = 0;
	for (int64_t i = 0; i < (int64_t)size; i++) {
		tmp = Buffer::ReadUInt64(buffer, offset);
		negInp[i] = { tmp, Buffer::ReadDouble(buffer, offset) };
	}
	resolver->AddLateTask([this, negInp, resolver]() {
		for (int64_t i = 0; i < (int64_t)negInp.size(); i++) {
			auto shared = resolver->ResolveFormID<Input>(negInp[i].first);
			if (shared && shared->derive && shared->test) {
				auto node = std::make_shared<InputNode>();
				node->input = shared;
				node->weight = negInp[i].second;
				node->primary = shared->GetPrimaryScore();
				node->secondary = shared->GetSecondaryScore();
				_negativeInputs.insert(node);
				_topK_primary.insert(node);
				_topK_secondary.insert(node);
			}
		}
	});
	// unfinished inputs
	_unfinishedInputs.clear();
	size = Buffer::ReadSize(buffer, offset);
	std::vector<std::pair<FormID, double>> unfInp(size);
	for (int64_t i = 0; i < (int64_t)size; i++) {
		tmp = Buffer::ReadUInt64(buffer, offset);
		unfInp[i] = { tmp, Buffer::ReadDouble(buffer, offset) };
	}
	resolver->AddLateTask([this, unfInp, resolver]() {
		for (int64_t i = 0; i < (int64_t)unfInp.size(); i++) {
			auto shared = resolver->ResolveFormID<Input>(unfInp[i].first);
			if (shared && shared->derive && shared->test) {
				auto node = std::make_shared<InputNode>();
				node->input = shared;
				node->weight = unfInp[i].second;
				node->primary = shared->GetPrimaryScore();
				node->secondary = shared->GetSecondaryScore();
				_unfinishedInputs.insert(node);
				_topK_primary.insert(node);
				_topK_secondary.insert(node);
				_topK_primary_Unfinished.insert(node);
				_topK_secondary_Unfinished.insert(node);
			}
		}
	});
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

bool SessionData::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
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
			FormID grammarid = Buffer::ReadUInt64(buffer, offset);
			FormID generationid = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, grammarid, generationid]() {
				this->_oracle = resolver->ResolveFormID<Oracle>(Data::StaticFormIDs::Oracle);
				this->_controller = resolver->ResolveFormID<TaskController>(Data::StaticFormIDs::TaskController);
				this->_exechandler = resolver->ResolveFormID<ExecutionHandler>(Data::StaticFormIDs::ExecutionHandler);
				this->_generator = resolver->ResolveFormID<Generator>(Data::StaticFormIDs::Generator);
				this->_grammar = resolver->ResolveFormID<Grammar>(grammarid);
				this->_settings = resolver->ResolveFormID<Settings>(Data::StaticFormIDs::Settings);
				this->_excltree = resolver->ResolveFormID<ExclusionTree>(Data::StaticFormIDs::ExclusionTree);
				this->_generation = resolver->ResolveFormID<Generation>(generationid);
				if (this->_generation.load())
					this->_generationID = _generation.load()->GetFormID();
				// this is redundant and has already been set
				this->data = resolver->_data;
				auto gens = resolver->_data->GetFormArray<Generation>();
				for (auto gen : gens)
					this->_generations.insert_or_assign(gen->GetFormID(), gen);
			});
			_lastGenerationID = Buffer::ReadUInt64(buffer, offset);
			return true;
		}
	default:
		return false;
	}
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
