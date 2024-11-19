#include "DeltaDebugging.h"
#include "Data.h"
#include "SessionData.h"
#include "SessionFunctions.h"

#include <algorithm>
#include <random>
#include <cfloat>

static std::mt19937 randan((unsigned int)(std::chrono::system_clock::now().time_since_epoch().count()));

namespace Functions
{
	void DDTestCallback::Run()
	{
		// if the input we just ran is a duplicate of some other input, we will 
		// find the other input using the exclusiontree.
		// After we found it, we can delete the input and corresponding test as we don't need it
		// anymore.
		if (_input->HasFlag(Input::Flags::Duplicate)) {
			FormID prefixID;
			if (_sessiondata->_excltree->HasPrefix(_input, prefixID))
			{
				auto input = _sessiondata->data->LookupFormID<Input>(prefixID);

				if (_input->test)
					_sessiondata->data->DeleteForm(_input->test);
				if (_input->derive)
					_sessiondata->data->DeleteForm(_input->derive);
				_sessiondata->data->DeleteForm(_input);

				_input = input;
			}
		} else {
			SessionFunctions::TestEnd(_sessiondata, _input);
		}

		_DDcontroller->CallbackTest(_input);

		// ----- SESSION STUFF -----
		// contrary to normal test cases, we do not need to generate new tests as this
		// wasn't normally generated, but instead part of the DD scheme
		//SessionFunctions::GenerateTests(_sessiondata);
		// perform master checks
		SessionFunctions::MasterControl(_sessiondata);
		// schedule test generation
		// scheduling test generation is important, as delta debugging may push ordinarily generated
		// tests completely out of the execution queue, which may result in no more new tests being generated
		// after delta debugging has finished.
		// To counteract this we make sure to push for new generations when the queue is too empty
		if (_sessiondata->_settings->generation.generationsize - (int64_t)_sessiondata->_exechandler->WaitingTasks() > 0)
			SessionFunctions::GenerateTests(_sessiondata);
	}

	bool DDTestCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of session and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, sessid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
		});
		// get id of saved controller and resolve link
		uint64_t controllerid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, controllerid, resolver]() {
			this->_DDcontroller = resolver->ResolveFormID<DeltaDebugging::DeltaController>(controllerid);
		});
		// get id of saved _input and resolve link
		uint64_t inputid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, inputid, resolver]() {
			this->_input = resolver->ResolveFormID<Input>(inputid);
		});
		return true;
	}

	bool DDTestCallback::WriteData(unsigned char* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_input->GetFormID(), buffer, offset);  // +8
		return true;
	}

	size_t DDTestCallback::GetLength()
	{
		return BaseFunction::GetLength() + 24;
	}

	void DDTestCallback::Dispose()
	{
		_DDcontroller.reset();
		_sessiondata.reset();
		if (_input)
			if (_input->test)
				_input->test->_callback.reset();
		_input.reset();
	}


	void DDEvaluateExplicitCallback::Run()
	{
		_DDcontroller->CallbackExplicitEvaluate();
	}

	bool DDEvaluateExplicitCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of saved controller and resolve link
		uint64_t controllerid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, controllerid, resolver]() {
			this->_DDcontroller = resolver->ResolveFormID<DeltaDebugging::DeltaController>(controllerid);
		});
		return true;
	}

	bool DDEvaluateExplicitCallback::WriteData(unsigned char* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);  // +8
		return true;
	}

	size_t DDEvaluateExplicitCallback::GetLength()
	{
		return BaseFunction::GetLength() + 8;
	}

	void DDEvaluateExplicitCallback::Dispose()
	{
		_DDcontroller.reset();
	}
}

namespace DeltaDebugging
{
	DeltaController::~DeltaController()
	{
		Clear();
	}

	void DeltaController::Clear()
	{
		delete _params;
	}

