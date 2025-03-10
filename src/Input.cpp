
#include <filesystem>
#include <boost/container_hash/hash.hpp>

#include "Input.h"
#include "Logging.h"
#include "ExecutionHandler.h"
#include "BufferOperations.h"
#include "Data.h"
#include "Test.h"

#pragma region InheritedForm

size_t Input::GetStaticSize(int32_t version)
{
	static size_t size0x1 = 4 +                 // version
	                        1 +                 // _hasfinished
	                        1 +                 // _trimmed
	                        8 +                 // _executiontime
	                        4 +                 // _exitcode
	                        sizeof(EnumType) +  // _oracleResult
	                        8 +                 // test
	                        8 +                 // derivationtree
	                        8 +                 // _primaryScore
	                        8 +                 // _secondaryScore
	                        8                   // trimmedLength
	                        + 8                 // _parent.parentInput
	                        + 8                 // _parent.positionBegin
	                        + 8                 // _parent.length
	                        + 1                 // _parent.complement
	                        + 8                 // generationID
	                        + 8                 // derivedInputs
	                        + 8;                // generationTime
	static size_t size0x2 = 4 +                 // version
	                        1 +                 // _hasfinished
	                        1 +                 // _trimmed
	                        8 +                 // _executiontime
	                        4 +                 // _exitcode
	                        sizeof(EnumType) +  // _oracleResult
	                        8 +                 // test
	                        8 +                 // derivationtree
	                        8 +                 // _primaryScore
	                        8 +                 // _secondaryScore
	                        8                   // trimmedLength
	                        + 8                 // _parent.parentInput
	                        + 1                 // _parent.complement
	                        + 8                 // generationID
	                        + 8                 // derivedInputs
	                        + 8                 // generationTime
	                        + 1                 // enablePrimaryScoreIndividual
	                        + 1                 // enableSecondaryScoreIndividual
	                        + 8;                // derivedFails

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

size_t Input::GetDynamicSize()
{
	size_t size = Form::GetDynamicSize()                                            // form stuff
	              + GetStaticSize(classversion)                                     // static stuff
	              + 8 + _parent.segments.size() * 16                                // sizeof(), parent.segments content 16 Bytes
	              + Buffer::VectorBasic::GetVectorSize(_primaryScoreIndividual)     // primaryScoreIndividual
	              + Buffer::VectorBasic::GetVectorSize(_secondaryScoreIndividual);  // secondaryScoreIndividual
	size += Buffer::CalcStringLength(_stringrep);
	if (HasFlag(Form::FormFlags::DoNotFree)) {  // if do not free flag is present this input is needed for something and won't be checked for regeneration
		size += Buffer::List::GetListLength(_sequence);
		size += Buffer::List::GetListLength(_orig_sequence);
	}
	return size;
}

bool Input::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write(_hasfinished, buffer, offset);
	Buffer::Write(_trimmed, buffer, offset);
	Buffer::Write(_executiontime, buffer, offset);
	Buffer::Write(_exitcode, buffer, offset);
	Buffer::Write((uint64_t)_oracleResult, buffer, offset);
	if (test) {
		Buffer::Write(test->GetFormID(), buffer, offset);
	} else
		Buffer::Write((FormID)0, buffer, offset);
	if (derive) {
		Buffer::Write(derive->GetFormID(), buffer, offset);
	} else
		Buffer::Write((FormID)0, buffer, offset);
	Buffer::Write(_stringrep, buffer, offset);
	if (HasFlag(Form::FormFlags::DoNotFree)) {
		Buffer::List::WriteList(_sequence, buffer, offset);
		Buffer::List::WriteList(_orig_sequence, buffer, offset);
	}
	Buffer::Write(_primaryScore, buffer, offset);
	Buffer::Write(_secondaryScore, buffer, offset);
	Buffer::Write(_trimmedlength, buffer, offset);
	Buffer::Write(_parent.parentInput, buffer, offset);
	Buffer::WriteSize(_parent.segments.size(), buffer, offset);
	for (auto [begin, length] : _parent.segments) {
		Buffer::Write(begin, buffer, offset);
		Buffer::Write(length, buffer, offset);
	}
	Buffer::Write(_parent.complement, buffer, offset);
	Buffer::Write(_generationID, buffer, offset);
	Buffer::Write(_derivedInputs, buffer, offset);
	Buffer::Write(_generationTime, buffer, offset);
	Buffer::Write(_enablePrimaryScoreIndividual, buffer, offset);
	Buffer::Write(_enableSecondaryScoreIndividual, buffer, offset);
	Buffer::VectorBasic::WriteVector(_primaryScoreIndividual, buffer, offset);
	Buffer::VectorBasic::WriteVector(_secondaryScoreIndividual, buffer, offset);
	Buffer::Write(_derivedFails, buffer, offset);
	return true;
}

