#include "BufferOperations.h"
#include "Data.h"
#include "Generation.h"
#include "Input.h"
#include "DeltaDebugging.h"

Generation::Generation()
{
	randan = std::mt19937((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
	_sourcesDistr = std::uniform_int_distribution<signed>(0, 0);
}

int64_t Generation::GetSize()
{
	return _size;
}

int64_t Generation::GetGeneratedSize()
{
	return _generatedSize;
}

int64_t Generation::GetDDSize()
{
	return _ddSize;
}

int64_t Generation::GetTargetSize()
{
	return _targetSize;
}

void Generation::SetTargetSize(int64_t size)
{
	std::unique_lock<std::shared_mutex> guard(_lock);
	_targetSize = size;
}

std::pair<bool, int64_t> Generation::CanGenerate()
{
	// this function ensures that no single thread can generate all inputs for this generation
	// i.e. make sure that multithreading is available for input generation
	std::unique_lock<std::shared_mutex> guard(_lock);
	if (_generatedSize < _targetSize) {
		int64_t active = _activeInputs;
		int64_t num = 0;
		// if enough inputs are active return false
		if (_maxActiveInputs - active <= 0)
			return { false, 0 };
		// if the number of inputs we are allowed to start is less than the number we are allowed to generate at a time
		// return the minimum of the ones we may activate or the ones we may generate in this generation
		if (_maxActiveInputs - active < _maxSimultaneousGeneration)
			num = std::min(_maxActiveInputs - active, _targetSize - _generatedSize);
		else
			// either the difference left to generate, or the maxSimultaneousGeneration
			num = _targetSize - _generatedSize > _maxSimultaneousGeneration ? _maxSimultaneousGeneration : _targetSize - _generatedSize;
		_activeInputs += num;
		_generatedSize += num;
		return { true, num };
	} else
		return { false, 0 };
}

void Generation::FailGeneration(int64_t fails)
{
	std::unique_lock<std::shared_mutex> guard(_lock);
	_generatedSize -= fails;
	_activeInputs -= fails;
	if (_activeInputs < 0) {
		_activeInputs = 0;
		//logwarn("_activeInputs went into the negative");
	}
}

void Generation::AddGeneratedInput(std::shared_ptr<Input> input)
{
	if (input) {
		std::unique_lock<std::shared_mutex> guard(_lock);
		_generatedInputs.insert_or_assign(input->GetFormID(), input);
		input->SetGenerationID(GetFormID());
	}
}

bool Generation::RemoveGeneratedInput(std::shared_ptr<Input> input)
{
	if (input) {
		std::unique_lock<std::shared_mutex> guard(_lock);
		// return value is the number of removed elements, i.e. 0 or 1
		if (_generatedInputs.erase(input->GetFormID()) == 1) {
			// reduce number of generate inputs since we removed one
			_generatedSize--;
			input->SetGenerationID(0);
			return true;
		}
		else
			return false;
	}
	return false;
}

void Generation::AddDDInput(std::shared_ptr<Input> input)
{
	if (input)
	{
		std::unique_lock<std::shared_mutex> guard(_lock);
		_ddInputs.insert_or_assign(input->GetFormID(), input);
		input->SetGenerationID(GetFormID());
		_ddSize++;
	}
}

bool Generation::RemoveDDInput(std::shared_ptr<Input> input)
{
	if (input) {
		std::unique_lock<std::shared_mutex> guard(_lock);
		// return value is the number of removed elements, i.e. 0 or 1
		if (_ddInputs.erase(input->GetFormID()) == 1) {
			// reduce number of dd inputs since we removed one
			_ddSize--;
			input->SetGenerationID(0);
			return true;
		}else
			return false;
	}
	return false;
}

void Generation::AddDDController(std::shared_ptr<DeltaDebugging::DeltaController> ddcontroller)
{
	if (ddcontroller)
	{
		std::unique_lock<std::shared_mutex> guard(_lock);
		_ddControllers.insert_or_assign(ddcontroller->GetFormID(), ddcontroller);
	}
}

void Generation::SetInputCompleted()
{
	_activeInputs--;
	if (_activeInputs < 0) {
		_activeInputs = 0;
		//logwarn("_activeInputs went into the negative");
	}
}

void Generation::SetGenerationNumber(int32_t number)
{
	_generationNumber = number;
}

int32_t Generation::GetGenerationNumber()
{
	return _generationNumber;
}

void Generation::SetMaxActiveInputs(int64_t activeInputs)
{
	_maxActiveInputs = activeInputs;
}

int64_t Generation::GetActiveInputs()
{
	return _activeInputs.load();
}

void Generation::SetMaxSimultaneuosGeneration(int64_t generationstep)
{
	_maxSimultaneousGeneration = generationstep;
}

bool Generation::IsActive()
{
	return !(_activeInputs == 0 && _generatedSize >= _targetSize);
}

bool Generation::NeedsGeneration()
{
	return (_generatedSize != _targetSize);
}

void Generation::ResetActiveInputs()
{
	_activeInputs = 0;
}

bool Generation::IsDeltaDebuggingActive()
{
	bool result = false;
	for (auto [_, form] : _ddControllers)
		result |= !form->Finished();
	return result;
}

void Generation::VisitDeltaDebugging(std::function<bool(std::shared_ptr<DeltaDebugging::DeltaController>)> visitor)
{
	for (auto [_, form] : _ddControllers)
	{
		if (visitor(form))
			return;
	}
}

bool Generation::HasSources()
{
	return _sources.size() > 0;
}

int32_t Generation::GetNumberOfSources()
{
	return (int32_t)_sources.size();
}

std::vector<std::shared_ptr<Input>> Generation::GetSources()
{
	return _sources;
}

void Generation::GetSources(std::vector<std::shared_ptr<Input>>& sources)
{
	while (_sourcesFlag.test_and_set(std::memory_order_acquire))
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
		_sourcesFlag.wait(true, std::memory_order_relaxed)
#endif
			;

	if (sources.size() < _sources.size())
		sources.resize(_sources.size());
	for (size_t i = 0; i < _sources.size(); i++)
		sources[i] = _sources[i];

	// exit flag
	_sourcesFlag.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
	_sourcesFlag.notify_one();
#endif
}

std::shared_ptr<Input> Generation::GetRandomSource()
{
	if (HasSources())
	{
		// see cppreference for this section (source)
		
		// since the randomness will even out over all inputs due to gleichverteilung, just use an iterator
		while (_sourcesFlag.test_and_set(std::memory_order_acquire))
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
			_sourcesFlag.wait(true, std::memory_order_relaxed)
#endif
				;

		// do stuff
		/*std::vector<std::shared_ptr<Input>>::iterator itr = _sources.end();
		size_t count = _sources.size();
		while (count > 0) {
			if (_sourcesIter != _sources.end()) {
				if ((_maxDerivedFailingInputs == 0 || (*_sourcesIter)->GetDerivedFails() < _maxDerivedFailingInputs) && 
					(_maxDerivedInputs == 0 || (*_sourcesIter)->GetDerivedInputs() < _maxDerivedInputs)) {
					itr = _sourcesIter++;
					break;
				} else
					_sourcesIter++;
			} else
				_sourcesIter = _sources.begin();
			count--;
		}*/
		int32_t idx = -1;
		size_t count = _sources.size();
		while (count > 0)
		{
			if (_sourcesIter < _sources.size())
			{
				if ((_maxDerivedFailingInputs == 0 || _sources[_sourcesIter]->GetDerivedFails() < _maxDerivedFailingInputs) &&
					(_maxDerivedInputs == 0 || _sources[_sourcesIter]->GetDerivedInputs() < _maxDerivedInputs)) {
					idx = _sourcesIter++;
					break;
				} else
					_sourcesIter++;
			} else
				_sourcesIter = 0;
			count--;
		}

		// exit flag
		_sourcesFlag.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
		_sourcesFlag.notify_one();
#endif
		//if (itr != _sources.end()) {
			//logmessage("ID: {}", Utility::GetHex((*itr)->GetFormID()));
			//return *itr;
		if (idx != -1) {
			//logmessage("ID: {}", Utility::GetHex(_sources[idx]->GetFormID()));
			return _sources[idx];
		}
		else
			return {};
	} else
		return {};
}

void Generation::AddSource(std::shared_ptr<Input> input)
{
	if (input) {
		// see cppreference for this section (source)

		// since the randomness will even out over all inputs due to gleichverteilung, just use an iterator
		while (_sourcesFlag.test_and_set(std::memory_order_acquire))
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
			_sourcesFlag.wait(true, std::memory_order_relaxed)
#endif
				;

		// do stuff
		_sources.resize(_sources.size() + 1);
		_sources[_sources.size() -1] = input;
		_sourcesDistr = std::uniform_int_distribution<signed>(0, (uint32_t)_sources.size() - 1);
		//_sourcesIter = _sources.begin();

		// exit flag
		_sourcesFlag.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
		_sourcesFlag.notify_one();
#endif
	}
}

bool Generation::CheckSourceValidity(std::function<bool(std::shared_ptr<Input>)> predicate)
{
	if (_sources.size() == 0)
		return true;
	// if all sources are invalid result becomes false
	
	// see cppreference for this section (source)

	// since the randomness will even out over all inputs due to gleichverteilung, just use an iterator
	while (_sourcesFlag.test_and_set(std::memory_order_acquire))
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
		_sourcesFlag.wait(true, std::memory_order_relaxed)
#endif
			;
	bool result = true;
	for (auto input : _sources)
		result |= !predicate(input);

	// exit flag
	_sourcesFlag.clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
	_sourcesFlag.notify_one();
#endif
	return result;
}

void Generation::GetDDControllers(std::vector<std::shared_ptr<DeltaDebugging::DeltaController>>& controllers)
{
	if (_ddControllers.size() > controllers.size())
		controllers.resize(_ddControllers.size());
	std::unique_lock<std::shared_mutex> guard(_lock);
	auto itr = _ddControllers.begin();
	int i = 0;
	while (itr != _ddControllers.end())
	{
		controllers[i] = itr->second;
		i++;
		itr++;
	}
}

size_t Generation::GetNumberOfDDControllers()
{
	return _ddControllers.size();
}

void Generation::SetActive()
{
	for (size_t i = 0; i < _sources.size(); i++) {
		if (_sources[i]) {
			_sources[i]->SetFlag(Form::FormFlags::DoNotFree);
			if (_sources[i]->derive)
				_sources[i]->derive->SetFlag(Form::FormFlags::DoNotFree);
		}
	}
}

void Generation::SetInactive()
{
	for (size_t i = 0; i < _sources.size(); i++) {
		if (_sources[i]) {
			_sources[i]->UnsetFlag(Form::FormFlags::DoNotFree);
			if (_sources[i]->derive)
				_sources[i]->derive->UnsetFlag(Form::FormFlags::DoNotFree);
		}
	}
}

bool Generation::CheckOracleResultAndLength(std::shared_ptr<Input>& input, bool allowFailing, size_t min_length_unfinished, size_t min_length_failing)
{
	if (input && ((allowFailing && input->GetOracleResult() == OracleResult::Failing && input->Length() >= min_length_failing) ||
					 (input->GetOracleResult() == OracleResult::Unfinished && input->Length() >= min_length_unfinished)))
		return true;
	else
		return false;
}

void Generation::SetMaxDerivedFailingInput(uint64_t maxDerivedFailingInputs)
{
	_maxDerivedFailingInputs = maxDerivedFailingInputs;
}
void Generation::SetMaxDerivedInput(uint64_t maxDerivedInputs)
{
	_maxDerivedInputs = maxDerivedInputs;
}

#pragma region FORM

size_t Generation::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4                     // version
	                        + 8                     // size
	                        + 8                     // generatedSize
	                        + 8                     // ddSize
	                        + 8                     // targetSize
	                        + 8                     // maxSimiltaneousGeneration
	                        + 8                     // activeInputs
	                        + 8                     // maxActiveInputs
	                        + 4;                    // generationNumber
	static size_t size0x2 = size0x1  // old size
	                        + 8      // maxDerivedFailingInputs
	                        + 8;     // maxDerivedInputs
	switch (version) {
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	default:
		return 0;
	}
}

size_t Generation::GetDynamicSize()
{
	return Form::GetDynamicSize()             // form stuff
	       + GetStaticSize(classversion)      // static size
	       + 8 + 8 * _generatedInputs.size()  // sizeof() + formids in generatedInputs
	       + 8 + 8 * _ddInputs.size()         // sizeof() + formids in ddInputs
	       + 8 + 8 * _ddControllers.size()    // sizeof() + formids in _ddControllers
	       + 8 + 8 * _sources.size();         // sizeof() + formids in _sources
}

bool Generation::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(_size, buffer, offset);
	Buffer::Write(_generatedSize, buffer, offset);
	Buffer::Write(_ddSize, buffer, offset);
	Buffer::Write(_targetSize, buffer, offset);
	Buffer::Write(_maxSimultaneousGeneration, buffer, offset);
	Buffer::Write(_activeInputs, buffer, offset);
	Buffer::Write(_maxActiveInputs, buffer, offset);
	Buffer::Write(_generationNumber, buffer, offset);
	// dynamic stuff
	// _generatedInputs
	Buffer::WriteSize(_generatedInputs.size(), buffer, offset);
	for (auto& [id, _] : _generatedInputs)
		Buffer::Write(id, buffer, offset);
	// _ddInputs
	Buffer::WriteSize(_ddInputs.size(), buffer, offset);
	for (auto& [id, _] : _ddInputs)
		Buffer::Write(id, buffer, offset);
	// _ddControllers
	Buffer::WriteSize(_ddControllers.size(), buffer, offset);
	for (auto& [id, _] : _ddControllers)
		Buffer::Write(id, buffer, offset);
	// _sources
	Buffer::WriteSize(_sources.size(), buffer, offset);
	for (auto& input : _sources)
		if (input)
			Buffer::Write(input->GetFormID(), buffer, offset);
		else
			Buffer::Write((FormID)0, buffer, offset);
	Buffer::Write(_maxDerivedFailingInputs, buffer, offset);
	Buffer::Write(_maxDerivedInputs, buffer, offset);
	return true;
}

