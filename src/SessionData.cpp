#include "SessionData.h"
#include "Settings.h"
#include "BufferOperations.h"
#include "Data.h"

#include <exception>
#include <stdexcept>
#include <memory>

SessionData::~SessionData()
{
	Clear();
}

void SessionData::Clear()
{
	//throw std::runtime_error("SessionData::Clear not implemented");
}

void SessionData::AddInput(std::shared_ptr<Input>& input, EnumType list, double optionalweight)
{
	if (!input)
		return;
	switch (list) {
	case Oracle::OracleResult::Failing:
		{
			std::shared_ptr<InputNode> node = std::make_shared<InputNode>();
			node->input = input;
			node->weight = optionalweight;
			node->primary = input->GetPrimaryScore();
			node->secondary = input->GetSecondaryScore();
			std::unique_lock<std::shared_mutex> guard(_negativeInputsLock);
			_negativeInputs.insert(node);
		}
		break;
	case Oracle::OracleResult::Passing:
		{
			std::unique_lock<std::shared_mutex> guard(_positiveInputsLock);
			_positiveInputs.push_back(input->GetFormID());
		}
		break;
	case Oracle::OracleResult::Undefined:
		{
			std::unique_lock<std::shared_mutex> guard(_undefinedInputLock);
			_undefinedInputs.push_back(input->GetFormID());
		}
		break;
	case Oracle::OracleResult::Unfinished:
		{
			std::shared_ptr<InputNode> node = std::make_shared<InputNode>();
			node->input = input;
			node->weight = optionalweight;
			node->primary = input->GetPrimaryScore();
			node->secondary = input->GetSecondaryScore();
			std::unique_lock<std::shared_mutex> guard(_unfinishedInputsLock);
			_unfinishedInputs.insert(node);
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
	                     + 8;  // lastchecks
	static size_t size0x2 = size0x1  // size of existing stuff from version 1
	                        + 8      // exitstats.natural
	                        + 8      // exitstats.lastinput
	                        + 8      // exitstats.terminated
	                        + 8      // exitstats.timeout
	                        + 8      // exitstats.fragmenttimeout
	                        + 8      // exitstats.memory
	                        + 8      // exitstats.initerror
	                        + 8      // failureRate
	                        + 8;     // addTestFails

	switch (version) {
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
	return GetStaticSize(classversion) +    // static class size
	       _positiveInputs.size() * 8       // list elements are uint64_t
	       + _negativeInputs.size() * 16    // two list elements: uint64_t, double
	       + _unfinishedInputs.size() * 16  // two list elements: uint64_t, double
	       + _undefinedInputs.size() * 8    // list elements are uint64_t
	       + _recentfailes.size();          // actual number of elements in the list * size of byte
}

std::vector<std::shared_ptr<Input>> SessionData::GetTopK(int32_t k)
{
	std::set<std::shared_ptr<InputNode>, InputNodeLess> set;
	std::vector<std::shared_ptr<Input>> ret;
	int32_t count = 0;
	auto itr = _unfinishedInputs.begin();
	while (count < k && itr != _unfinishedInputs.end())
	{
		set.insert(*itr);
		itr++;
		count++;
	}
	count = 0;
	itr = _negativeInputs.begin();
	while (count < k && itr != _negativeInputs.end()) {
		set.insert(*itr);
		itr++;
		count++;
	}
	count = 0;
	itr = set.begin();
	while (count < k && itr != set.end()) {
		ret.push_back((*itr)->input.lock());
		itr++;
		count++;
	}
	return ret;
}

bool SessionData::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
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
	Buffer::Write(lastchecks, buffer, offset);
	// -----VERSION 2-----
	Buffer::Write(exitstats.natural, buffer, offset);
	Buffer::Write(exitstats.lastinput, buffer, offset);
	Buffer::Write(exitstats.terminated, buffer, offset);
	Buffer::Write(exitstats.timeout, buffer, offset);
	Buffer::Write(exitstats.fragmenttimeout, buffer, offset);
	Buffer::Write(exitstats.memory, buffer, offset);
	Buffer::Write(exitstats.initerror, buffer, offset);
	Buffer::Write(_failureRate, buffer, offset);
	Buffer::Write(_addtestFails, buffer, offset);
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
	resolver->AddTask([&_negInputs = _negativeInputs, negInp, resolver]() {
		for (int64_t i = 0; i < (int64_t)negInp.size(); i++) {
			auto shared = resolver->ResolveFormID<Input>(negInp[i].first);
			if (shared) {
				auto node = std::make_shared<InputNode>();
				node->input = shared;
				node->weight = negInp[i].second;
				node->primary = shared->GetPrimaryScore();
				node->secondary = shared->GetSecondaryScore();
				_negInputs.insert(node);
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
	resolver->AddTask([&_unfInputs = _unfinishedInputs, unfInp, resolver]() {
		for (int64_t i = 0; i < (int64_t)unfInp.size(); i++) {
			auto shared = resolver->ResolveFormID<Input>(unfInp[i].first);
			if (shared) {
				auto node = std::make_shared<InputNode>();
				node->input = shared;
				node->weight = unfInp[i].second;
				node->primary = shared->GetPrimaryScore();
				node->secondary = shared->GetSecondaryScore();
				_unfInputs.insert(node);
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
	lastchecks = Buffer::ReadTime(buffer, offset);
	return true;
}

bool SessionData::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		return ReadData0x1(buffer, offset, length, resolver);
	case 0x2:
		{
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
		}
	default:
		return false;
	}
}