bool Input::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	//logdebug("ReadData");
	size_t initoff = offset;
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			if (length < GetStaticSize(version))
				return false;
			Form::ReadData(buffer, offset, length, resolver);
			// static init
			_pythonconverted = false;
			_pythonstring = "";
			// end
			_hasfinished = Buffer::ReadBool(buffer, offset);
			_trimmed = Buffer::ReadBool(buffer, offset);
			_executiontime = Buffer::ReadNanoSeconds(buffer, offset);
			_exitcode = Buffer::ReadInt32(buffer, offset);
			_oracleResult = Buffer::ReadUInt64(buffer, offset);
			FormID testid = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, testid]() {
				this->test = resolver->ResolveFormID<Test>(testid);
			});
			FormID deriveid = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, deriveid]() {
				this->derive = resolver->ResolveFormID<DerivationTree>(deriveid);
			});
			//logdebug("ReadData string rep");
			// get _stringrep
			//if (length <= offset - initoff + 8 || length <= offset - initoff + 8 + Buffer::CalcStringLength(buffer, offset))
			//	return false;
			_stringrep = Buffer::ReadString(buffer, offset);
			if (HasFlag(Form::FormFlags::DoNotFree)) {
				Buffer::List::ReadList(_sequence, buffer, offset);
				Buffer::List::ReadList(_orig_sequence, buffer, offset);
			}
			_primaryScore = Buffer::ReadDouble(buffer, offset);
			_secondaryScore = Buffer::ReadDouble(buffer, offset);
			_trimmedlength = Buffer::ReadInt64(buffer, offset);
			_parent.parentInput = Buffer::ReadUInt64(buffer, offset);
			_parent.segments.clear();
			auto positionBegin = Buffer::ReadInt64(buffer, offset);
			_parent.segments.push_back({ positionBegin, Buffer::ReadInt64(buffer, offset) });
			_parent.complement = Buffer::ReadBool(buffer, offset);
			_generationID = Buffer::ReadUInt64(buffer, offset);
			_derivedInputs = Buffer::ReadUInt64(buffer, offset);
			_generationTime = Buffer::ReadNanoSeconds(buffer, offset);
		}
		return true;
	case 0x2:
		{
			if (length < GetStaticSize(version))
				return false;
			Form::ReadData(buffer, offset, length, resolver);
			// static init
			_pythonconverted = false;
			_pythonstring = "";
			// end
			_hasfinished = Buffer::ReadBool(buffer, offset);
			_trimmed = Buffer::ReadBool(buffer, offset);
			_executiontime = Buffer::ReadNanoSeconds(buffer, offset);
			_exitcode = Buffer::ReadInt32(buffer, offset);
			_oracleResult = Buffer::ReadUInt64(buffer, offset);
			FormID testid = Buffer::ReadUInt64(buffer, offset);
			if (testid == 0) {
				logwarn("Test ID invalid");
			} else {
				resolver->AddTask([this, resolver, testid]() {
					this->test = resolver->ResolveFormID<Test>(testid);
					if (!this->test)
						logwarn("Test not found");
				});
			}
			FormID deriveid = Buffer::ReadUInt64(buffer, offset);
			resolver->AddTask([this, resolver, deriveid]() {
				this->derive = resolver->ResolveFormID<DerivationTree>(deriveid);
				if (!derive)
					logwarn("Cannot find DerivationTree: {}", deriveid);
			});
			//logdebug("ReadData string rep");
			// get _stringrep
			//if (length <= offset - initoff + 8 || length <= offset - initoff + 8 + Buffer::CalcStringLength(buffer, offset))
			//	return false;
			_stringrep = Buffer::ReadString(buffer, offset);
			if (HasFlag(Form::FormFlags::DoNotFree)) {
				Buffer::List::ReadList(_sequence, buffer, offset);
				Buffer::List::ReadList(_orig_sequence, buffer, offset);
			}
			_primaryScore = Buffer::ReadDouble(buffer, offset);
			_secondaryScore = Buffer::ReadDouble(buffer, offset);
			_trimmedlength = Buffer::ReadInt64(buffer, offset);
			_parent.parentInput = Buffer::ReadUInt64(buffer, offset);
			_parent.segments.clear();
			size_t len = Buffer::ReadSize(buffer, offset);
			for (size_t i = 0; i < len; i++) {
				auto positionBegin = Buffer::ReadInt64(buffer, offset);
				_parent.segments.push_back({ positionBegin, Buffer::ReadInt64(buffer, offset) });
			}
			_parent.complement = Buffer::ReadBool(buffer, offset);
			_generationID = Buffer::ReadUInt64(buffer, offset);
			_derivedInputs = Buffer::ReadUInt64(buffer, offset);
			_generationTime = Buffer::ReadNanoSeconds(buffer, offset);
			_enablePrimaryScoreIndividual = Buffer::ReadBool(buffer, offset);
			_enableSecondaryScoreIndividual = Buffer::ReadBool(buffer, offset);
			_primaryScoreIndividual = Buffer::VectorBasic::ReadVector<double>(buffer, offset);
			_secondaryScoreIndividual = Buffer::VectorBasic::ReadVector<double>(buffer, offset);
			_derivedFails = Buffer::ReadUInt64(buffer, offset);
		}
		return true;
	default:
		return false;
	}
}