bool Generation::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x2:
	case 0x1:
		Form::ReadData(buffer, offset, length, resolver);
		_size = Buffer::ReadInt64(buffer, offset);
		_generatedSize = Buffer::ReadInt64(buffer, offset);
		_ddSize = Buffer::ReadInt64(buffer, offset);
		_targetSize = Buffer::ReadInt64(buffer, offset);
		_maxSimultaneousGeneration = Buffer::ReadInt64(buffer, offset);
		_activeInputs = Buffer::ReadInt64(buffer, offset);
		_maxActiveInputs = Buffer::ReadInt64(buffer, offset);
		_generationNumber = Buffer::ReadInt32(buffer, offset);
		// _generatedInputs
		size_t size = Buffer::ReadSize(buffer, offset);
		std::vector<FormID> genInp;
		for (int64_t i = 0; i < (int64_t)size; i++)
			genInp.push_back(Buffer::ReadUInt64(buffer, offset));
		// _ddInputs
		size = Buffer::ReadSize(buffer, offset);
		std::vector<FormID> ddInp;
		for (int64_t i = 0; i < (int64_t)size; i++)
			ddInp.push_back(Buffer::ReadUInt64(buffer, offset));
		// _ddControllers
		size = Buffer::ReadSize(buffer, offset);
		std::vector<FormID> ddCon;
		for (int64_t i = 0; i < (int64_t)size; i++)
			ddCon.push_back(Buffer::ReadUInt64(buffer, offset));
		// _sources
		size = Buffer::ReadSize(buffer, offset);
		std::vector<FormID> sc;
		for (int64_t i = 0; i < (int64_t)size; i++)
			sc.push_back(Buffer::ReadUInt64(buffer, offset));
		resolver->AddTask([this, genInp, ddInp, ddCon, sc, resolver]() {
			for (int64_t i = 0; i < (int64_t)genInp.size(); i++)
			{
				auto shared = resolver->ResolveFormID<Input>(genInp[i]);
				if (shared)
					_generatedInputs.insert_or_assign(shared->GetFormID(), shared);
			}
			for (int64_t i = 0; i < (int64_t)ddInp.size(); i++) {
				auto shared = resolver->ResolveFormID<Input>(ddInp[i]);
				if (shared)
					_ddInputs.insert_or_assign(shared->GetFormID(), shared);
			}
			for (int64_t i = 0; i < (int64_t)ddCon.size(); i++) {
				auto shared = resolver->ResolveFormID<DeltaDebugging::DeltaController>(ddCon[i]);
				if (shared)
					_ddControllers.insert_or_assign(shared->GetFormID(), shared);
			}
			for (int64_t i = 0; i < (int64_t)sc.size(); i++)
			{
				auto shared = resolver->ResolveFormID<Input>(sc[i]);
				if (shared)
					_sources.push_back(shared);
			}
			if (_sources.size() > 0)
				_sourcesDistr = std::uniform_int_distribution<signed>(0, (uint32_t)_sources.size() - 1);
			//_sourcesIter = _sources.begin();
			_sourcesIter = 0;
		});
		if (version == 0x2) {
			_maxDerivedFailingInputs = Buffer::ReadUInt64(buffer, offset);
			_maxDerivedInputs = Buffer::ReadUInt64(buffer, offset);
		}
		return true;
	}
	return false;
}

void Generation::Delete(Data*)
{
	Clear();
}

void Generation::Clear()
{
	Form::ClearForm();
	_size = 0;
	_generatedSize = 0;
	_ddSize = 0;
	_targetSize = 0;
	_maxSimultaneousGeneration = 0;
	_maxDerivedFailingInputs = 0;
	_activeInputs = 0;
	_generatedInputs.clear();
	_ddInputs.clear();
	_ddControllers.clear();
	_sources.clear();
	//_sourcesIter = _sources.end();
	_sourcesIter = (int32_t)_sources.size();
	_sourcesDistr = std::uniform_int_distribution<signed>(0, 0);
	_generationNumber = 0;
}

void Generation::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = true;
	}
}

size_t Generation::MemorySize()
{
	return sizeof(Generation) + _sources.size() * sizeof(std::shared_ptr<Input>) + _ddControllers.size() * sizeof(std::pair<FormID, std::shared_ptr<DeltaDebugging::DeltaController>>) + _ddInputs.size() * sizeof(std::pair<FormID, std::shared_ptr<Input>>) + _generatedInputs.size() * sizeof(std::pair<FormID, std::shared_ptr<Input>>);
}

#pragma endregion