	bool DeltaController::Start(DDParameters* params, std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Input> input, std::shared_ptr<Functions::BaseFunction> callback)
	{
		std::unique_lock<std::shared_mutex> guard(_lock);
		_params = params;
		// if the input has already been delta debugged return
		if (input->HasFlag(Input::Flags::DeltaDebugged))
			return false;
		// set internal variables
		_sessiondata = sessiondata;
		_input = input;
		_origInput = input;
		_self = sessiondata->data->LookupFormID<DeltaController>(GetFormID());
		_tasks = 0;
		_remainingtasks = 0;
		_tests = 0;
		_remainingtests = 0;
		_totaltests = 0;
		_callback = callback;
		input->SetFlag(Form::FormFlags::DoNotFree);
		input->SetFlag(Input::Flags::DeltaDebugged);
		if (input->GetGenerated() == false) {
			// we are trying to add an _input that hasn't been generated or regenerated
			// try the generate it and if it succeeds add the test
			SessionFunctions::GenerateInput(input, _sessiondata);
			if (input->GetGenerated() == false)
				return false;
		}
		if (input->Length() == 0) 
			return false;
		switch (_params->mode) {
		case DDMode::Standard:
			{
				// standard mode is the one originally developed by Andreas Zeller
				// it uses division of the _input in form of binary search and executes the
				// subsets and its complements
				StandardGenerateFirstLevel();
			}
			break;
		}
		return true;
	}

	void DeltaController::CallbackTest(std::shared_ptr<Input> input)
	{
		// update test status
		_tests++;
		_remainingtests--;
		_totaltests++;
		{
			std::unique_lock<std::mutex> guard(_completedTestsLock);
			_completedTests.insert(input);
		}
		// check for stage completion
		if (_remainingtests == 0) {
			// get out of light callback so we aren't blocking vital tasks
			auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
			callback->_DDcontroller = _self;
			_sessiondata->_controller->AddTask(callback);
		}
	}

	void DeltaController::CallbackExplicitEvaluate()
	{
		switch (_params->mode) {
		case DDMode::Standard:
			StandardEvaluateLevel();
			break;
		}
	}

	std::vector<std::shared_ptr<Input>> DeltaController::GenerateSplits(int32_t number)
	{
		StartProfiling;
		std::vector<std::shared_ptr<Input>> splits;
		// calculate the ideal size we would get from the current level
		int32_t tmp = (int32_t)(std::trunc(_input->Length() / number));
		// this value is rounded so the actual length our inputs get is different from 
		// the naive caluclation we need to recalculate [number] and can then calculate
		// the actual splitsize
		number = (int32_t)(_input->Length() / tmp);
		int32_t splitsize = (int32_t)(_input->Length() / number);
		int32_t splitbegin = 0;
		auto itr = _input->begin();
		for (int32_t i = 0; i < number; i++) {
			auto inp = _sessiondata->data->CreateForm<Input>();
			inp->SetFlag(Form::FormFlags::DoNotFree);
			if (i == number -1)
				inp->SetParentSplitInformation(_input->GetFormID(), splitbegin, (int32_t)_input->Length() - splitbegin, false);
			else
				inp->SetParentSplitInformation(_input->GetFormID(), splitbegin, splitsize, false);

			// try to find derivation tree for our input
			inp->derive = _sessiondata->data->CreateForm<DerivationTree>();
			inp->derive->_inputID = inp->GetFormID();

			_sessiondata->_grammar->Extract(_input->derive, inp->derive, inp->GetParentSplitBegin(), inp->GetParentSplitLength(), _input->Length(), inp->GetParentSplitComplement());
			if (inp->derive->_valid == false) {
				// the input cannot be derived from the given grammar
				logwarn("The split cannot be derived from the grammar.");
				_sessiondata->data->DeleteForm(inp->derive);
				_sessiondata->data->DeleteForm(inp);
				continue;
			}

			splits.push_back(inp);
			splits[i]->SetGenerated();
			if (i == number - 1) {
				// add rest of _input to split
				while (itr != _input->end()) {
					splits[i]->AddEntry(*itr);
					itr++;
					splitbegin++;
				}
			} else {
				// add a specific number of strings to _input
				for (int32_t x = 0; x < splitsize; x++) {
					splits[i]->AddEntry(*itr);
					itr++;
					splitbegin++;
				}
			}
		}
		profile(TimeProfiling, "Time taken for split generation.");
		return splits;
	}