void Input::Delete(Data* /*data*/)
{
	Clear();
}

bool Input::CanDelete(Data*)
{
	if (_derivedInputs > 0)
		return false;
	else
		return true;
}

#pragma endregion

Input::Input()
{
	_oracleResult = OracleResult::Running;
}

Input::~Input()
{
	Clear();
}

void Input::Clear()
{
	Form::ClearForm();
	//_hasfinished = false;
	//_trimmed = false;
	//_trimmedlength = -1;
	//_executiontime = std::chrono::nanoseconds(0);
	//_exitcode = 0;
	//_primaryScore = 0.0f;
	//_secondaryScore = 0.0f;
	_primaryScoreIndividual.clear();
	_secondaryScoreIndividual.clear();
	//_enablePrimaryScoreIndividual = false;
	//_enableSecondaryScoreIndividual = false;
	_pythonstring = "";
	//_pythonstring.shrink_to_fit();
	//_pythonconverted = false;
	//_generatedSequence = false;
	//_parent = {};
	//_generationID = 0;
	//_derivedFails = 0;
	//_derivedInputs = 0;
	//_generationTime = std::chrono::nanoseconds(0);
	test.reset();
	derive.reset();
	_stringrep = "";
	//_stringrep.shrink_to_fit();
	_oracleResult = OracleResult::Running;
	_sequence.clear();
	_orig_sequence.clear();
	_lua_sequence_next = _sequence.end();
}

void Input::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}

std::string Input::ConvertToPython(bool update)
{
	// if we already generated the python commands and aren't forces to update, just return the last one
	if (!update && _pythonconverted)
		return _pythonstring;
	_pythonstring = "[ ";
	auto itr = begin();
	if (itr != end())
	{
		// first one doesn't have command
		_pythonstring += (*itr);
		itr++;
	}
	while (itr != end()) {
		_pythonstring += ", " + (*itr);
		itr++;
	}
	_pythonstring += " ]";
	_pythonconverted = true;
	return _pythonstring;
}

size_t Input::Length()
{
	if (_sequence.size() == 0)
	{
		if (_trimmedlength > 0)
			return _trimmedlength;
		if (_enablePrimaryScoreIndividual)
			return _primaryScoreIndividual.size();
		if (derive)
		{
			if (derive->_valid)
				return derive->_sequenceNodes;
			else
			{
				if (derive->_parent._parentID != 0) {
					if (derive->_parent._complement)
						return derive->_parent._stop - derive->_parent._length;
					else
						// this is either DD: length has value, targetlen = 0
						// or extended: length has parent length, targetlen has this length
						return derive->_parent._length + derive->_targetlen;
				}
				else
				{
					return derive->_targetlen;
				}
			}
		}
	}
	return _sequence.size();
}

size_t Input::GetSequenceLength()
{
	return _sequence.size();
}

std::string& Input::operator[](size_t index)
{
	auto itr = begin();
	size_t pos = 0;
	while (itr != end())
	{
		if (pos == index)
			return *itr;
		pos++;
		itr++;
	}
	throw std::out_of_range("");
}

void Input::AddEntry(std::string entry)
{
	_sequence.push_back(entry);
}

