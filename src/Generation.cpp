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
		logwarn("_activeInputs went into the negative");
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
		if (_ddInputs.erase(input->GetFormID() == 1)) {
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
		logwarn("_activeInputs went into the negative");
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
	if (sources.size() < _sources.size())
		sources.resize(_sources.size());
	for (size_t i = 0; i < _sources.size(); i++)
		sources[i] = _sources[i];
}

std::shared_ptr<Input> Generation::GetRandomSource()
{
	if (HasSources())
	{
		return _sources[_sourcesDistr(randan)];
	} else
		return {};
}

void Generation::AddSource(std::shared_ptr<Input> input)
{
	_sources.push_back(input);
	_sourcesDistr = std::uniform_int_distribution<signed>(0, (uint32_t)_sources.size() - 1);
}

bool Generation::CheckSourceValidity(std::function<bool(std::shared_ptr<Input>)> predicate)
{
	// if all sources are invalid result becomes false
	bool result = true;
	for (auto input : _sources)
		result &= !predicate(input);
	return result;
}

void Generation::GetDDControllers(std::vector<std::shared_ptr<DeltaDebugging::DeltaController>>& controllers)
{
	if (_ddControllers.size() > controllers.size())
		controllers.resize(_ddControllers.size());
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
	_sourcesFlags.clear();
	for (size_t i = 0; i < _sources.size(); i++)
		_sourcesFlags.push_back(std::move(std::make_unique<FlagHolder>(_sources[i], Form::FormFlags::DoNotFree)));
}

void Generation::SetInactive()
{
	_sourcesFlags.clear();
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

	switch (version) {
	case 0x1:
		return size0x1;
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
	return true;
}

bool Generation::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
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
		});
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

}

void Generation::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = true;
	}
}
#pragma endregion