	std::vector<std::shared_ptr<Input>> DeltaController::GenerateComplements(std::vector<std::shared_ptr<Input>>& splits)
	{
		StartProfiling;
		std::vector<std::shared_ptr<Input>> complements;
		for (int32_t i = 0; i < (int32_t)splits.size(); i++) {
			DeltaInformation dcmpl;
			dcmpl.positionbegin = (int32_t)splits[i]->GetParentSplitBegin();
			dcmpl.length = (int32_t)splits[i]->GetParentSplitLength();
			dcmpl.complement = true;
			auto inp = _sessiondata->data->CreateForm<Input>();
			inp->SetFlag(Form::FormFlags::DoNotFree);
			inp->SetParentSplitInformation(_input->GetFormID(), dcmpl.positionbegin, dcmpl.length, dcmpl.complement);

			// try to find derivation tree for our input
			inp->derive = _sessiondata->data->CreateForm<DerivationTree>();
			inp->derive->_inputID = inp->GetFormID();

			_sessiondata->_grammar->Extract(_input->derive, inp->derive, inp->GetParentSplitBegin(), inp->GetParentSplitLength(), _input->Length(), inp->GetParentSplitComplement());
			if (inp->derive->_valid == false) {
				// the input cannot be derived from the given grammar
				logwarn("The split cannot be derived from the grammar.");
				_sessiondata->data->DeleteForm(inp->derive);
				_sessiondata->data->DeleteForm(inp);
				continue;
			}

			complements.push_back(inp);
			complements[i]->SetGenerated();
			size_t count = 0;
			auto itr = _input->begin();
			while (itr != _input->end())
			{
				// if the current position is less then the beginning of the split itself, or if it is after the split
				// sequence is over
				if (count < dcmpl.positionbegin || count >= (size_t)(dcmpl.positionbegin + dcmpl.length))
					complements[i]->AddEntry(*itr);
				count++;
				itr++;
			}
		}
		profile(TimeProfiling, "Time taken for complement generation.");
		return complements;
	}

	void DeltaController::StandardGenerateFirstLevel()
	{
		// level is the number of subsets to generate from _input, it will increase every round of tests
		_level = 2;

		if (_input->Length() >= 2) {
			StandardGenerateNextLevel();
		}
	}

	void DeltaController::StandardGenerateNextLevel()
	{
		StartProfiling;
		auto splits = GenerateSplits(_level);
		auto complements = GenerateComplements(splits);
		// set internals
		_tests = 0;
		_remainingtests = (int32_t)splits.size() + (int32_t)complements.size();
		// temp
		auto addtests = [this](std::vector<std::shared_ptr<Input>>& splits) {
			FormID prefixID = 0;
			int32_t fails = 0;
			for (int32_t i = 0; i < (int32_t)splits.size(); i++) {
				// only execute _input if the length is above our settings and 
				// it isn't present in the exclusiontree
				if (_sessiondata->_settings->dd.executeAboveLength < (int64_t)splits[i]->Length()) {
					if (_sessiondata->_excltree->HasPrefix(splits[i], prefixID) == false) {
						auto call = dynamic_pointer_cast<Functions::DDTestCallback>(Functions::DDTestCallback::Create());
						call->_DDcontroller = _self;
						call->_input = splits[i];
						call->_sessiondata = _sessiondata;
						// add the tests bypassing regular tests so we can get this done with as fast as possible
						if (_sessiondata->_exechandler->AddTest(splits[i], call, _params->bypassTests) == false) {
							fails++;
							_sessiondata->IncAddTestFails();
							_sessiondata->data->DeleteForm(splits[i]);
							call->Dispose();
						} else
							_activeInputs.insert(splits[i]);
					} else {
						_sessiondata->IncGeneratedWithPrefix();
						fails++;
						_tests++;
						if (prefixID != 0)
						{
							auto ptr = _sessiondata->data->LookupFormID<Input>(prefixID);
							if (ptr)
								_activeInputs.insert(ptr);
						}
					}
				} else
					fails++;
			}
			_remainingtests -= fails;
		};
		addtests(splits);
		addtests(complements);
		// check if all the input results are already known
		if (_remainingtests == 0) {
			// start new callback to avoid blocking for too long, and to avoid reentry into the lock as
			// the Evaluation Methods are blocking
			auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
			callback->_DDcontroller = _self;
			_sessiondata->_controller->AddTask(callback);
		}
		profile(TimeProfiling, "Time taken to generate next dd level.");
	}