std::string Input::ToString()
{
	std::string str = "[ ";
	int32_t count = 0;
	for (auto& s : _sequence)
	{
		count++;
		if (count == _sequence.size())
			str += "\"" + s + "\"";
		else
			str += "\"" + s + "\", ";
	}
	str += " ]";
	return str;
}

std::list<std::string>::iterator Input::begin()
{
	return _sequence.begin();
}

std::list<std::string>::iterator Input::end()
{
	return _sequence.end();
}

std::size_t Input::Hash()
{
	return boost::hash_range(_sequence.begin(), _sequence.end());
}

std::vector<std::shared_ptr<Input>> Input::ParseInputs(std::filesystem::path path)
{
	// check whether the path exists and whether its actually a file
	if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
		logwarn("Cannot read inputs from file: {}. Either the path does not exist or refers to a directory.", path.string());
		return {};
	}
	// open filestream
	std::ifstream _stream;
	try {
		_stream = std::ifstream(path, std::ios_base::in);
	} catch (std::exception& e) {
		logwarn("Cannot read inputs from file: {}. The file cannot be accessed with error message: {}", path.string(), e.what());
		return {};
	}
	if (_stream.is_open()) {
		/* auto CountSymbols = [](std::string str, char symbol, char escaped1, char escaped2) {
			int32_t count = 0;
			bool escape = false;
			for (char& c : str) {
				// escaped strings are marked by, e.g., " or ' and everything between two of them should not be counted
				// we need to check for both since one string could be marked with ", while another one could use '
				// this means that something thats invalid in python "usdzfgf' is counted as valid.
				if (c == escaped1 || c == escaped2)
					escape = !escape;
				if (escape == false)
					if (c == symbol)
						count++;
			}
			return count;
		};*/

		// parse file for a variable named inputs
		std::string contents;
		int32_t open = 0;
		int32_t closed = 0;
		std::string line;
		while (std::getline(_stream, line)) {
			// we read another line
			// check if its empty or with a comment
			if (line.empty())
				continue;
			// remove leading spaces and tabs
			while (line.length() > 0 && (line[0] == ' ' || line[0] == '\t')) {
				line = line.substr(1, line.length() - 1);
			}
			// check again
			if (line.length() == 0)
				continue;
			// now check whether the beginning of the line matches "inputs = ["
			if (auto pos = line.find("inputs = ["); pos != std::string::npos && pos == 0) {
				contents += line.substr(pos + 8, line.size() - (pos + 8));
				// count the number of opening and closing brackets, and add lines until it matches
				open += Utility::CountSymbols(line, '[', '\'', '\"');
				closed += Utility::CountSymbols(line, ']', '\'', '\"');
				while (open != closed && std::getline(_stream, line)) {
					contents += line;
					open += Utility::CountSymbols(line, '[', '\'', '\"');
					closed += Utility::CountSymbols(line, ']', '\'', '\"');
				}
				if (open != closed) {
					logwarn("Cannot read inputs from file: {}. Syntax error.", path.string());
					return {};
				} else {
					// open == closed
					// we have found our complete inuput, now onto parse it and extract the _input sequences
					std::vector<std::shared_ptr<Input>> inputs;
					std::shared_ptr<Input> input(nullptr);

					struct record
					{
						int32_t open = 0;
						bool escaped = false;
						std::string current;
						bool brk = false;
					};
					record rec;
					std::string worked = "";

					for (char c : contents) {
						worked += c;
						if (rec.open == 0) {
							switch (c) {
							case '[':
								rec.open++;
								break;
							}
						} else if (rec.open == 1) {
							switch (c) {
							case '[':
								// we are entering an _input definition
								rec.open++;
								input = std::make_shared<Input>();
								break;
							case ']':
								// we are done with all inputs
								rec.open--;
								rec.brk = true;
								break;
							}
						} else if (rec.open == 2) {
							// we are currently parsing an _input entry
							if (rec.escaped == true) {
								switch (c) {
								case '\'':  // fallthrough intended
								case '\"':
									// escaped _sequence has ended. Add current as entry to the _input and reset current
									rec.escaped = false;
									input->AddEntry(rec.current);
									rec.current = "";
									break;
								default:
									// add c to current
									rec.current += c;
									break;
								}
							} else {
								switch (c) {
								case '[':
									logwarn("Cannot read inputs from file: {}. Syntax Error: 3-dimensional arrays are not supported.", path.string());
									return {};
								case ']':
									// closing one _input
									inputs.push_back(input);
									input.reset();
									rec.open--;
									break;
								case '\'':  // fallthrough intended
								case '\"':
									// escaped _sequence has begun
									rec.escaped = true;
									break;
								}
							}
						}
						if (rec.brk == true)
							break;
					}
					if (rec.open != 0 || rec.escaped == true) {
						logwarn("Cannot read inputs from file: {}. Encountered Syntax error while parsing inputs.", path.string());
						return {};
					}
					// successfully parsed the file
					// return all found inputs
					return inputs;
				}
			}
		}
		logwarn("Cannot read inputs from file: {}. Could not find the definition of inputs.", path.string());
		return {};

	} else {
		logwarn("Cannot read inputs from file: {}. Could not open file.", path.string());
		return {};
	}
}