	void DeltaController::StandardEvaluateLevel()
	{
		// lambda that clears the DoNotFree flags on inputs that are no longer needed
		auto clearFlags = [this]() {
			for (auto ptr : _completedTests)
			{
				// if the ptr is not in the list of results
				if (_results.find(ptr) == _results.end())
					ptr->UnsetFlag(Form::FormFlags::DoNotFree);
			}
			_completedTests.clear();
		};

		std::unique_lock<std::shared_mutex> guard(_lock);
		switch (_params->GetGoal()) {
		case DDGoal::None:
			// some weird shenanigans that should not happen at all
			Finish();
			break;
		case DDGoal::ReproduceResult:
			{
				auto reproc = [this]() {
					std::vector<std::shared_ptr<Input>> res;
					auto oracle = this->_origInput->GetOracleResult();
					auto itr = _activeInputs.begin();
					while (itr != _activeInputs.end()) {
						if (oracle == (*itr)->GetOracleResult()) {
							res.push_back(*itr);
							_results.insert_or_assign(*itr, std::pair<double, int32_t>{ 0, _level });
						}
						itr++;
					}
					return res;
				};
				auto rinputs = reproc();
				_activeInputs.clear();

				// we have found all results, so free all inputs that we do not need anymore
				clearFlags();

				if (rinputs.size() == 0) {
					if (_level >= (int32_t)_input->Length()) {
						// we have already reached the maximum level
						// so we are effectively done with delta debugging.
						// Since our results are naturally integrated into the overall database we
						// don't need to do anything, but set the finished flag and we are done
						_finished = true;
						Finish();
						return;
					}

					// no inputs were found that reproduce the original result
					// so we increase the level and try new sets of inputs
					_level = std::min(_level * 2, (int32_t)_input->Length());
					StandardGenerateNextLevel();
				} else {
					// this is the very ordinary variant where we choose a test as our new starting _input
					// based on its ability to reproduce the original oracle result
					ReproduceResult* rrparams = (ReproduceResult*)_params;
					switch (rrparams->secondarygoal) {
					case DDGoal::None:
					case DDGoal::ReproduceResult:
						{
							// find the tests that reproduced the original result and pick one
							std::uniform_int_distribution<signed> dist(0, (int32_t)rinputs.size() - 1);
							// choose new base _input
							_input = rinputs[dist(randan)];
							// adjust level
							_level = std::max(_level - 1, 2);
							// run next generation
							StandardGenerateNextLevel();
						}
						break;
					case DDGoal::MaximizePrimaryScore:
						{
							// find the tests that reproduced the original result and pick the one with the highest primary score
							double score = DBL_MIN;
							std::shared_ptr<Input> inp;
							for (int32_t i = 0; i < (int32_t)rinputs.size(); i++) {
								if (score < rinputs[i]->GetPrimaryScore()) {
									inp = rinputs[i];
									score = inp->GetPrimaryScore();
								}
							}
							// just a precaution
							if (!inp)
								inp = rinputs[0];
							// set new base _input
							_input = inp;
							// adjust level
							_level = std::max(_level - 1, 2);
							// run next generation
							StandardGenerateNextLevel();
						}
						break;
					case DDGoal::MaximizeSecondaryScore:
						{
							// find the tests that reproduced the original result and pick the one with the highest secondary score
							double score = DBL_MIN;
							std::shared_ptr<Input> inp;
							for (int32_t i = 0; i < (int32_t)rinputs.size(); i++) {
								if (score < rinputs[i]->GetSecondaryScore()) {
									inp = rinputs[i];
									score = inp->GetSecondaryScore();
								}
							}
							// just a precaution
							if (!inp)
								inp = rinputs[0];
							// set new base _input
							_input = inp;
							// adjust level
							_level = std::max(_level - 1, 2);
							// run next generation
							StandardGenerateNextLevel();
						}
						break;
					}
				}
			}
			break;
		case DDGoal::MaximizePrimaryScore:
			{
				MaximizePrimaryScore* mpparams = (MaximizePrimaryScore*)_params;

				struct PrimaryLess
				{
					bool operator()(const std::shared_ptr<Input>& lhs, const std::shared_ptr<Input>& rhs) const
					{
						return lhs->GetPrimaryScore() > rhs->GetPrimaryScore();
					}
				};

				// automatically sorted based on the primary score
				// begin() -> end() == higher score -> lower score
				auto sortprimary = [this]() {
					std::set<std::shared_ptr<Input>, PrimaryLess> res;
					auto itr = _activeInputs.begin();
					while (itr != _activeInputs.end()) {
						res.insert(*itr);
						itr++;
					}
					return res;
				};
				auto rinputs = sortprimary();
				_activeInputs.clear();

				auto loss = [this](std::shared_ptr<Input> input) {
					return 1 - (input->GetPrimaryScore() / this->_origInput->GetPrimaryScore());
				};

				std::vector<std::shared_ptr<Input>> passing;
				std::vector<double> ploss;
				auto itr = rinputs.begin();
				while (itr != rinputs.end())
				{
					double l = loss(*itr);
					if (l < mpparams->acceptableLoss) {
						passing.push_back(*itr);
						ploss.push_back(l);
						_results.insert_or_assign(*itr, std::pair<double, int32_t>{ l, _level });
					}
					itr++;
				}

				// we have found all results, so free all inputs that we do not need anymore
				clearFlags();

				if (passing.size() == 0) {
					if (_level >= (int32_t)_input->Length()) {
						// we have already reached the maximum level
						// so we are effectively done with delta debugging.
						// Since our results are naturally integrated into the overall database we
						// don't need to do anything, but set the finished flag and we are done
						_finished = true;
						Finish();
						return;
					}

					// no inputs were found that reproduce the original result
					// so we increase the level and try new sets of inputs
					_level = std::min(_level * 2, (int32_t)_input->Length());
					StandardGenerateNextLevel();
				} else {
					// set new base _input
					_input = passing[0];
					// adjust level
					_level = std::max(_level - 1, 2);
					// run next generation
					StandardGenerateNextLevel();
				}
			}
			break;
		case DDGoal::MaximizeSecondaryScore:
			{
				MaximizeSecondaryScore* mpparams = (MaximizeSecondaryScore*)_params;

				struct SecondaryLess
				{
					bool operator()(const std::shared_ptr<Input>& lhs, const std::shared_ptr<Input>& rhs) const
					{
						return lhs->GetSecondaryScore() > rhs->GetSecondaryScore();
					}
				};

				// automatically sorted based on the primary score
				// begin() -> end() == higher score -> lower score
				auto sortprimary = [this]() {
					std::set<std::shared_ptr<Input>, SecondaryLess> res;
					auto itr = _activeInputs.begin();
					while (itr != _activeInputs.end()) {
						res.insert(*itr);
						itr++;
					}
					return res;
				};
				auto rinputs = sortprimary();
				_activeInputs.clear();

				auto loss = [this](std::shared_ptr<Input> input) {
					return 1 - (input->GetSecondaryScore() / this->_origInput->GetSecondaryScore());
				};

				std::vector<std::shared_ptr<Input>> passing;
				std::vector<double> ploss;
				auto itr = rinputs.begin();
				while (itr != rinputs.end()) {
					double l = loss(*itr);
					if (l < mpparams->acceptableLossSecondary) {
						passing.push_back(*itr);
						ploss.push_back(l);
						_results.insert_or_assign(*itr, std::pair<double, int32_t>{ l, _level });
					}
					itr++;
				}

				// we have found all results, so free all inputs that we do not need anymore
				clearFlags();

				if (passing.size() == 0) {
					if (_level >= (int32_t)_input->Length()) {
						// we have already reached the maximum level
						// so we are effectively done with delta debugging.
						// Since our results are naturally integrated into the overall database we
						// don't need to do anything, but set the finished flag and we are done
						_finished = true;
						Finish();
						return;
					}

					// no inputs were found that reproduce the original result
					// so we increase the level and try new sets of inputs
					_level = std::min(_level * 2, (int32_t)_input->Length());
					StandardGenerateNextLevel();
				} else {
					// set new base _input
					_input = passing[0];
					// adjust level
					_level = std::max(_level - 1, 2);
					// run next generation
					StandardGenerateNextLevel();
				}
			}
			break;
		}
	}

	void DeltaController::Finish()
	{
		for (auto [ptr, pair] : _results)
			ptr->UnsetFlag(Form::FormFlags::DoNotFree);
		_origInput->UnsetFlag(Form::FormFlags::DoNotFree);
		_activeInputs.clear();
		_completedTests.clear();
		if (_callback) {
			_sessiondata->_controller->AddTask(_callback);
			_callback.reset();
		}
	}

	size_t DeltaController::GetStaticSize(int32_t version)
	{
		static size_t size0x1 = Form::GetDynamicSize()  // base form size
		                        + 4                     // version
		                        + 4                     // tasks
		                        + 4                     // remainingtasks
		                        + 4                     // tests
		                        + 4                     // remainingtests
		                        + 4                     // totaltests
		                        + 1                     // finished
		                        + 8                     // origInput
		                        + 8                     // _input
		                        + 4                     // level
		                        + 1;                    // has callback

		switch (version)
		{
		case 0x1:
			return size0x1;
		default:
			return 0;
		}
	}

	size_t DeltaController::GetDynamicSize()
	{
		size_t sz = GetStaticSize(classversion)                                                      // static elements
		            + 8 /*size of results*/ + (8 + 8 + 4) * _results.size()                          // formids, loss, and level in results
		            + 8 /*size of activeInputs*/ + 8 * _activeInputs.size()                          // formids in activeInputs
		            + 8 /*size of completedTests*/ + 8 * _completedTests.size()                      //formids in completedTests
		            + 4 /*goal type*/ + 8;                                                           /*class size*/

		switch (_params->GetGoal()) {
		case DDGoal::MaximizePrimaryScore:
			sz += sizeof(MaximizePrimaryScore);
			break;
		case DDGoal::ReproduceResult:
			sz += sizeof(ReproduceResult);
			break;
		case DDGoal::MaximizeSecondaryScore:
			sz += sizeof(MaximizeSecondaryScore);
			break;
		case DDGoal::None:
			sz += sizeof(DDParameters);
		default:
			break;
		} // size of param class

		if (_callback)
			sz += _callback->GetLength();
		return sz;
	}