void Input::DeepCopy(std::shared_ptr<Input> other)
{
	other->derive = derive;
	other->_orig_sequence = _orig_sequence;
	other->_lua_sequence_next = _lua_sequence_next;
	other->_sequence = _sequence;
	other->_oracleResult = _oracleResult;
	other->_stringrep = _stringrep;
	other->_generationID = _generationID;
	other->_parent = _parent;
	other->_pythonconverted = _pythonconverted;
	other->_pythonstring = _pythonstring;
	other->_secondaryScore = _secondaryScore;
	other->_primaryScore = _primaryScore;
	other->_exitcode = _exitcode;
	other->_executiontime = _executiontime;
	other->_trimmedlength = _trimmedlength;
	other->_trimmed = _trimmed;
	other->_hasfinished = _hasfinished;
	other->_flags = _flags;
}

void Input::FreeMemory()
{
	if (TryLock()) {
		// return if the DoNotFree flag is set
		if (HasFlag(Form::FormFlags::DoNotFree)) {
			Form::Unlock();
			return;
		}
		if (!derive)
			logcritical("Input is missing dev tree");
		if (derive && derive->GetRegenerate() == false)
			logcritical("Input cannot be regenerated");
		// if _input is generated AND the test has already been run and the _callback has been invalidated
		// (i.e. we have evaluated oracle and stuff)
		// we can destroy the derivation tree and clear the _sequence to reclaim
		// memory space
		if (_generatedSequence && test && test->IsValid() == false && !test->_callback) {
			_generatedSequence = false;

			if (derive)
				derive->FreeMemory();

			_sequence.clear();
			_orig_sequence.clear();
		}
		// reset python string
		_pythonconverted = false;
		_pythonstring = "";
		_pythonstring.shrink_to_fit();

		if (!derive)
			logcritical("Input is missing dev tree now");
		if (derive && derive->GetRegenerate() == false)
			logcritical("Input cannot be regenerated now");

		Form::Unlock();
	}
}

bool Input::Freed()
{
	if (_sequence.size() == 0)
		return true;
	return false;
}

size_t Input::MemorySize()
{
	return sizeof(Input) + sizeof(std::pair<int64_t, int64_t>) * _parent.segments.size() + _primaryScoreIndividual.size() * sizeof(double) + _secondaryScoreIndividual.size() * sizeof(double) + _pythonstring.size() + _stringrep.size() + _sequence.size() * sizeof(std::string) + _orig_sequence.size() * sizeof(std::string);
}

void Input::TrimInput(int32_t executed)
{
	if (executed != -1 && executed < _sequence.size()) {
		// we are trimming to a specific length
		_trimmed = true;
		int32_t count = 0;
		auto aitr = begin();
		// add as long as we don't reach the end of the _sequence and haven't added more than [executed] inputs
		while (aitr != end() && count < executed) {
			_orig_sequence.push_back(*aitr);
			aitr++;
			count++;
		}
		std::swap(_sequence, _orig_sequence);
		_trimmedlength = _sequence.size();
	}
}

void Input::SetParentSplitInformation(FormID parentInput, std::vector<std::pair<int64_t, int64_t>> segments, bool complement)
{
	UnsetFlag(Input::Flags::GeneratedGrammar);
	UnsetFlag(Input::Flags::GeneratedGrammarParent);
	SetFlag(Input::Flags::GeneratedDeltaDebugging);
	_parent.parentInput = parentInput;
	_parent.segments = segments;
	_parent.complement = complement;
}

void Input::SetParentGenerationInformation(FormID parentInput)
{
	UnsetFlag(Input::Flags::GeneratedDeltaDebugging);
	SetFlag(Input::Flags::GeneratedGrammar);
	SetFlag(Input::Flags::GeneratedGrammarParent);
	_parent.parentInput = parentInput;
	_parent.segments.clear();
}

void Input::SetGenerationInformation()
{
	UnsetFlag(Input::Flags::GeneratedDeltaDebugging);
	UnsetFlag(Input::Flags::GeneratedGrammarParent);
	SetFlag(Input::Flags::GeneratedGrammar);
}

FormID Input::GetParentID()
{
	if (HasFlag(Input::Flags::GeneratedDeltaDebugging) || HasFlag(Input::Flags::GeneratedGrammarParent))
		return _parent.parentInput;
	else
		return 0;
}

int64_t Input::GetParentSplitBegin()
{
	if (HasFlag(Flags::GeneratedDeltaDebugging))
		if (_parent.segments.size() > 0)
			return _parent.segments[0].first;
	return -1;
}

int64_t Input::GetParentSplitLength()
{
	if (HasFlag(Flags::GeneratedDeltaDebugging))
		if (_parent.segments.size() > 0)
			return _parent.segments[0].second;
	return -1;
}

std::vector<std::pair<int64_t, int64_t>> Input::GetParentSplits()
{
	if (HasFlag(Flags::GeneratedDeltaDebugging))
		return _parent.segments;
	return {};
}

bool Input::GetParentSplitComplement()
{
	if (HasFlag(Flags::GeneratedDeltaDebugging))
		return _parent.complement;
	else
		return 0;
}

FormID Input::GetGenerationID()
{
	std::shared_lock<std::shared_mutex> guard(_lock);
	return _generationID;
}

void Input::SetGenerationID(FormID genID)
{
	std::unique_lock<std::shared_mutex> guard(_lock);
	_generationID = genID;
}

std::chrono::nanoseconds Input::GetExecutionTime()
{
	if (_hasfinished)
		return _executiontime;
	else
		return std::chrono::nanoseconds(-1);
}

int32_t Input::GetExitCode()
{
	if (_hasfinished)
		return _exitcode;
	else
		return -1;
}

bool Input::Finished()
{
	return _hasfinished;
}

EnumType Input::GetOracleResult()
{
	return _oracleResult;
}

double Input::GetPrimaryScore()
{
	return _primaryScore;
}

double Input::GetSecondaryScore()
{
	return _secondaryScore;
}

int64_t Input::GetTrimmedLength()
{
	return _trimmedlength;
}

bool Input::IsTrimmed()
{
	return _trimmed;
}

void Input::IncDerivedInputs()
{
	Utility::SpinLock guard(_derivedFlag);
	_derivedInputs++;
}

void Input::IncDerivedFails()
{
	Utility::SpinLock guard(_derivedFlag);
	_derivedFails++;
}

uint64_t Input::GetDerivedInputs()
{
	Utility::SpinLock guard(_derivedFlag);
	return _derivedInputs;
}

uint64_t Input::GetDerivedFails()
{
	Utility::SpinLock guard(_derivedFlag);
	return _derivedFails;
}

void Input::SetGenerationTime(std::chrono::nanoseconds genTime)
{
	std::unique_lock<std::shared_mutex> guard(_lock);
	_generationTime = genTime;
}

std::chrono::nanoseconds Input::GetGenerationTime()
{
	return _generationTime;
}

int64_t Input::GetTargetLength()
{
	if (derive)
		return derive->_sequenceNodes;
	else
		return -1;
}

bool Input::IsIndividualPrimaryScoresEnabled()
{
	return _enablePrimaryScoreIndividual;
}

bool Input::IsIndividualSecondaryScoresEnabled()
{
	return _enableSecondaryScoreIndividual;
}

size_t Input::GetIndividualPrimaryScoresLength()
{
	return _primaryScoreIndividual.size();
}

size_t Input::GetIndividualSecondaryScoresLength()
{
	return _secondaryScoreIndividual.size();
}

double Input::GetIndividualPrimaryScore(size_t position)
{
	if (_enablePrimaryScoreIndividual && position < _primaryScoreIndividual.size())
		return _primaryScoreIndividual[position];
	else
		return -1.0f;
}

double Input::GetIndividualSecondaryScore(size_t position)
{
	if (_enableSecondaryScoreIndividual && position < _secondaryScoreIndividual.size())
		return _secondaryScoreIndividual[position];
	else
		return -1.0f;
}