	bool DeltaController::WriteData(unsigned char* buffer, size_t& offset)
	{
		Buffer::Write(classversion, buffer, offset);
		Form::WriteData(buffer, offset);
		Buffer::Write(_tasks, buffer, offset);
		Buffer::Write(_remainingtasks, buffer, offset);
		Buffer::Write(_tests, buffer, offset);
		Buffer::Write(_remainingtests, buffer, offset);
		Buffer::Write(_totaltests, buffer, offset);
		Buffer::Write(_finished, buffer, offset);
		if (_origInput)
			Buffer::Write(_origInput->GetFormID(), buffer, offset);
		else
			Buffer::Write((FormID)0, buffer, offset);
		if (_input)
			Buffer::Write(_input->GetFormID(), buffer, offset);
		else
			Buffer::Write((FormID)0, buffer, offset);
		Buffer::Write(_level, buffer, offset);
		// results
		Buffer::WriteSize(_results.size(), buffer, offset);
		for (auto [ptr, pair] : _results) {
			if (ptr) {
				Buffer::Write(ptr->GetFormID(), buffer, offset);
				Buffer::Write(std::get<0>(pair), buffer, offset);
				Buffer::Write(std::get<1>(pair), buffer, offset);
			} else {
				Buffer::Write((FormID)0, buffer, offset);
				Buffer::Write((double)0, buffer, offset);
				Buffer::Write((int32_t)0, buffer, offset);
			}
		}
		// activeInputs
		Buffer::WriteSize(_activeInputs.size(), buffer, offset);
		for (auto ptr : _activeInputs)
		{
			if (ptr)
				Buffer::Write(ptr->GetFormID(), buffer, offset);
			else
				Buffer::Write((FormID)0, buffer, offset);
		}
		// completedTests
		Buffer::WriteSize(_completedTests.size(), buffer, offset);
		for (auto ptr : _completedTests)
			Buffer::Write(ptr->GetFormID(), buffer, offset);
		// params
		Buffer::Write((int32_t)_params->GetGoal(), buffer, offset);
		switch (_params->GetGoal())
		{
		case DDGoal::MaximizePrimaryScore:
			Buffer::WriteSize(sizeof(MaximizePrimaryScore), buffer, offset);
			//Buffer::Write((unsigned char*)_params, buffer, offset, sizeof(MaximizePrimaryScore));
			Buffer::Write(_params->minimalSubsetSize, buffer, offset);
			Buffer::Write(_params->bypassTests, buffer, offset);
			Buffer::Write((int32_t)_params->mode, buffer, offset);
			Buffer::Write((int32_t)((MaximizePrimaryScore*)_params)->acceptableLoss, buffer, offset);
			break;
		case DDGoal::ReproduceResult:
			Buffer::WriteSize(sizeof(ReproduceResult), buffer, offset);
			//Buffer::Write((unsigned char*)_params, buffer, offset, sizeof(ReproduceResult));
			Buffer::Write(_params->minimalSubsetSize, buffer, offset);
			Buffer::Write(_params->bypassTests, buffer, offset);
			Buffer::Write((int32_t)_params->mode, buffer, offset);
			Buffer::Write((int32_t)((ReproduceResult*)_params)->secondarygoal, buffer, offset);
			break;
		case DDGoal::MaximizeSecondaryScore:
			Buffer::WriteSize(sizeof(MaximizeSecondaryScore), buffer, offset);
			//Buffer::Write((unsigned char*)_params, buffer, offset, sizeof(MaximizeSecondaryScore));
			Buffer::Write(_params->minimalSubsetSize, buffer, offset);
			Buffer::Write(_params->bypassTests, buffer, offset);
			Buffer::Write((int32_t)_params->mode, buffer, offset);
			Buffer::Write((int32_t)((MaximizeSecondaryScore*)_params)->acceptableLossPrimary, buffer, offset);
			Buffer::Write((int32_t)((MaximizeSecondaryScore*)_params)->acceptableLossSecondary, buffer, offset);
			break;
		case DDGoal::None:
			Buffer::WriteSize(sizeof(DDParameters), buffer, offset);
			//Buffer::Write((unsigned char*)_params, buffer, offset, sizeof(DDParameters));
			break;
		}
		// _callback
		Buffer::Write(_callback.operator bool(), buffer, offset);
		if (_callback)
			_callback->WriteData(buffer, offset);
		return true;
	}