template <class T>
std::vector<std::pair<size_t, size_t>> ExtractRanges(std::vector<T> range)
{
	if (range.size() > 0) {
		std::vector<std::pair<size_t, size_t>> ranges;
		size_t begin = 0;
		size_t length = 0;
		double lastvalue = range[0];
		for (size_t current = 1; current < range.size(); current++) {
			if (range[current] == lastvalue)
				length++;
			else {
				// it's not identical so all elements within the range from begin until begin + length have the same value
				// -----if the length is longer than 1 element, consider it valid for our purposes
				// cut it we want perfection
				if (length > 0)
					ranges.push_back({ begin, length });
				length = 0;
				begin = current;
			}
			lastvalue = range[current];
		}
		return ranges;
	} else
		return {};
}

std::vector<std::pair<size_t, size_t>> Input::FindIndividualPrimaryScoreRangesWithoutChanges()
{
	if (_enablePrimaryScoreIndividual == false)
		return {};
	else
		return ExtractRanges<double>(_primaryScoreIndividual);
}

std::vector<std::pair<size_t, size_t>> Input::FindIndividualSecondaryScoreRangesWithoutChanges()
{
	if (_enableSecondaryScoreIndividual == false)
		return {};
	else
		return ExtractRanges<double>(_secondaryScoreIndividual);
}

#pragma region LuaFunctions

void Input::RegisterLuaFunctions(lua_State* L)
{
	// register lua functions
	lua_register(L, "Input_ConvertToPython", Input::lua_ConvertToPython);
	lua_register(L, "Input_IsTrimmed", Input::lua_IsTrimmed);
	lua_register(L, "Input_Trim", Input::lua_TrimInput);
	lua_register(L, "Input_GetExecutionTime", Input::lua_GetExecutionTime);
	lua_register(L, "Input_GetExitCode", Input::lua_GetExitCode);
	lua_register(L, "Input_GetSequenceLength", Input::lua_GetSequenceLength);
	lua_register(L, "Input_GetSequenceFirst", Input::lua_GetSequenceFirst);
	lua_register(L, "Input_GetSequenceNext", Input::lua_GetSequenceNext);
	lua_register(L, "Input_GetExitReason", Input::lua_GetExitReason);
	lua_register(L, "Input_GetCmdArgs", Input::lua_GetCmdArgs);
	lua_register(L, "Input_GetOutput", Input::lua_GetOutput);
	lua_register(L, "Input_GetReactionTimeLength", Input::lua_GetReactionTimeLength);
	lua_register(L, "Input_GetReactionTimeFirst", Input::lua_GetReactionTimeFirst);
	lua_register(L, "Input_GetReactionTimeNext", Input::lua_GetReactionTimeNext);
	lua_register(L, "Input_SetPrimaryScore", Input::lua_SetPrimaryScore);
	lua_register(L, "Input_SetSecondaryScore", Input::lua_SetSecondaryScore);

	lua_register(L, "Input_EnablePrimaryScoreIndividual", Input::lua_EnablePrimaryScoreIndividual);
	lua_register(L, "Input_EnableSecondaryScoreIndividual", Input::lua_EnableSecondaryScoreIndividual);
	lua_register(L, "Input_AddPrimaryScoreIndividual", Input::lua_AddPrimaryScoreIndividual);
	lua_register(L, "Input_AddSecondaryScoreIndividual", Input::lua_AddSecondaryScoreIndividual);
	lua_register(L, "Input_ClearScores", Input::lua_ClearScores);
	lua_register(L, "Input_ClearTrim", Input::lua_ClearTrim);
	lua_register(L, "IsOSWindows", Input::lua_IsOSWindows);
	lua_register(L, "Input_GetRetries", Input::lua_GetRetries);
}

int Input::lua_IsOSWindows(lua_State* L)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	lua_pushboolean(L, true);
#else
	lua_pushboolean(L, false);
#endif
	return 1;
}

int Input::lua_ConvertToPython(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->ConvertToPython();
	lua_pushstring(L, input->_pythonstring.c_str());
	return 1;
}

int Input::lua_IsTrimmed(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	lua_pushboolean(L, input->_trimmed);
	return 1;
}

int Input::lua_TrimInput(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	lua_Integer len = luaL_checkinteger(L, 2);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	luaL_argcheck(L, 0 <= len && len <= (lua_Integer)input->_sequence.size(), 2, (std::string("out of range, value: ") + std::to_string(len) + std::string(" , max: ") + std::to_string(input->_sequence.size())).c_str());
	input->TrimInput((int)len);
	return 0;
}

int Input::lua_GetExecutionTime(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	lua_pushinteger(L, (lua_Integer)input->_executiontime.count());
	return 1;
}

int Input::lua_GetExitCode(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	lua_pushinteger(L, (lua_Integer)input->_exitcode);
	return 1;
}

int Input::lua_GetSequenceLength(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	lua_pushinteger(L, (lua_Integer)input->_sequence.size());
	return 1;
}

int Input::lua_GetSequenceFirst(lua_State* L)
{
	static const char* empty = "";
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->_lua_sequence_next = input->begin();
	if (input->_lua_sequence_next != input->end()) {
		lua_pushstring(L, (*(input->_lua_sequence_next)).c_str());
		input->_lua_sequence_next++;
	} else
		lua_pushstring(L, empty);
	return 1;
}

int Input::lua_GetSequenceNext(lua_State* L)
{
	static const char* empty = "";
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	if (input->_lua_sequence_next != input->end()) {
		lua_pushstring(L, (*(input->_lua_sequence_next)).c_str());
		input->_lua_sequence_next++;
	} else
		lua_pushstring(L, empty);
	return 1;
}

int Input::lua_GetExitReason(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	if (input->test) {
		lua_pushinteger(L, (lua_Integer)input->test->_exitreason);
	} else
		lua_pushinteger(L, -1);
	return 1;
}

int Input::lua_GetCmdArgs(lua_State* L)
{
	static const char* empty = "";
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	if (input->test) {
		lua_pushstring(L, input->test->_cmdArgs.c_str());
	} else
		lua_pushstring(L, empty);
	return 1;
}

int Input::lua_GetOutput(lua_State* L)
{
	static const char* empty = "";
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	if (input->test) {
		lua_pushstring(L, input->test->_output.c_str());
	} else
		lua_pushstring(L, empty);
	return 1;
}

int Input::lua_GetReactionTimeLength(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	if (input->test)
		lua_pushinteger(L, (lua_Integer)input->test->_reactiontime.size());
	else
		lua_pushinteger(L, -1);
	return 1;
}

int Input::lua_GetReactionTimeFirst(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	if (input->test) {
		input->test->_lua_reactiontime_next = input->test->_reactiontime.end();
		if (input->test->_lua_reactiontime_next != input->test->_reactiontime.end()) {
			lua_pushinteger(L, (lua_Integer) (* (input->test->_lua_reactiontime_next)));
			input->test->_lua_reactiontime_next++;
		} else
			lua_pushinteger(L, -1);
	} else
		lua_pushinteger(L, -1);
	return 1;
}

int Input::lua_GetReactionTimeNext(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	if (input->test) {
		if (input->test->_lua_reactiontime_next != input->test->_reactiontime.end()) {
			lua_pushinteger(L, (lua_Integer) (* (input->test->_lua_reactiontime_next)));
			input->test->_lua_reactiontime_next++;
		} else
			lua_pushinteger(L, -1);
	} else
		lua_pushinteger(L, -1);
	return 1;
}

int Input::lua_SetPrimaryScore(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	lua_Number score = lua_tonumber(L, 2);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->_primaryScore = score;
	return 0;
}

int Input::lua_SetSecondaryScore(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	lua_Number score = lua_tonumber(L, 2);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->_secondaryScore = score;
	return 0;
}

int Input::lua_EnablePrimaryScoreIndividual(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->_enablePrimaryScoreIndividual = true;
	return 0;
}

int Input::lua_EnableSecondaryScoreIndividual(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->_enableSecondaryScoreIndividual = true;
	return 0;
}

int Input::lua_AddPrimaryScoreIndividual(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	lua_Number score = lua_tonumber(L, 2);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->_primaryScoreIndividual.push_back(score);
	return 0;
}

int Input::lua_AddSecondaryScoreIndividual(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	lua_Number score = lua_tonumber(L, 2);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->_secondaryScoreIndividual.push_back(score);
	return 0;
}

int Input::lua_ClearScores(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->_primaryScore = 0;
	input->_secondaryScore = 0;
	input->_enableSecondaryScoreIndividual = false;
	input->_enablePrimaryScoreIndividual = false;
	input->_primaryScoreIndividual.clear();
	input->_secondaryScoreIndividual.clear();
	return 0;
}
int Input::lua_ClearTrim(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	input->_trimmed = false;
	input->_trimmedlength = -1;
	return 0;
}

int Input::lua_GetRetries(lua_State* L)
{
	Input* input = (Input*)lua_touserdata(L, 1);
	luaL_argcheck(L, input != nullptr, 1, "input expected");
	lua_pushinteger(L, input->_retries);
	return 1;
}

#pragma endregion