	bool DeltaController::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
	{
		int32_t version = Buffer::ReadInt32(buffer, offset);
		switch (version) {
		case 0x1:
			{
				Form::ReadData(buffer, offset, length, resolver);
				_tasks = Buffer::ReadInt32(buffer, offset);
				_remainingtasks = Buffer::ReadInt32(buffer, offset);
				_tests = Buffer::ReadInt32(buffer, offset);
				_remainingtests = Buffer::ReadInt32(buffer, offset);
				_totaltests = Buffer::ReadInt32(buffer, offset);
				_finished = Buffer::ReadBool(buffer, offset);
				FormID origInput = Buffer::ReadUInt64(buffer, offset);
				FormID input = Buffer::ReadUInt64(buffer, offset);
				_level = Buffer::ReadInt32(buffer, offset);
				// results
				size_t reslen = Buffer::ReadSize(buffer, offset);
				std::vector<FormID> res;
				std::vector<double> resloss;
				std::vector<int32_t> reslevel;
				for (size_t i = 0; i < reslen; i++) {
					res.push_back(Buffer::ReadUInt64(buffer, offset));
					resloss.push_back(Buffer::ReadDouble(buffer, offset));
					reslevel.push_back(Buffer::ReadInt32(buffer, offset));
				}
				// activeInputs
				size_t actIlen = Buffer::ReadSize(buffer, offset);
				std::vector<FormID> actI;
				for (size_t i = 0; i < actIlen; i++)
					actI.push_back(Buffer::ReadUInt64(buffer, offset));
				// resolver
				resolver->AddTask([this, resolver, origInput, input, res, resloss, reslevel, actI]() {
					_sessiondata = resolver->ResolveFormID<SessionData>(Data::StaticFormIDs::SessionData);
					_self = resolver->ResolveFormID<DeltaController>(this->GetFormID());
					_input = resolver->ResolveFormID<Input>(input);
					_origInput = resolver->ResolveFormID<Input>(origInput);
					// results
					for (size_t i = 0; i < res.size(); i++) {
						auto ptr = resolver->ResolveFormID<Input>(res[i]);
						if (ptr)
							_results.insert_or_assign(ptr, std::pair<double, int32_t>{ resloss[i], reslevel[i] });
					}
					// activeInputs
					for (size_t i = 0; i < actI.size(); i++) {
						auto ptr = resolver->ResolveFormID<Input>(actI[i]);
						if (ptr)
							_activeInputs.insert(ptr);
					}
				});
				// completedTests
				size_t cmpllen = Buffer::ReadSize(buffer, offset);
				std::vector<FormID> complInp;
				for (size_t i = 0; i < cmpllen; i++)
					complInp.push_back(Buffer::ReadUInt64(buffer, offset));
				resolver->AddTask([this, resolver, complInp]() {
					for (size_t i = 0; i < complInp.size(); i++) {
						auto ptr = resolver->ResolveFormID<Input>(complInp[i]);
						if (ptr)
							_completedTests.insert(ptr);
					}
				});
				// params
				DDGoal goal = (DDGoal)Buffer::ReadInt32(buffer, offset);
				size_t sz = Buffer::ReadSize(buffer, offset);
				//unsigned char* dat = Buffer::ReadBuffer(buffer, offset, sz);
				switch (goal)
				{
				case DDGoal::None:
					_params = new DDParameters;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					break;
				case DDGoal::ReproduceResult:
					_params = new ReproduceResult;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((ReproduceResult*)_params)->secondarygoal = (DDGoal)Buffer::ReadInt32(buffer, offset);
					break;
				case DDGoal::MaximizePrimaryScore:
					_params = new MaximizePrimaryScore;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((MaximizePrimaryScore*)_params)->acceptableLoss = Buffer::ReadFloat(buffer, offset);
					break;
				case DDGoal::MaximizeSecondaryScore:
					_params = new MaximizeSecondaryScore;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((MaximizeSecondaryScore*)_params)->acceptableLossPrimary = Buffer::ReadFloat(buffer, offset);
					((MaximizeSecondaryScore*)_params)->acceptableLossSecondary = Buffer::ReadFloat(buffer, offset);
					break;
				}
				//memcpy((void*)_params, dat, sz);
				//delete dat;
				// _callback
				bool hascall = Buffer::ReadBool(buffer, offset);
				if (hascall)
					_callback = Functions::BaseFunction::Create(buffer, offset, length, resolver);

				return true;
			}
			break;
		default:
			return false;
		}
	}

	void DeltaController::Delete(Data*)
	{
		Clear();
	}

	void DeltaController::RegisterFactories()
	{
		if (!_registeredFactories)
		{
			_registeredFactories = true;
			Functions::RegisterFactory(Functions::DDTestCallback::GetTypeStatic(), Functions::DDTestCallback::Create);
			Functions::RegisterFactory(Functions::DDEvaluateExplicitCallback::GetTypeStatic(), Functions::DDEvaluateExplicitCallback::Create);
		}
	}
}