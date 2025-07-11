#include "DeltaDebugging.h"
#include "Data.h"
#include "SessionData.h"
#include "SessionFunctions.h"
#include "Generation.h"

#include <algorithm>
#include <random>
#include <cfloat>

static std::mt19937 randan((unsigned int)(std::chrono::system_clock::now().time_since_epoch().count()));

namespace Functions
{
	void DDTestCallback::Run()
	{
		if (!_input) {
			logcritical("DDTestCallback was called, but _input is empty.");
			return;
		}
		if (!_sessiondata) {
			logcritical("DDTestCallback was called, but _sessiondata was empty.");
		}
		// check whether the input length doesn't match the devtree size
		if (_input->derive && (int64_t)_input->GetSequenceLength() > _input->derive->_sequenceNodes) {
			logcritical("Input is longer than dev tree large, Form: {}", Utility::PrintForm(_input));
			_input->IncRetries();
			if (_input->test)
				_input->test->_exitreason = Test::ExitReason::Repeat;
			_input->Debug_ClearSequence();
			_input->SetGenerated(false);
			auto test = _input->test;
			_input->test.reset();
			auto callback = this->DeepCopy();
			_sessiondata->data->DeleteForm(test);
			_sessiondata->_exechandler->AddTest(_input, callback, true, false);
			SessionFunctions::AddTestExitReason(_sessiondata, Test::ExitReason::Repeat);
			return;
		}

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
			if (SessionFunctions::TestEnd(_sessiondata, _input, false, _DDcontroller))
			{
				// test has to be repeated, don't do anything yet
				return;
			}
		}

		_DDcontroller->CallbackTest(_input, _batchident, _batchtasks);

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
		if (!CmdArgs::_testdd && _sessiondata->_settings->generation.generationsize - (int64_t)_sessiondata->_exechandler->WaitingTasks() > 0)
			SessionFunctions::GenerateTests(_sessiondata);
	}

	std::shared_ptr<BaseFunction> DDTestCallback::DeepCopy()
	{
		auto ptr = std::make_shared<DDTestCallback>();
		ptr->_sessiondata = _sessiondata;
		ptr->_DDcontroller = _DDcontroller;
		ptr->_input = _input;
		ptr->_batchtasks = _batchtasks;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	void DDTestCallback::Init(LoadResolver* resolver, FormID sessid, FormID controlid, FormID inputid)
	{
		resolver->AddTask([this, sessid, resolver]() {
			this->_sessiondata = resolver->ResolveFormID<SessionData>(sessid);
		});
		resolver->AddTask([this, controlid, resolver]() {
			this->_DDcontroller = resolver->ResolveFormID<DeltaDebugging::DeltaController>(controlid);
		});
		resolver->AddTask([this, inputid, resolver]() {
			this->_input = resolver->ResolveFormID<Input>(inputid);
		});
		resolver->AddLateTask([this]() {
			if (this->_DDcontroller && this->_DDcontroller->GetBatchIdent() == this->_batchident)
				this->_batchtasks = this->_DDcontroller->GetBatchTasks();
			else
				this->_batchtasks = std::make_shared<DeltaDebugging::Tasks>();
		});
	}

	bool DDTestCallback::ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of session and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		// get id of saved controller and resolve link
		uint64_t controllerid = Buffer::ReadUInt64(buffer, offset);
		// get id of saved _input and resolve link
		uint64_t inputid = Buffer::ReadUInt64(buffer, offset);
		_batchident = Buffer::ReadUInt64(buffer, offset);
		Init(resolver, sessid, controllerid, inputid);
		return true;
	}
	bool DDTestCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of session and resolve link
		uint64_t sessid = Buffer::ReadUInt64(buffer, offset);
		// get id of saved controller and resolve link
		uint64_t controllerid = Buffer::ReadUInt64(buffer, offset);
		// get id of saved _input and resolve link
		uint64_t inputid = Buffer::ReadUInt64(buffer, offset);
		_batchident = Buffer::ReadUInt64(buffer, offset);
		Init(resolver, sessid, controllerid, inputid);
		return true;
	}

	bool DDTestCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_input->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_batchident, buffer, offset);         // +8
		return true;
	}

	unsigned char* DDTestCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		Buffer::Write(_sessiondata->GetFormID(), buffer, offset);   // +8
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);  // +8
		Buffer::Write(_input->GetFormID(), buffer, offset);         // +8
		Buffer::Write(_batchident, buffer, offset);                 // +8
		size = GetLength();
		return buffer;
	}

	size_t DDTestCallback::GetLength()
	{
		return BaseFunction::GetLength() + 32;
	}

	void DDTestCallback::Dispose()
	{
		_DDcontroller.reset();
		_sessiondata.reset();
		if (_input)
			if (_input->test)
				_input->test->_callback.reset();
		_input.reset();
		_batchtasks.reset();
	}


	void DDEvaluateExplicitCallback::Run()
	{
		if (_DDcontroller) {
			_DDcontroller->CallbackExplicitEvaluate();
		}
	}

	std::shared_ptr<BaseFunction> DDEvaluateExplicitCallback::DeepCopy()
	{
		auto ptr = std::make_shared<DDEvaluateExplicitCallback>();
		ptr->_DDcontroller = _DDcontroller;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	bool DDEvaluateExplicitCallback::ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		// get id of saved controller and resolve link
		uint64_t controllerid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, controllerid, resolver]() {
			this->_DDcontroller = resolver->ResolveFormID<DeltaDebugging::DeltaController>(controllerid);
		});
		return true;
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

	bool DDEvaluateExplicitCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);  // +8
		return true;
	}

	unsigned char* DDEvaluateExplicitCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);  // +8
		size = GetLength();
		return buffer;
	}

	size_t DDEvaluateExplicitCallback::GetLength()
	{
		return BaseFunction::GetLength() + 8;
	}

	void DDEvaluateExplicitCallback::Dispose()
	{
		_DDcontroller.reset();
	}

	void DDGenerateComplementCallback::Run()
	{
		if (_DDcontroller && _input && _batchtasks) {
			_DDcontroller->GenerateComplements_Async_Callback(_begin, _length, _approxthreshold, _batchident, _input, _batchtasks);
		}
	}
	bool DDGenerateComplementCallback::ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		uint64_t controllerid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, controllerid, resolver]() {
			this->_DDcontroller = resolver->ResolveFormID<DeltaDebugging::DeltaController>(controllerid);
		});
		_begin = Buffer::ReadInt32(buffer, offset);
		_length = Buffer::ReadInt32(buffer, offset);
		_approxthreshold = Buffer::ReadDouble(buffer, offset);
		_batchident = Buffer::ReadUInt64(buffer, offset);
		uint64_t inputid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, inputid, resolver]() {
			this->_input = resolver->ResolveFormID<Input>(inputid);
		});
		resolver->AddLateTask([this]() {
			if (this->_DDcontroller && this->_DDcontroller->GetBatchIdent() == this->_batchident)
				this->_batchtasks = this->_DDcontroller->GetBatchTasks();
			else
				this->_batchtasks = std::make_shared<DeltaDebugging::Tasks>();
		});
		return true;
	}
	bool DDGenerateComplementCallback::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		uint64_t controllerid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, controllerid, resolver]() {
			this->_DDcontroller = resolver->ResolveFormID<DeltaDebugging::DeltaController>(controllerid);
		});
		_begin = Buffer::ReadInt32(buffer, offset);
		_length = Buffer::ReadInt32(buffer, offset);
		_approxthreshold = Buffer::ReadDouble(buffer, offset);
		_batchident = Buffer::ReadUInt64(buffer, offset);
		uint64_t inputid = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, inputid, resolver]() {
			this->_input = resolver->ResolveFormID<Input>(inputid);
		});
		resolver->AddLateTask([this]() {
			if (this->_DDcontroller && this->_DDcontroller->GetBatchIdent() == this->_batchident)
				this->_batchtasks = this->_DDcontroller->GetBatchTasks();
			else
				this->_batchtasks = std::make_shared<DeltaDebugging::Tasks>();
		});
		return true;
	}
	bool DDGenerateComplementCallback::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);
		Buffer::Write(_begin, buffer, offset);
		Buffer::Write(_length, buffer, offset);
		Buffer::Write(_approxthreshold, buffer, offset);
		Buffer::Write(_batchident, buffer, offset);
		Buffer::Write(_input->GetFormID(), buffer, offset);
		return true;
	}

	unsigned char* DDGenerateComplementCallback::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);
		Buffer::Write(_begin, buffer, offset);
		Buffer::Write(_length, buffer, offset);
		Buffer::Write(_approxthreshold, buffer, offset);
		Buffer::Write(_batchident, buffer, offset);
		Buffer::Write(_input->GetFormID(), buffer, offset);
		size = GetLength();
		return buffer;
	}

	std::shared_ptr<BaseFunction> DDGenerateComplementCallback::DeepCopy()
	{
		auto ptr = std::make_shared<DDGenerateComplementCallback>();
		ptr->_DDcontroller = _DDcontroller;
		ptr->_begin = _begin;
		ptr->_length = _length;
		ptr->_approxthreshold = _approxthreshold;
		ptr->_batchident = _batchident;
		ptr->_input = _input;
		ptr->_batchtasks = _batchtasks;
		return dynamic_pointer_cast<Functions::BaseFunction>(ptr);

	}
	void DDGenerateComplementCallback::Dispose()
	{
		_DDcontroller.reset();
		_input.reset();
		_batchtasks.reset();
	}
	size_t DDGenerateComplementCallback::GetLength()
	{
		return BaseFunction::GetLength() + /*8 DD, 4 begin, 4 length, 8 approx, 8 batch,8 input*/ 40;
	}

	void DDGenerateCheckSplit::Run()
	{
		if (_DDcontroller && _input && _batchtasks) {
			_DDcontroller->GenerateSplits_Async_Callback(_input, _approxthreshold, _batchident, _batchtasks);
		}
	}

	std::shared_ptr<BaseFunction> DDGenerateCheckSplit::DeepCopy()
	{
		auto ptr = std::make_shared<DDGenerateCheckSplit>();
		ptr->_DDcontroller = _DDcontroller;
		ptr->_input = _input;
		ptr->_approxthreshold = _approxthreshold;
		ptr->_batchident = _batchident;
		ptr->_batchtasks = _batchtasks;
		return dynamic_pointer_cast<BaseFunction>(ptr);
	}

	bool DDGenerateCheckSplit::ReadData(std::istream* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		FormID controllerID = Buffer::ReadUInt64(buffer, offset);
		FormID inputID = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, controllerID, inputID, resolver]() {
			_DDcontroller = resolver->ResolveFormID<DeltaDebugging::DeltaController>(controllerID);
			_input = resolver->ResolveFormID<Input>(inputID);
		});
		_approxthreshold = Buffer::ReadDouble(buffer, offset);
		_batchident = Buffer::ReadUInt64(buffer, offset);
		resolver->AddLateTask([this]() {
			if (this->_DDcontroller && this->_DDcontroller->GetBatchIdent() == this->_batchident)
				this->_batchtasks = this->_DDcontroller->GetBatchTasks();
			else
				this->_batchtasks = std::make_shared<DeltaDebugging::Tasks>();
		});
		return true;
	}
	bool DDGenerateCheckSplit::ReadData(unsigned char* buffer, size_t& offset, size_t, LoadResolver* resolver)
	{
		FormID controllerID = Buffer::ReadUInt64(buffer, offset);
		FormID inputID = Buffer::ReadUInt64(buffer, offset);
		resolver->AddTask([this, controllerID, inputID, resolver]() {
			_DDcontroller = resolver->ResolveFormID<DeltaDebugging::DeltaController>(controllerID);
			_input = resolver->ResolveFormID<Input>(inputID);
		});
		_approxthreshold = Buffer::ReadDouble(buffer, offset);
		_batchident = Buffer::ReadUInt64(buffer, offset);
		resolver->AddLateTask([this]() {
			if (this->_DDcontroller && this->_DDcontroller->GetBatchIdent() == this->_batchident)
				this->_batchtasks = this->_DDcontroller->GetBatchTasks();
			else
				this->_batchtasks = std::make_shared<DeltaDebugging::Tasks>();
		});
		return true;
	}
	bool DDGenerateCheckSplit::WriteData(std::ostream* buffer, size_t& offset)
	{
		BaseFunction::WriteData(buffer, offset);
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);
		Buffer::Write(_input->GetFormID(), buffer, offset);
		Buffer::Write(_approxthreshold, buffer, offset);
		Buffer::Write(_batchident, buffer, offset);
		return true;
	}

	unsigned char* DDGenerateCheckSplit::GetData(size_t& size)
	{
		unsigned char* buffer = new unsigned char[GetLength()];
		size_t offset = 0;
		Buffer::Write(GetType(), buffer, offset);
		Buffer::Write(_DDcontroller->GetFormID(), buffer, offset);
		Buffer::Write(_input->GetFormID(), buffer, offset);
		Buffer::Write(_approxthreshold, buffer, offset);
		Buffer::Write(_batchident, buffer, offset);
		size = GetLength();
		return buffer;
	}

	void DDGenerateCheckSplit::Dispose()
	{
		_DDcontroller.reset();
		_input.reset();
		_batchtasks.reset();
	}
	size_t DDGenerateCheckSplit::GetLength()
	{
		return BaseFunction::GetLength() + /*8 DD, 8 input, 8 approx, 8 batch*/ 32;
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
		Form::ClearForm();
		_tasks = 0;
		_remainingtasks = 0;
		_tests = 0;
		_remainingtests = 0;
		_totaltests = 0;
		_finished = false;
		_results.clear();
		_origInput.reset();
		_input.reset();
		_inputRanges.clear();
		_sessiondata.reset();
		_self.reset();
		_activeInputs.clear();
		_completedTests.clear();
		_callback.clear();
		_bestScore = { 0.0f, 0.0f };
		_level = 2;
		if (_params)
			delete _params;
		_params = nullptr;
	}

	bool DeltaController::Start(DDParameters* params, std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Input> input, std::shared_ptr<Functions::BaseFunction> callback)
	{
		// taint changed
		SetChanged();

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
		{
			std::unique_lock<std::mutex> guardl(_callbackLock);
			_callback.push_back(callback);
		}
		input->SetFlag(Form::FormFlags::DoNotFree);
		input->derive->SetFlag(Form::FormFlags::DoNotFree);
		input->SetFlag(Input::Flags::DeltaDebugged);
		if (input->GetGenerated() == false||input->GetSequenceLength() == 0) {
			input->SetGenerated(false);
			// we are trying to add an _input that hasn't been generated or regenerated
			// try the generate it and if it succeeds add the test
			SessionFunctions::GenerateInput(input, _sessiondata);
			if (input->GetGenerated() == false)
				return false;
		}
		if (input->GetSequenceLength() == 0)
			return false;
		switch (_params->GetGoal()) {
		case DDGoal::MaximizePrimaryScore:
			_bestScore = { _input->GetPrimaryScore(), 0.0f };
			break;
		case DDGoal::MaximizeSecondaryScore:
			_bestScore = { 0.0f, _input->GetSecondaryScore() };
			break;
		case DDGoal::MaximizeBothScores:
			_bestScore = { _input->GetPrimaryScore(), _input->GetSecondaryScore() };
			break;
		case DDGoal::ReproduceResult:
			switch (((ReproduceResult*)_params)->secondarygoal) {
			case DDGoal::MaximizePrimaryScore:
				_bestScore = { _input->GetPrimaryScore(), 0.0f };
				break;
			case DDGoal::MaximizeSecondaryScore:
				_bestScore = { 0.0f, _input->GetSecondaryScore() };
				break;
			case DDGoal::MaximizeBothScores:
				_bestScore = { _input->GetPrimaryScore(), _input->GetSecondaryScore() };
				break;
			case DDGoal::ReproduceResult:
				_bestScore = { _input->GetPrimaryScore(), _input->GetSecondaryScore() };
				break;
			case DDGoal::None:
				_bestScore = { _input->GetPrimaryScore(), _input->GetSecondaryScore() };
				break;
			}
			break;
		case DDGoal::None:
			break;
		}
		// set starting time point
		_DD_begin = sessiondata->data->GetRuntime();

		switch (_params->mode) {
		case DDMode::Standard:
			{
				// standard mode is the one originally developed by Andreas Zeller
				// it uses division of the _input in form of binary search and executes the
				// subsets and its complements
				StandardGenerateFirstLevel();
			}
			break;
		case DDMode::ScoreProgress:
			{
				// custom mode
				// it uses division of the _input in form of binary search and only removes subsets form the input
				// that show no primary progress
				// executes only complements
				_inputRanges = _input->FindIndividualPrimaryScoreRangesWithoutChanges();
				if (_sessiondata->_settings->dd.ScoreProgressSkipDeltaDebuggedParts) {
					if (input->HasFlag(Input::Flags::GeneratedDeltaDebugging)) {
						// input has been generated by delta debugging
						// -> we likely didn't produce any better inputs so we are giving this one another chance
						// -> do not skip any ranges in the primary score
						_skipRanges = 0;
					} else if (input->HasFlag(Input::Flags::GeneratedGrammarParent)) {
						// input is an extension of a parent input.
						// if the parent input was generated by delta debugging, skip
						// the ranges that remain in the parent as they are unlikely to
						// lead to any further improvements, instead only focus
						// on the new parts of the input that must be reduced
						auto parent = _sessiondata->data->LookupFormID<Input>(input->GetParentID());
						if (parent) {
							while (parent && parent->HasFlag(Input::Flags::GeneratedDeltaDebugging) == false) {
								parent = _sessiondata->data->LookupFormID<Input>(parent->GetParentID());
							}
							if (parent) {
								// parent has been generated by delta debugging
								_skipRanges = parent->FindIndividualPrimaryScoreRangesWithoutChanges(parent->Length() - parent->GetParentBacktrack()).size();
								if (_skipRanges > 0)
									_skipRanges--; // allow dd'ing the last range recorded
							} else
								// there is no ancestor that was generated by delta debugging
								_skipRanges = 0;
						} else
							// no parent available // should not be possible
							_skipRanges = 0;
					} else {
						// directly generated input so don't skip any ranges
						_skipRanges = 0;
					}
				}
				ScoreProgressGenerateFirstLevel();
			}
			break;
		}
		return true;
	}

	void DeltaController::CallbackTest(std::shared_ptr<Input> input, uint64_t batchident, std::shared_ptr<DeltaDebugging::Tasks> tasks)
	{
		// taint changed
		SetChanged();
		// update test status
		{
			std::unique_lock<std::mutex> guard(_completedTestsLock);
			if (batchident == genCompData.batchident) {
				_tests++;
				_remainingtests--;
				_activetests--;
				_totaltests++;
			} else {
				_totaltests++;
				tasks->tasks--;
				//loginfo("{}, wrong batch, tasks --: {}", input->GetFormID(), tasks->tasks.load());
				//logmessage("Skipped: Current batch: {}, Actual Batch: {}", genCompData.batchident, batchident);
				// old batch aldtready superseeded
				// unset flags and free
				input->UnsetFlag(Form::FormFlags::DoNotFree);
				if (input->test)
					input->test->UnsetFlag(Form::FormFlags::DoNotFree);
				if (input->derive)
					input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
				// free memory to save reclaim time
				input->FreeMemory();
				if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
					if (input->derive)
						input->derive->FreeMemory();
					if (input->test)
						input->test->FreeMemory();
				}
				//logmessage("free ddtest {}", input->GetFormID());
				return;
			}
		}
		// if the level is active try get a new task and start evaluating whether the level is completed
		if (genCompData.active) {
			// try starting new task
			std::shared_ptr<Functions::BaseFunction> job;
			{
				std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
				if (genCompData.testqueue.empty() == false) {
					job = genCompData.testqueue.front();
					if (genCompData.testqueue.empty() == false) {
						genCompData.testqueue.pop_front();
					}
					if (job)
						genCompData.tasks.load()->tasks++;
				}
			}
			// task has finished
			tasks->tasks--;
			loginfo("{}, tasks --: {}", input->GetFormID(), tasks->tasks.load());
			if (job) {
				// if job has been found
				_sessiondata->_controller->AddTask(job);
			}
			// if there is no more task to start check whether the active inputs are 0 and then end the level
			auto genCtask = genCompData.tasks.load();
			if (genCtask && genCtask->tasks.load() <= 0 /*&& _activetests == 0 */) {
				genCompData.active = false;
				// the current stage has finished
				// get out of light callback so we aren't blocking vital tasks
				auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
				callback->_DDcontroller = _self;
				genCtask->sendEndEvent = true;
				_sessiondata->_controller->AddTask(callback);
				return;
			}

			bool res = false;
			switch (_params->mode) {
			case DDMode::Standard:
				res = StandardEvaluateInput(input);
				break;
			case DDMode::ScoreProgress:
				res = ScoreProgressEvaluateInput(input);
				break;
			}
			_completedTests.insert(input);

			// evaluate whether th level should be ended prematurely
			if (_sessiondata->_settings->dd.batchprocessing != 0 /*enabled*/) {
				// evaluate the testcase here and now
				if (res)
					_stopbatch = true;
				std::unique_lock<std::mutex> guard(_batchlock);
				// check whether we have exceeded our budget
				if (_params->budget != 0 && _params->budget <= _totaltests)
					_stopbatch = true;

				if (_stopbatch) {
					genCompData.active = false;
					{
						std::unique_lock<std::mutex> guardtestlock(genCompData.testqueuelock);
						std::list<std::shared_ptr<Functions::BaseFunction>> tmp;
						genCompData.testqueue.swap(tmp);
						genCompData.testqueue.clear();
						// stop batch, delete all waiting tests and call evaluate
						_skippedTests += (int32_t)tmp.size();
						_remainingtests = 0;

						for (auto ptr : tmp) {
							ptr->Dispose();
						}
					}

					// get out of light callback so we aren't blocking vital tasks
					auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
					callback->_DDcontroller = _self;
					tasks->sendEndEvent = true;
					_sessiondata->_controller->AddTask(callback);
				}
			}
		} else {
			// unset flags and free
			input->UnsetFlag(Form::FormFlags::DoNotFree);
			if (input->test)
				input->test->UnsetFlag(Form::FormFlags::DoNotFree);
			if (input->derive)
				input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
			// free memory to save reclaim time
			input->FreeMemory();
			if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
				if (input->derive)
					input->derive->FreeMemory();
				if (input->test)
					input->test->FreeMemory();
			}

			tasks->tasks--;
			loginfo("{}, inactive batch, tasks --: {}", Utility::PrintForm(input), tasks->tasks.load());
		}
	}

	void DeltaController::CallbackExplicitEvaluate()
	{
		// taint changed
		SetChanged();
		auto tasks = genCompData.tasks.load();
		if (!tasks || tasks->processedEndEvent)
		{
			logwarn("already processed end event, aborting");
			return;
		}
		genCompData.tasks.load()->processedEndEvent = true;
		switch (_params->mode) {
		case DDMode::Standard:
			StandardEvaluateLevel();
			break;
		case DDMode::ScoreProgress:
			ScoreProgressEvaluateLevel();
			break;
		}
	}

	std::vector<std::shared_ptr<Input>> DeltaController::GenerateSplits(int32_t number, std::vector<DeltaInformation>& splitinfo)
	{
		StartProfiling;
		double approxthreshold = _origInput->GetPrimaryScore() - _origInput->GetPrimaryScore() * _sessiondata->_settings->dd.approximativeExecutionThreshold;
		std::vector<std::shared_ptr<Input>> splits;
		// calculate the ideal size we would get from the current level
		int32_t tmp = (int32_t)(std::trunc(_input->Length() / number));
		if (tmp < 1)
			tmp = 1;

		// this value is rounded so the actual length our inputs get is different from 
		// the naive caluclation we need to recalculate [number] and can then calculate
		// the actual splitsize
		number = (int32_t)(_input->Length() / tmp);
		int32_t splitsize = (int32_t)(_input->Length() / number);
		int32_t splitbegin = 0;
		auto itr = _input->begin();
		for (int32_t i = 0; i < number; i++) {
			DeltaInformation df;
			df.positionbegin = splitbegin;
			if (i == number - 1)
				df.length = (int32_t)_input->Length() - splitbegin;
			else
				df.length = splitsize;
			df.complement = false;
			splitinfo.push_back(df);
			// skip inputs that are beneath the min exec length
			if (df.length < _sessiondata->_settings->dd.executeAboveLength)
				continue;

			auto inp = _sessiondata->data->CreateForm<Input>();
			inp->SetFlag(Form::FormFlags::DoNotFree);
			if (i == number - 1)
				inp->SetParentSplitInformation(_input->GetFormID(), { { splitbegin, (int32_t)_input->Length() - splitbegin } }, false);
			else
				inp->SetParentSplitInformation(_input->GetFormID(), { { splitbegin, splitsize } }, false);

			if (i == number - 1) {
				// add rest of _input to split
				while (itr != _input->end()) {
					inp->AddEntry(*itr);
					itr++;
					splitbegin++;
				}
			} else {
				// add a specific number of strings to _input
				for (int32_t x = 0; x < splitsize; x++) {
					inp->AddEntry(*itr);
					itr++;
					splitbegin++;
				}
			}

			if (CheckInput(_input, inp, approxthreshold))
				splits.push_back(inp);

		}
		profile(TimeProfiling, "Time taken for split generation.");
		return  splits;
	}

	
	void DeltaController::GenerateSplits_Async(int32_t number)
	{
		StartProfiling;
		{
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			double approxthreshold = _origInput->GetPrimaryScore() - _origInput->GetPrimaryScore() * _sessiondata->_settings->dd.approximativeExecutionThreshold;
			std::vector<std::shared_ptr<Input>> splits;
			// calculate the ideal size we would get from the current level
			int32_t tmp = (int32_t)(std::trunc(_input->Length() / number));
			if (tmp < 1)
				tmp = 1;

			ReadLockHolder<Input> holder(_input);

			// this value is rounded so the actual length our inputs get is different from
			// the naive caluclation we need to recalculate [number] and can then calculate
			// the actual splitsize
			number = (int32_t)(_input->Length() / tmp);
			int32_t splitsize = (int32_t)(_input->Length() / number);
			int32_t splitbegin = 0;
			auto itr = _input->begin();

			for (int32_t i = 0; i < number; i++) {
				DeltaInformation df;
				df.positionbegin = splitbegin;
				if (i == number - 1)
					df.length = (int32_t)_input->Length() - splitbegin;
				else
					df.length = splitsize;
				df.complement = false;
				genCompData.dinfo.push_back(df);
				// skip inputs that are beneath the min exec length
				if (df.length < _sessiondata->_settings->dd.executeAboveLength)
					continue;

				auto inp = _sessiondata->data->CreateForm<Input>();
				inp->SetFlag(Form::FormFlags::DoNotFree);
				if (i == number - 1)
					inp->SetParentSplitInformation(_input->GetFormID(), { { splitbegin, (int32_t)_input->Length() - splitbegin } }, false);
				else
					inp->SetParentSplitInformation(_input->GetFormID(), { { splitbegin, splitsize } }, false);

				if (i == number - 1) {
					// add rest of _input to split
					while (itr != _input->end()) {
						inp->AddEntry(*itr);
						itr++;
						splitbegin++;
					}
				} else {
					// add a specific number of strings to _input
					for (int32_t x = 0; x < splitsize; x++) {
						inp->AddEntry(*itr);
						itr++;
						splitbegin++;
					}
				}

				auto callback = dynamic_pointer_cast<Functions::DDGenerateCheckSplit>(Functions::DDGenerateCheckSplit::Create());
				callback->_DDcontroller = _self;
				callback->_input = inp;
				callback->_approxthreshold = approxthreshold;
				callback->_batchident = genCompData.batchident;
				callback->_batchtasks = genCompData.tasks;
				genCompData.testqueue.push_back(callback);
			}
		}
		StandardGenerateNextLevel_Inter();
	}
	void DeltaController::GenerateSplits_Async_Callback(std::shared_ptr<Input> input, double approxthreshold, uint64_t batchident, std::shared_ptr<DeltaDebugging::Tasks> tasks)
	{
		// taint changed
		SetChanged();
		if (batchident != genCompData.batchident) {
			// do nothing if the input we are supposed to handle is from an older batch, just discard
			_skippedTests++;
			return;
		}
		if (CheckInput(_input, input, approxthreshold) && input) {
			if (DoTest(input, batchident, tasks))  // if the test is valid and is being executed just return
				return;
		}
		// if the test is not valid or cannot be executed generate a new test for execution
		std::shared_ptr<Functions::BaseFunction> job;
		{
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			if (genCompData.testqueue.empty() == false) {
				job = genCompData.testqueue.front();
				if (genCompData.testqueue.empty() == false) {
					genCompData.testqueue.pop_front();
				}
				if (job)
					genCompData.tasks.load()->tasks++;
			}
		}
		tasks->tasks--;
		loginfo("{}, tasks --: {}", Utility::PrintForm(input), tasks->tasks.load());
		if (job) {
			// if job has been found
			_sessiondata->_controller->AddTask(job);
		} else {
			std::unique_lock<std::mutex> guard(_completedTestsLock);
			if (genCompData.tasks.load()->tasks.load() <= 0 /*&& _activetests == 0 */) {
				genCompData.active = false;
				// the current stage has finished
				// get out of light callback so we aren't blocking vital tasks
				auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
				callback->_DDcontroller = _self;
				_sessiondata->_controller->AddTask(callback);
				return;
			}
		}
	}

	bool DeltaController::CheckInput(std::shared_ptr<Input> parentinp, std::shared_ptr<Input> inp, double approxthreshold)
	{
		StartProfiling;

		// fix for parentinp
		if (parentinp->GetGenerated() == false || parentinp->GetSequenceLength() == 0 || parentinp->derive->_nodes == 0) {
			parentinp->SetGenerated(false);
			// we are trying to add an parentinp that hasn't been generated or regenerated
			// try the generate it and if it succeeds add the test
			SessionFunctions::GenerateInput(parentinp, _sessiondata);
			if (parentinp->GetGenerated() == false)
				return false;
		}

		LockHolder<Input> inputlock(inp);
		ReadLockHolder<Input> parentlock(parentinp);
		ReadLockHolder<DerivationTree> parentdevlock(parentinp->derive);

		// check against exclusion tree
		if (_sessiondata->_settings->dd.approximativeTestExecution && _params->GetGoal() == DDGoal::MaximizePrimaryScore) {
			auto [hasPrefix, prefixID, hasextension, extensionID] = _sessiondata->_excltree->HasPrefixAndShortestExtension(inp);
			if (hasextension) {
				auto parent = _sessiondata->data->LookupFormID<Input>(extensionID);
				if (parent && parent->GetPrimaryScore() > approxthreshold) {
					// do the other stuff
				} else {
					_sessiondata->IncExcludedApproximation();
					_sessiondata->data->DeleteForm(inp);
					_approxTests++;
					profile(TimeProfiling, "Time taken to check input");
					return false;
				}
			} else {
				if (hasPrefix == false) {
					// do the other stuff
				} else {
					_sessiondata->IncGeneratedWithPrefix();
					_tests++;
					if (prefixID != 0) {
						auto ptr = _sessiondata->data->LookupFormID<Input>(prefixID);
						if (ptr) {
							{
								Utility::SpinLock lock(_activeInputsFlag);
								_activeInputs.insert(ptr);
							}
							ptr->SetFlag(Form::FormFlags::DoNotFree);
							if (ptr->derive)
								ptr->derive->SetFlag(Form::FormFlags::DoNotFree);
						}
					}
					if (inp->test)
						_sessiondata->data->DeleteForm(inp->test);
					if (inp->derive)
						_sessiondata->data->DeleteForm(inp->derive);
					_sessiondata->data->DeleteForm(inp);
					_prefixTests++;
					profile(TimeProfiling, "Time taken to check input");
					return false;
				}
			}
		} else {
			FormID prefixID = 0;
			bool hasPrefix = _sessiondata->_excltree->HasPrefix(inp, prefixID);
			if (hasPrefix == false) {
				// do the other stuff
			} else {
				// if we are in reproduction mode we actually want to get inputs that have prefixes into our data set again
				if (_params->GetGoal() == DDGoal::ReproduceResult)
				{

				} else {
					_sessiondata->IncGeneratedWithPrefix();
					_tests++;
					if (prefixID != 0) {
						auto ptr = _sessiondata->data->LookupFormID<Input>(prefixID);
						if (ptr) {
							{
								Utility::SpinLock lock(_activeInputsFlag);
								_activeInputs.insert(ptr);
							}
							ptr->SetFlag(Form::FormFlags::DoNotFree);
							if (ptr->derive)
								ptr->derive->SetFlag(Form::FormFlags::DoNotFree);
						}
					}
					if (inp->test)
						_sessiondata->data->DeleteForm(inp->test);
					if (inp->derive)
						_sessiondata->data->DeleteForm(inp->derive);
					_sessiondata->data->DeleteForm(inp);
					_prefixTests++;
					profile(TimeProfiling, "Time taken to check input");
					return false;
				}
			}
		}

		// try to find derivation tree for our input
		inp->derive = _sessiondata->data->CreateForm<DerivationTree>();
		inp->derive->SetFlag(Form::FormFlags::DoNotFree);
		LockHolder<DerivationTree> inputdevlock(inp->derive);
		//inp->derive->SetFlag(Form::FormFlags::DoNotFree);
		inp->derive->SetInputID(inp->GetFormID());

		auto segments = inp->GetParentSplits();
		_sessiondata->_grammar->Extract(parentinp->derive, inp->derive, segments, parentinp->Length(), inp->GetParentSplitComplement());

		if (inp->derive->_valid == false) {
			// the input cannot be derived from the given grammar
			logwarn("The split cannot be derived from the grammar.");
			_sessiondata->data->DeleteForm(inp->derive);
			_sessiondata->data->DeleteForm(inp);
			_invalidTests++;
			profile(TimeProfiling, "{}: Time taken to check input", Utility::PrintForm(inp->derive));
			return false;
		}
		inputlock.~LockHolder();
		inputdevlock.~LockHolder();

		inp->SetGenerated();
		inp->SetGenerationTime(_sessiondata->data->GetRuntime());
		inp->SetGenerationID(_sessiondata->GetCurrentGenerationID());
		profile(TimeProfiling, "{}: Time taken to check input", Utility::PrintForm(inp->derive));
		return true;
	}

	std::shared_ptr<Input> DeltaController::GetComplement(int32_t begin, int32_t end, double approxthreshold, std::shared_ptr<Input> parent)
	{
		StartProfiling;

		ReadLockHolder<Input> parentlock(parent);
		ReadLockHolder<DerivationTree> parentdevlock(parent->derive);
		profile(TimeProfiling, "{}: Time taken to get locks", Utility::PrintForm(parent));

		DeltaInformation dcmpl;
		dcmpl.positionbegin = (int32_t)begin;
		dcmpl.length = (int32_t)end;
		dcmpl.complement = true;

		if ((int32_t)parent->Length() - dcmpl.length < _sessiondata->_settings->dd.executeAboveLength)
			return {};


		std::vector<std::pair<int64_t, int64_t>> psplitinfo;
		// if the input is not the orig input get its parent split information
		// if it is the orig input we want to reset the counting
		if (parent->GetFormID() != _origInput->GetFormID())
			psplitinfo = parent->GetParentSplits();
		RangeCalculator<int64_t> calc(&psplitinfo, _origInput->Length());
		auto ranges = calc.GetNewRangesWithout((int64_t)begin, (int64_t)end);
		profile(TimeProfiling, "{}: Got new ranges", Utility::PrintForm(parent));

		auto inp = _sessiondata->data->CreateForm<Input>();
		inp->SetFlag(Form::FormFlags::DoNotFree);
		//inp->SetParentSplitInformation(parent->GetFormID(), { { dcmpl.positionbegin, dcmpl.length } }, dcmpl.complement);
		inp->SetParentSplitInformation(_origInput->GetFormID(), ranges, dcmpl.complement);

		// extract the new input first so we can check against the exclusion tree
		size_t count = 0;
		auto itr = parent->begin();
		while (itr != parent->end()) {
			// if the current position is less then the beginning of the split itself, or if it is after the split
			// sequence is over
			if (count < dcmpl.positionbegin || count >= (size_t)(dcmpl.positionbegin + dcmpl.length))
				inp->AddEntry(*itr);
			count++;
			itr++;
		}

		if (inp->GetSequenceLength() > parent->GetSequenceLength())
			logcritical("ooooopss");
		profile(TimeProfiling, "{}: Time taken for complement generation", Utility::PrintForm(parent));
		if (CheckInput(_origInput, inp, approxthreshold))
			return inp;
		else
			return {};
	}

	void DeltaController::AddTests(std::vector<std::shared_ptr<Input>>& inputs)
	{
		int32_t fails = 0;
		for (int32_t i = 0; i < (int32_t)inputs.size(); i++) {
			if (inputs[i]) {
				if (_activetests < _sessiondata->_settings->dd.batchprocessing || _sessiondata->_settings->dd.batchprocessing == 0 /*disabled*/) {
					if (DoTest(inputs[i], genCompData.batchident, genCompData.tasks) == false)
						fails++;
				} else
					_waitingTests.push_back(inputs[i]);

			} else
				fails++;
		}
		_remainingtests -= fails;
	}

	bool DeltaController::DoTest(std::shared_ptr<Input> input, uint64_t batchident, std::shared_ptr<Tasks> tasks)
	{
		//if (!input)
		//	return false;
		auto call = dynamic_pointer_cast<Functions::DDTestCallback>(Functions::DDTestCallback::Create());
		call->_DDcontroller = _self;
		call->_input = input;
		call->_sessiondata = _sessiondata;
		call->_batchident = batchident;
		call->_batchtasks = tasks;
		// add the tests bypassing regular tests so we can get this done with as fast as possible
		if (_sessiondata->_exechandler->AddTest(input, call, _params->bypassTests) == false) {
			_sessiondata->IncAddTestFails();
			_sessiondata->data->DeleteForm(input->derive);
			_sessiondata->data->DeleteForm(input);
			call->Dispose();
			return false;
		} else {
			Utility::SpinLock lock(_activeInputsFlag);
			_activeInputs.insert(input);
			_activetests++;
		}
		return true;
	}

	std::vector<std::shared_ptr<Input>> DeltaController::GenerateComplements(std::vector<DeltaInformation>& splitinfo)
	{
		StartProfiling;
		double approxthreshold = _origInput->GetPrimaryScore() - _origInput->GetPrimaryScore() * _sessiondata->_settings->dd.approximativeExecutionThreshold;
		std::vector<std::shared_ptr<Input>> complements;
		for (int32_t i = 0; i < (int32_t)splitinfo.size(); i++) {
			auto inp = GetComplement((int32_t)splitinfo[i].positionbegin, (int32_t)splitinfo[i].length, approxthreshold, _input);
			if (inp)
				complements.push_back(inp);
		}
		profile(TimeProfiling, "Time taken for complement generation.");
		return complements;
	}

	void DeltaController::GenerateComplements_Async(std::vector<DeltaInformation>& splitinfo)
	{
		StartProfiling;
		{
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			double approxthreshold = _origInput->GetPrimaryScore() - _origInput->GetPrimaryScore() * _sessiondata->_settings->dd.approximativeExecutionThreshold;
			std::vector<std::shared_ptr<Input>> complements;
			for (int32_t i = 0; i < (int32_t)splitinfo.size(); i++) {
				auto callback = dynamic_pointer_cast<Functions::DDGenerateComplementCallback>(Functions::DDGenerateComplementCallback::Create());
				callback->_DDcontroller = _self;
				callback->_begin = (int32_t)splitinfo[i].positionbegin;
				callback->_length = (int32_t)splitinfo[i].length;
				callback->_approxthreshold = approxthreshold;
				callback->_batchident = genCompData.batchident;
				callback->_input = _input;
				callback->_batchtasks = genCompData.tasks;
				genCompData.testqueue.push_back(callback);
			}
		}
		StandardGenerateNextLevel_End();
		profile(TimeProfiling, "Time taken for complement generation initialization.");
	}

	void DeltaController::GenerateComplements_Async_Callback(int32_t begin, int32_t length, double approx, uint64_t batchident, std::shared_ptr<Input> parent, std::shared_ptr<DeltaDebugging::Tasks> tasks)
	{
		// taint changed
		SetChanged();
		if (batchident != genCompData.batchident) {
			// do nothing if the input we are supposed to handle is from an older batch, just discard
			_skippedTests++;
			return;
		}
		if (parent)  // skip if parent is for some reason empty
		{
			auto inp = GetComplement(begin, length, approx, parent);
			if (inp) {
				if (DoTest(inp, batchident, tasks))  // if the test is valid and is being executed just return
					return;
			}
		}
		// if the test is not valid or cannot be executed generate a new test for execution
		std::shared_ptr<Functions::BaseFunction> job;
		{
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			if (genCompData.testqueue.empty() == false) {
				job = genCompData.testqueue.front();
				if (genCompData.testqueue.empty() == false) {
					genCompData.testqueue.pop_front();
				}
				if (job)
					genCompData.tasks.load()->tasks++;
			}
		}
		tasks->tasks--;
		loginfo("{}, tasks --: {}", Utility::PrintForm(parent), tasks->tasks.load());
		if (job)
		{
			// if job has been found
			_sessiondata->_controller->AddTask(job);
		} else {
			std::unique_lock<std::mutex> guard(_completedTestsLock);
			if (genCompData.tasks.load()->tasks.load() <= 0 /*&& _activetests == 0 */) {
				genCompData.active = false;
				// the current stage has finished
				// get out of light callback so we aren't blocking vital tasks
				auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
				callback->_DDcontroller = _self;
				_sessiondata->_controller->AddTask(callback);
				return;
			}
		}
	}

	void DeltaController::StandardGenerateFirstLevel()
	{
		// level is the number of subsets to generate from _input, it will increase every round of tests
		_level = 2;

		if (_input->Length() >= 2) {
			StandardGenerateNextLevel_Async();
		}
	}

	void DeltaController::StandardGenerateNextLevel()
	{
		StartProfiling;

		// insurance
		if (_input->GetGenerated() == false || _input->GetSequenceLength() == 0) {
			// we are trying to add an _input that hasn't been generated or regenerated
			// try the generate it and if it succeeds add the test
			_input->SetGenerated(false);
			SessionFunctions::GenerateInput(_input, _sessiondata);
			if (_input->GetGenerated() == false)
				Finish();
		}

		std::vector<DeltaInformation> splitinfo;
		auto splits = GenerateSplits(_level, splitinfo);
		auto complements = GenerateComplements(splitinfo);

		_stopbatch = false;
		
		double approxthreshold = _origInput->GetPrimaryScore() - _origInput->GetPrimaryScore() * _sessiondata->_settings->dd.approximativeExecutionThreshold;
		// set internals
		_tests = 0;
		_remainingtests = (int32_t)splits.size() + (int32_t)complements.size();
		// temp
		AddTests(splits);
		AddTests(complements);
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

	void DeltaController::StandardGenerateNextLevel_Async()
	{
		{
			std::unique_lock<std::mutex> guard(_completedTestsLock);
			genCompData.Reset();
			genCompData.active = true;
			genCompData.tasks = std::make_shared<Tasks>();
			genCompData.tasks.load()->tasks = 0;
		}
		__NextGenTime = std::chrono::steady_clock::now();
		// insurance
		if (_input->GetGenerated() == false || _input->GetSequenceLength() == 0) {
			// we are trying to add an _input that hasn't been generated or regenerated
			// try the generate it and if it succeeds add the test
			_input->SetGenerated(false);
			SessionFunctions::GenerateInput(_input, _sessiondata);
			if (_input->GetGenerated() == false)
				Finish();
		}
		ReadLockHolder<Input> holder(_input);

		std::vector<DeltaInformation> splitinfo;
		GenerateSplits_Async(_level);
	}

	void DeltaController::StandardGenerateNextLevel_Inter()
	{
		GenerateComplements_Async(genCompData.dinfo);
	}

	void DeltaController::StandardGenerateNextLevel_End()
	{
		StartProfiling;

		_stopbatch = false;
		
		{
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			// check if all the input results are already known
			if (genCompData.testqueue.size() == 0) {
				// start new callback to avoid blocking for too long, and to avoid reentry into the lock as
				// the Evaluation Methods are blocking
				auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
				callback->_DDcontroller = _self;
				_sessiondata->_controller->AddTask(callback);
				profile(__NextGenTime, "Time taken to generate next dd level.");
				return;
			}
		}

		// if there are inputs to be generated, add them to generation
		// set internals
		_tests = 0;
		
		// instead of adding the tests to the queue, we are gonna follow a more advanced system
		// in which we add [batchsize] tests to the generation queue and then actually run
		// the tests once they have been generated
		
		// reset tests active in this batch
		_activetests = 0;
		auto tasks = genCompData.tasks.load();
		tasks->tasks = 0;
		int32_t active = 0;
		if (_sessiondata->_settings->dd.batchprocessing > 0) {
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			while (active < _sessiondata->_settings->dd.batchprocessing && genCompData.testqueue.size() > 0) {
				tasks->tasks++;
				active++;
				auto task = genCompData.testqueue.front();
				if (task)
					_sessiondata->_controller->AddTask(task);
				if (!genCompData.testqueue.empty())
					genCompData.testqueue.pop_front();
			}
		} else {
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			for (auto ptr : genCompData.testqueue) {
				if (ptr) {
					tasks->tasks++;
					active++;
					_sessiondata->_controller->AddTask(ptr);
				}
			}
			genCompData.testqueue.clear();
		}
		if (active == 0) {
			loginfo("{} no tests", genCompData.batchident);
			// start new callback to avoid blocking for too long, and to avoid reentry into the lock as
			// the Evaluation Methods are blocking
			auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
			callback->_DDcontroller = _self;
			_sessiondata->_controller->AddTask(callback);
			profile(__NextGenTime, "Time taken to generate next level.");
			return;
		}
		//AddTests(genCompData.splits);
		//AddTests(genCompData.complements);

		profile(__NextGenTime, "Time taken to generate next dd level.");
	}

	bool DeltaController::StandardEvaluateInput(std::shared_ptr<Input> input)
	{
		// taint changed
		SetChanged();
		if (!input)
			return false;
		switch (_params->GetGoal()) {
		case DDGoal::None:
			return false;
			break;
		case DDGoal::ReproduceResult:
			{
				auto oracle = this->_origInput->GetOracleResult();
				if (oracle == input->GetOracleResult()) {
					return true;  // doesn't reproduce, return
				}
				// we have found an input that reproduces the result and thus we return true, as all requirements are fulfilled
				else {
					input->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->test)
						input->test->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->derive)
						input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
					// free memory to save reclaim time
					input->FreeMemory();
					if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
						if (input->derive)
							input->derive->FreeMemory();
						if (input->test)
							input->test->FreeMemory();
					}
					return false;
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
						return lhs->GetPrimaryScore() == rhs->GetPrimaryScore() ? lhs->GetSecondaryScore() > rhs->GetSecondaryScore() : lhs->GetPrimaryScore() > rhs->GetPrimaryScore();
					}
				};

				double l = lossPrimary(input);
				if (l < mpparams->acceptableLoss && 
					lossPrimaryAbsolute(input) < mpparams->acceptableLossAbsolute) {
					// we have found an input that reproduces the result and thus we return true, as all requirements are fulfilled
					return true;
				} else {
					input->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->test)
						input->test->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->derive)
						input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
					// free memory to save reclaim time
					input->FreeMemory();
					if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
						if (input->derive)
							input->derive->FreeMemory();
						if (input->test)
							input->test->FreeMemory();
					}
					return false;
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
						return lhs->GetSecondaryScore() == rhs->GetSecondaryScore() ? lhs->GetPrimaryScore() > rhs->GetSecondaryScore() : lhs->GetSecondaryScore() > rhs->GetSecondaryScore();
					}
				};

				double l = lossSecondary(input);
				if (l < mpparams->acceptableLossSecondary && 
					lossSecondaryAbsolute(input) < mpparams->acceptableLossSecondaryAbsolute) {
					// we have found an input that reproduces the result and thus we return true, as all requirements are fulfilled
					return true;
				} else {
					input->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->test)
						input->test->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->derive)
						input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
					// free memory to save reclaim time
					input->FreeMemory();
					if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
						if (input->derive)
							input->derive->FreeMemory();
						if (input->test)
							input->test->FreeMemory();
					}
					return false;
				}
			}
			break;
		case DDGoal::MaximizeBothScores:
			{
				MaximizeBothScores* mbparams = (MaximizeBothScores*)_params;

				struct PrimaryLess
				{
					bool operator()(const std::shared_ptr<Input>& lhs, const std::shared_ptr<Input>& rhs) const
					{
						return lhs->GetPrimaryScore() == rhs->GetPrimaryScore() ? lhs->GetSecondaryScore() > rhs->GetSecondaryScore() : lhs->GetPrimaryScore() > rhs->GetPrimaryScore();
					}
				};

				double priml = lossPrimary(input);
				double seconl = lossSecondary(input);
				if (priml < mbparams->acceptableLossPrimary && 
					lossPrimaryAbsolute(input) < mbparams->acceptableLossPrimaryAbsolute && 
					seconl < mbparams->acceptableLossSecondary && 
					lossSecondaryAbsolute(input) < mbparams->acceptableLossSecondaryAbsolute) {
					// we have found an input that reproduces the result and thus we return true, as all requirements are fulfilled
					return true;
				} else {
					input->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->test)
						input->test->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->derive)
						input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
					// free memory to save reclaim time
					input->FreeMemory();
					if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
						if (input->derive)
							input->derive->FreeMemory();
						if (input->test)
							input->test->FreeMemory();
					}
					return false;
				}
			}
			break;
		}
		return false;
	}

	void DeltaController::StandardEvaluateLevel()
	{
		// increase batch ident here to ensure that no inputs that finish after right now are added to our lists
		genCompData.batchident++;
		// lambda that clears the DoNotFree flags on inputs that are no longer needed
		auto clearFlags = [this]() {
			for (auto ptr : _completedTests) {
				// if the ptr is not in the list of results
				if (_results.find(ptr) == _results.end()) {
					//ptr->UnsetFlag(Form::FormFlags::DoNotFree);
					//if (ptr->derive)
					//	ptr->derive->UnsetFlag(Form::FormFlags::DoNotFree);
				}
			}
			_completedTests.clear();
			for (auto ptr : _activeInputs) {
				// if the ptr is not in the list of results
				if (_results.find(ptr) == _results.end()) {
					//ptr->UnsetFlag(Form::FormFlags::DoNotFree);
					//if (ptr->derive)
					//	ptr->derive->UnsetFlag(Form::FormFlags::DoNotFree);
				}
			}
			_activeInputs.clear();
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
					auto itr = _completedTests.begin();
					while (itr != _completedTests.end()) {
						if (oracle == (*itr)->GetOracleResult()) {
							res.push_back(*itr);
							_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ 0, 0, _level });
						}
						itr++;
					}
					return res;
				};
				auto rinputs = reproc();

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
					StandardGenerateNextLevel_Async();
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
							int idx = dist(randan);
							_input = rinputs[idx];
							// adjust level
							if (!_input->GetParentSplitComplement())
								_level = 2;
							else
								_level = std::max(_level - 1, 2);
							// run next generation if budget hasn't been exceeded
							if (_params->budget != 0 && _params->budget <= _totaltests) {
								_finished = true;
								Finish();
							} else {
								StandardGenerateNextLevel_Async();
								// free passing inputs we don't need anymore
								for (size_t i = 0; i < rinputs.size(); i++) {
									if (i == idx)
										continue;
									rinputs[i]->UnsetFlag(Form::FormFlags::DoNotFree);
									rinputs[i]->test->UnsetFlag(Form::FormFlags::DoNotFree);
									rinputs[i]->derive->UnsetFlag(Form::FormFlags::DoNotFree);
									// free memory to save reclaim time
									rinputs[i]->FreeMemory();
									if (rinputs[i]->GetGenerated() == false && rinputs[i]->test && rinputs[i]->test->IsValid() == false) {
										if (rinputs[i]->derive)
											rinputs[i]->derive->FreeMemory();
										if (rinputs[i]->test)
											rinputs[i]->test->FreeMemory();
									}
								}
							}
						}
						break;
					case DDGoal::MaximizeBothScores:
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
							if (!_input->GetParentSplitComplement())
								_level = 2;
							else
								_level = std::max(_level - 1, 2);
							// run next generation if budget hasn't been exceeded
							if (_params->budget != 0 && _params->budget <= _totaltests) {
								_finished = true;
								Finish();
							} else {
								StandardGenerateNextLevel_Async();
								// free passing inputs we don't need anymore
								for (size_t i = 1; i < rinputs.size(); i++) {
									rinputs[i]->UnsetFlag(Form::FormFlags::DoNotFree);
									rinputs[i]->test->UnsetFlag(Form::FormFlags::DoNotFree);
									rinputs[i]->derive->UnsetFlag(Form::FormFlags::DoNotFree);
									// free memory to save reclaim time
									rinputs[i]->FreeMemory();
									if (rinputs[i]->GetGenerated() == false && rinputs[i]->test && rinputs[i]->test->IsValid() == false) {
										if (rinputs[i]->derive)
											rinputs[i]->derive->FreeMemory();
										if (rinputs[i]->test)
											rinputs[i]->test->FreeMemory();
									}
								}
							}
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
							if (!_input->GetParentSplitComplement())
								_level = 2;
							else
								_level = std::max(_level - 1, 2);
							// run next generation if budget hasn't been exceeded
							if (_params->budget != 0 && _params->budget <= _totaltests) {
								_finished = true;
								Finish();
							} else {
								StandardGenerateNextLevel_Async();
								// free passing inputs we don't need anymore
								for (size_t i = 1; i < rinputs.size(); i++) {
									rinputs[i]->UnsetFlag(Form::FormFlags::DoNotFree);
									rinputs[i]->test->UnsetFlag(Form::FormFlags::DoNotFree);
									rinputs[i]->derive->UnsetFlag(Form::FormFlags::DoNotFree);
									// free memory to save reclaim time
									rinputs[i]->FreeMemory();
									if (rinputs[i]->GetGenerated() == false && rinputs[i]->test && rinputs[i]->test->IsValid() == false) {
										if (rinputs[i]->derive)
											rinputs[i]->derive->FreeMemory();
										if (rinputs[i]->test)
											rinputs[i]->test->FreeMemory();
									}
								}
							}
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
						return lhs->GetPrimaryScore() == rhs->GetPrimaryScore() ? lhs->GetSecondaryScore() > rhs->GetSecondaryScore() : lhs->GetPrimaryScore() > rhs->GetPrimaryScore();
					}
				};

				// automatically sorted based on the primary score
				// begin() -> end() == higher score -> lower score
				auto sortprimary = [this]() {
					std::multiset<std::shared_ptr<Input>, PrimaryLess> res;
					auto itr = _completedTests.begin();
					while (itr != _completedTests.end()) {
						res.insert(*itr);
						itr++;
					}
					return res;
				};
				auto rinputs = sortprimary();

				std::vector<std::shared_ptr<Input>> passing;
				std::vector<double> ploss;
				auto itr = rinputs.begin();
				while (itr != rinputs.end()) {
					double l = lossPrimary(*itr);
					if (l < mpparams->acceptableLoss && lossPrimaryAbsolute(*itr) < mpparams->acceptableLossAbsolute) {
						passing.push_back(*itr);
						ploss.push_back(l);
						_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ l, 0.0f, _level });
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
					// run next generation if budget hasn't been exceeded
					if (_params->budget != 0 && _params->budget <= _totaltests) {
						_finished = true;
						Finish();
					} else
						StandardGenerateNextLevel_Async();
				} else {
					// set new base _input
					_input = passing[0];
					if (_bestScore.first < _input->GetPrimaryScore())
						_bestScore = { _input->GetPrimaryScore(), 0.0f };
					// adjust level
					if (!_input->GetParentSplitComplement())
						_level = 2;
					else
						_level = std::max(_level - 1, 2);
					// run next generation if budget hasn't been exceeded
					if (_params->budget != 0 && _params->budget <= _totaltests) {
						_finished = true;
						Finish();
					} else {
						StandardGenerateNextLevel_Async();
						// free passing inputs we don't need anymore
						for (size_t i = 1; i < passing.size(); i++) {
							passing[i]->UnsetFlag(Form::FormFlags::DoNotFree);
							passing[i]->test->UnsetFlag(Form::FormFlags::DoNotFree);
							passing[i]->derive->UnsetFlag(Form::FormFlags::DoNotFree);
							// free memory to save reclaim time
							passing[i]->FreeMemory();
							if (passing[i]->GetGenerated() == false && passing[i]->test && passing[i]->test->IsValid() == false) {
								if (passing[i]->derive)
									passing[i]->derive->FreeMemory();
								if (passing[i]->test)
									passing[i]->test->FreeMemory();
							}
						}
					}
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
						return lhs->GetSecondaryScore() == rhs->GetSecondaryScore() ? lhs->GetPrimaryScore() > rhs->GetSecondaryScore() : lhs->GetSecondaryScore() > rhs->GetSecondaryScore();
					}
				};

				// automatically sorted based on the primary score
				// begin() -> end() == higher score -> lower score
				auto sortprimary = [this]() {
					std::multiset<std::shared_ptr<Input>, SecondaryLess> res;
					auto itr = _completedTests.begin();
					while (itr != _completedTests.end()) {
						res.insert(*itr);
						itr++;
					}
					return res;
				};
				auto rinputs = sortprimary();

				std::vector<std::shared_ptr<Input>> passing;
				std::vector<double> ploss;
				auto itr = rinputs.begin();
				while (itr != rinputs.end()) {
					double l = lossSecondary(*itr);
					if (l < mpparams->acceptableLossSecondary &&
						lossSecondaryAbsolute(*itr) < mpparams->acceptableLossSecondaryAbsolute) {
						passing.push_back(*itr);
						ploss.push_back(l);
						_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ 0.0f, l, _level });
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
					// run next generation if budget hasn't been exceeded
					if (_params->budget != 0 && _params->budget <= _totaltests) {
						_finished = true;
						Finish();
					} else
						StandardGenerateNextLevel_Async();
				} else {
					// set new base _input
					_input = passing[0];
					if (_bestScore.second < _input->GetSecondaryScore())
						_bestScore = { 0.0f, _input->GetSecondaryScore() };
					// adjust level
					if (!_input->GetParentSplitComplement())
						_level = 2;
					else
						_level = std::max(_level - 1, 2);
					// run next generation if budget hasn't been exceeded
					if (_params->budget != 0 && _params->budget <= _totaltests) {
						_finished = true;
						Finish();
					} else {
						StandardGenerateNextLevel_Async();
						// free passing inputs we don't need anymore
						for (size_t i = 1; i < passing.size(); i++) {
							passing[i]->UnsetFlag(Form::FormFlags::DoNotFree);
							passing[i]->test->UnsetFlag(Form::FormFlags::DoNotFree);
							passing[i]->derive->UnsetFlag(Form::FormFlags::DoNotFree);
							// free memory to save reclaim time
							passing[i]->FreeMemory();
							if (passing[i]->GetGenerated() == false && passing[i]->test && passing[i]->test->IsValid() == false) {
								if (passing[i]->derive)
									passing[i]->derive->FreeMemory();
								if (passing[i]->test)
									passing[i]->test->FreeMemory();
							}
						}
					}
				}
			}
			break;
		case DDGoal::MaximizeBothScores:
			{
				MaximizeBothScores* mbparams = (MaximizeBothScores*)_params;

				struct PrimaryLess
				{
					bool operator()(const std::shared_ptr<Input>& lhs, const std::shared_ptr<Input>& rhs) const
					{
						return lhs->GetPrimaryScore() == rhs->GetPrimaryScore() ? lhs->GetSecondaryScore() > rhs->GetSecondaryScore() : lhs->GetPrimaryScore() > rhs->GetPrimaryScore();
					}
				};

				// automatically sorted based on the primary score
				// begin() -> end() == higher score -> lower score
				auto sortprimary = [this]() {
					std::multiset<std::shared_ptr<Input>, PrimaryLess> res;
					auto itr = _completedTests.begin();
					while (itr != _completedTests.end()) {
						res.insert(*itr);
						itr++;
					}
					return res;
				};
				auto rinputs = sortprimary();

				std::vector<std::shared_ptr<Input>> passing;
				std::vector<std::pair<double, double>> ploss;
				auto itr = rinputs.begin();
				while (itr != rinputs.end()) {
					double priml = lossPrimary(*itr);
					double seconl = lossSecondary(*itr);
					if (priml < mbparams->acceptableLossPrimary &&
						lossPrimaryAbsolute(*itr) < mbparams->acceptableLossPrimaryAbsolute &&
						seconl < mbparams->acceptableLossSecondary && 
						lossSecondaryAbsolute(*itr) < mbparams->acceptableLossSecondaryAbsolute) {
						passing.push_back(*itr);
						ploss.push_back({priml, seconl});
						_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ priml, seconl, _level });
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
					// run next generation if budget hasn't been exceeded
					if (_params->budget != 0 && _params->budget <= _totaltests) {
						_finished = true;
						Finish();
					} else
						StandardGenerateNextLevel_Async();
				} else {
					// set new base _input
					_input = passing[0];
					if (_bestScore.first < _input->GetPrimaryScore())
						_bestScore = { _input->GetPrimaryScore(), 0.0f };
					// adjust level
					if (!_input->GetParentSplitComplement())
						_level = 2;
					else
						_level = std::max(_level - 1, 2);
					// run next generation if budget hasn't been exceeded
					if (_params->budget != 0 && _params->budget <= _totaltests) {
						_finished = true;
						Finish();
					} else {
						StandardGenerateNextLevel_Async();
						// free passing inputs we don't need anymore
						for (size_t i = 1; i < passing.size(); i++) {
							passing[i]->UnsetFlag(Form::FormFlags::DoNotFree);
							passing[i]->test->UnsetFlag(Form::FormFlags::DoNotFree);
							passing[i]->derive->UnsetFlag(Form::FormFlags::DoNotFree);
							// free memory to save reclaim time
							passing[i]->FreeMemory();
							if (passing[i]->GetGenerated() == false && passing[i]->test && passing[i]->test->IsValid() == false) {
								if (passing[i]->derive)
									passing[i]->derive->FreeMemory();
								if (passing[i]->test)
									passing[i]->test->FreeMemory();
							}
						}
					}
				}
			}
			break;
		}
	}

	std::vector<std::shared_ptr<Input>> DeltaController::ScoreProgressGenerateComplements(int32_t level)
	{
		StartProfiling;
		double approxthreshold = _origInput->GetPrimaryScore() - _origInput->GetPrimaryScore() * _sessiondata->_settings->dd.approximativeExecutionThreshold;
		std::vector<std::shared_ptr<Input>> complements;
		RangeIterator<size_t> rangeIterator(&_inputRanges, _sessiondata->_settings->dd.skipoptions, _skipRanges);
		size_t size = rangeIterator.GetLength() / level;
		
		auto ranges = rangeIterator.GetRangesAbove(size);
		for (size_t i = 0; i < ranges.size(); i++)
		{
			auto [begin, length] = ranges[i];

			auto inp = GetComplement((int32_t)begin, (int32_t)length, approxthreshold, _input);
			if (inp)
				complements.push_back(inp);
		}


		profile(TimeProfiling, "Time taken for complement generation");
		return complements;
	}
	void DeltaController::ScoreProgressGenerateComplements_Async(int32_t level)
	{
		loginfo("{} begin", genCompData.batchident);
		loginfo("1");
		double approxthreshold = _origInput->GetPrimaryScore() - _origInput->GetPrimaryScore() * _sessiondata->_settings->dd.approximativeExecutionThreshold;
		loginfo("2, approx: {}", approxthreshold);
		std::vector<std::shared_ptr<Input>> complements;
		loginfo("3, inputranges: {}, skip: {}", _inputRanges.size(), _skipRanges);
		RangeIterator<size_t> rangeIterator(&_inputRanges, _sessiondata->_settings->dd.skipoptions, _skipRanges);
		loginfo("4, range Length:{}", rangeIterator.GetLength());
		loginfo("5, range max range:{}", rangeIterator.GetMaxRange());
		size_t size = rangeIterator.GetLength() / level;
		loginfo("6, size: {}",size);

		{
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			auto ranges = rangeIterator.GetRangesAbove(size);
			loginfo("{} iterate", genCompData.batchident);
			for (size_t i = 0; i < ranges.size(); i++) {
				auto [begin, length] = ranges[i];

				auto callback = dynamic_pointer_cast<Functions::DDGenerateComplementCallback>(Functions::DDGenerateComplementCallback::Create());
				callback->_DDcontroller = _self;
				callback->_begin = (int32_t)begin;
				callback->_length = (int32_t)length;
				callback->_approxthreshold = approxthreshold;
				callback->_batchident = genCompData.batchident;
				callback->_input = _input;
				callback->_batchtasks = genCompData.tasks;
				genCompData.testqueue.push_back(callback);
			}
		}
		loginfo("{} end", genCompData.batchident);
		ScoreProgressGenerateNextLevel_End();
	}

	void DeltaController::ScoreProgressGenerateFirstLevel()
	{
		// level is the max size of ranges that can be removed from an input
		_level = 2;

		ScoreProgressGenerateNextLevel_Async();
	}

	void DeltaController::ScoreProgressGenerateNextLevel()
	{
		StartProfiling;

		// insurance
		if (_input->GetGenerated() == false || _input->GetSequenceLength() == 0) {
			// we are trying to add an _input that hasn't been generated or regenerated
			// try the generate it and if it succeeds add the test
			_input->SetGenerated(false);
			SessionFunctions::GenerateInput(_input, _sessiondata);
			if (_input->GetGenerated() == false)
				Finish();
		}

		_stopbatch = false;

		auto complements = ScoreProgressGenerateComplements(_level);
		// set internals
		_tests = 0;
		_remainingtests = (int32_t)complements.size();

		AddTests(complements);

		// check if all the input results are already known
		if (_remainingtests == 0)
		{
			// start new callback to avoid blocking for too long, and to avoid reentry into the lock as
			// the Evaluation Methods are blocking
			auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
			callback->_DDcontroller = _self;
			_sessiondata->_controller->AddTask(callback);
		}
		profile(TimeProfiling, "Time taken to generate next dd level.");
	}

	void DeltaController::ScoreProgressGenerateNextLevel_Async()
	{
		loginfo("{}", genCompData.batchident);
		{
			std::unique_lock<std::mutex> guard(_completedTestsLock);
			genCompData.Reset();
			genCompData.active = true;
			genCompData.tasks = std::make_shared<Tasks>();
			genCompData.tasks.load()->tasks = 0;
		}
		__NextGenTime = std::chrono::steady_clock::now();

		// insurance
		if (_input->GetGenerated() == false || _input->GetSequenceLength() == 0) {
			// we are trying to add an _input that hasn't been generated or regenerated
			// try the generate it and if it succeeds add the test
			_input->SetGenerated(false);
			SessionFunctions::GenerateInput(_input, _sessiondata);
			if (_input->GetGenerated() == false)
				Finish();
		}

		_stopbatch = false;
		ScoreProgressGenerateComplements_Async(_level);
	}

	void DeltaController::ScoreProgressGenerateNextLevel_End()
	{
		loginfo("{} enter", genCompData.batchident);
		// check if there are no inputs to be generated
		{
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			if (genCompData.testqueue.size() == 0) {
				loginfo("{} no tests", genCompData.batchident);
				// start new callback to avoid blocking for too long, and to avoid reentry into the lock as
				// the Evaluation Methods are blocking
				auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
				callback->_DDcontroller = _self;
				_sessiondata->_controller->AddTask(callback);
				profile(__NextGenTime, "Time taken to generate next level.");
				return;
			}
		}
		// if there are inputs to be generated, add them to generation

		// set internals

		loginfo("{} start tests", genCompData.batchident);
		// reset tests active in this batch
		_activetests = 0;
		auto tasks = genCompData.tasks.load();
		tasks->tasks = 0;
		_tests = 0;
		int32_t active = 0;
		if (_sessiondata->_settings->dd.batchprocessing > 0) {
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			while (active < _sessiondata->_settings->dd.batchprocessing && genCompData.testqueue.size() > 0) {
				tasks->tasks++;
				active++;
				auto task = genCompData.testqueue.front();
				if (task)
					_sessiondata->_controller->AddTask(task);
				if (!genCompData.testqueue.empty())
					genCompData.testqueue.pop_front();
			}
		} else {
			std::unique_lock<std::mutex> guard(genCompData.testqueuelock);
			for (auto ptr : genCompData.testqueue) {
				if (ptr) {
					tasks->tasks++;
					active++;
					_sessiondata->_controller->AddTask(ptr);
				}
			}
			genCompData.testqueue.clear();
		}
		if (active == 0) {
			loginfo("{} no tests", genCompData.batchident);
			// start new callback to avoid blocking for too long, and to avoid reentry into the lock as
			// the Evaluation Methods are blocking
			auto callback = dynamic_pointer_cast<Functions::DDEvaluateExplicitCallback>(Functions::DDEvaluateExplicitCallback::Create());
			callback->_DDcontroller = _self;
			_sessiondata->_controller->AddTask(callback);
			profile(__NextGenTime, "Time taken to generate next level.");
			return;
		}
		loginfo("{} end", genCompData.batchident);
		//AddTests(genCompData.complements);
		profile(__NextGenTime, "Time taken to generate next level.");
	}

	bool DeltaController::ScoreProgressEvaluateInput(std::shared_ptr<Input> input)
	{
		switch (_params->GetGoal()) {
		case DDGoal::MaximizeSecondaryScore:
			{
				MaximizeSecondaryScore* msparams = (MaximizeSecondaryScore*)_params;
				double l = lossSecondary(input);
				if (l < msparams->acceptableLossSecondary && lossSecondaryAbsolute(input) < msparams->acceptableLossSecondaryAbsolute) {
					return true;
				}
				else
				{
					input->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->test)
						input->test->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->derive)
						input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
					// free memory to save reclaim time
					input->FreeMemory();
					if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
						if (input->derive)
							input->derive->FreeMemory();
						if (input->test)
							input->test->FreeMemory();
					}
					return false;
				}
			}
			break;
		case DDGoal::MaximizePrimaryScore:
			{
				MaximizePrimaryScore* mpparams = (MaximizePrimaryScore*)_params;
				double l = lossPrimary(input);
				if (l < mpparams->acceptableLoss && lossPrimaryAbsolute(input) < mpparams->acceptableLossAbsolute) {
					return true;
				}
				else {
					input->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->test)
						input->test->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->derive)
						input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
					// free memory to save reclaim time
					input->FreeMemory();
					if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
						if (input->derive)
							input->derive->FreeMemory();
						if (input->test)
							input->test->FreeMemory();
					}
					return false;
				}
			}
			break;
		case DDGoal::MaximizeBothScores:
			{
				MaximizeBothScores* mbparams = (MaximizeBothScores*)_params;
				double priml = lossPrimary(input);
				double seconl = lossSecondary(input);
				if (priml < mbparams->acceptableLossPrimary &&
					lossPrimaryAbsolute(input) < mbparams->acceptableLossPrimaryAbsolute &&
					seconl < mbparams->acceptableLossSecondary &&
					lossSecondaryAbsolute(input) < mbparams->acceptableLossSecondaryAbsolute) {
					return true;
				} else {
					input->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->test)
						input->test->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->derive)
						input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
					// free memory to save reclaim time
					input->FreeMemory();
					if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
						if (input->derive)
							input->derive->FreeMemory();
						if (input->test)
							input->test->FreeMemory();
					}
					return false;
				}
			}
			break;
		case DDGoal::None:
		case DDGoal::ReproduceResult:
			{
				ReproduceResult* rpparams = (ReproduceResult*)_params;
				auto oracle = _origInput->GetOracleResult();
				if (oracle == (input)->GetOracleResult()) {
					return true;
				} else {
					input->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->test)
						input->test->UnsetFlag(Form::FormFlags::DoNotFree);
					if (input->derive)
						input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
					// free memory to save reclaim time
					input->FreeMemory();
					if (input->GetGenerated() == false && input->test && input->test->IsValid() == false) {
						if (input->derive)
							input->derive->FreeMemory();
						if (input->test)
							input->test->FreeMemory();
					}
					return false;
				}
			}
			break;
		}
		return false;
	}

	void DeltaController::ScoreProgressEvaluateLevel()
	{
		// taint changed
		SetChanged();
		// increase batch ident here to ensure that no inputs that finish after right now are added to our lists
		genCompData.batchident++;
		loginfo("{} begin", genCompData.batchident);
		// lambda that clears the DoNotFree flags on inputs that are no longer needed
		auto clearFlags = [this]() {
			for (auto ptr : _completedTests) {
				// if the ptr is not in the list of results
				if (_results.find(ptr) == _results.end()) {
					//ptr->UnsetFlag(Form::FormFlags::DoNotFree);
					//if (ptr->derive)
					//	ptr->derive->UnsetFlag(Form::FormFlags::DoNotFree);
				}
			}
			_completedTests.clear();
			for (auto ptr : _activeInputs) {
				// if the ptr is not in the list of results
				if (_results.find(ptr) == _results.end()) {
					//ptr->UnsetFlag(Form::FormFlags::DoNotFree);
					//if (ptr->derive)
					//	ptr->derive->UnsetFlag(Form::FormFlags::DoNotFree);
				}
			}
			_activeInputs.clear();
		};
		loginfo("{} try lock", genCompData.batchident);

		std::unique_lock<std::shared_mutex> guard(_lock);

		loginfo("{} has lock", genCompData.batchident);

		struct PrimaryLess
		{
			bool operator()(const std::shared_ptr<Input>& lhs, const std::shared_ptr<Input>& rhs) const
			{
				return lhs->GetPrimaryScore() == rhs->GetPrimaryScore() ? (lhs->Length() == rhs->Length() ? lhs->GetSecondaryScore() > rhs->GetSecondaryScore() : lhs->Length() < rhs->Length()) : lhs->GetPrimaryScore() > rhs->GetPrimaryScore();
			}
		};

		struct SecondaryLess
		{
			bool operator()(const std::shared_ptr<Input>& lhs, const std::shared_ptr<Input>& rhs) const
			{
				return lhs->GetSecondaryScore() == rhs->GetSecondaryScore() ? lhs->GetPrimaryScore() > rhs->GetSecondaryScore() : lhs->GetSecondaryScore() > rhs->GetSecondaryScore();
			}
		};

		struct LengthLess
		{
			bool operator()(const std::shared_ptr<Input>& lhs, const std::shared_ptr<Input>& rhs) const
			{
				//return lhs->Length() < rhs->Length();
				return lhs->Length() == rhs->Length() ? (lhs->GetPrimaryScore() == rhs->GetPrimaryScore() ? lhs->GetSecondaryScore() > rhs->GetSecondaryScore() : lhs->GetPrimaryScore() < rhs->GetPrimaryScore()) : lhs->Length() > rhs->Length();
			}
		};

		// automatically sorted based on the primary score
		// begin() -> end() == higher score -> lower score
		auto sortprimary = [this]() {
			std::multiset<std::shared_ptr<Input>, PrimaryLess> res;
			auto itr = _completedTests.begin();
			while (itr != _completedTests.end()) {
				res.insert(*itr);
				itr++;
			}
			return res;
		};
		auto sortsecondary = [this]() {
			std::multiset<std::shared_ptr<Input>, SecondaryLess> res;
			auto itr = _completedTests.begin();
			while (itr != _completedTests.end()) {
				res.insert(*itr);
				itr++;
			}
			return res;
		};
		auto sortlength = [this]() {
			std::multiset<std::shared_ptr<Input>, LengthLess> res;
			auto itr = _completedTests.begin();
			while (itr != _completedTests.end()) {
				res.insert(*itr);
				itr++;
			}
			return res;
		};

		loginfo("{} sort begin", genCompData.batchident);
		std::vector<std::shared_ptr<Input>> passing;
		std::vector<std::pair<double, double>> ploss;
		switch (_params->GetGoal()) {
		case DDGoal::MaximizeSecondaryScore:
			{
				MaximizeSecondaryScore* msparams = (MaximizeSecondaryScore*)_params;
				auto rinputs = sortsecondary();
				auto itr = rinputs.begin();
				while (itr != rinputs.end()) {
					double l = lossSecondary(*itr);
					if (l < msparams->acceptableLossSecondary &&
						lossSecondaryAbsolute(*itr) < msparams->acceptableLossSecondaryAbsolute) {
						passing.push_back(*itr);
						ploss.push_back({ 0.0f, l });
						_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ l, 0.0f, _level });
					}
					itr++;
				}
			}
			break;
		case DDGoal::MaximizePrimaryScore:
			{
				MaximizePrimaryScore* mpparams = (MaximizePrimaryScore*)_params;
				auto rinputs = sortprimary();
				auto itr = rinputs.begin();
				while (itr != rinputs.end()) {
					double l = lossPrimary(*itr);
					if (l < mpparams->acceptableLoss &&
						lossPrimaryAbsolute(*itr) < mpparams->acceptableLossAbsolute) {
						passing.push_back(*itr);
						ploss.push_back({ l, 0.0f });
						_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ l, 0.0f, _level });
					}
					itr++;
				}
			}
			break;
		case DDGoal::MaximizeBothScores:
			{
				MaximizeBothScores* mbparams = (MaximizeBothScores*)_params;
				auto rinputs = sortprimary();
				auto itr = rinputs.begin();
				while (itr != rinputs.end()) {
					double priml = lossPrimary(*itr);
					double seconl = lossSecondary(*itr);
					if (priml < mbparams->acceptableLossPrimary && 
						lossPrimaryAbsolute(*itr) <mbparams->acceptableLossPrimaryAbsolute && 
						seconl < mbparams->acceptableLossSecondary && 
						lossSecondaryAbsolute(*itr) < mbparams->acceptableLossSecondaryAbsolute) {
						passing.push_back(*itr);
						ploss.push_back({ priml, seconl });
						_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ priml, seconl, _level });
					}
					itr++;
				}
			}
			break;
		case DDGoal::ReproduceResult:
			{
				ReproduceResult* rpparams = (ReproduceResult*)_params;
				switch (rpparams->secondarygoal) {
				case DDGoal::MaximizePrimaryScore:
					{
						auto rinputs = sortprimary();
						auto oracle = _origInput->GetOracleResult();
						auto itr = rinputs.begin();
						while (itr != rinputs.end()) {
							double l = lossPrimary(*itr);
							if (oracle == (*itr)->GetOracleResult()) {
								passing.push_back(*itr);
								ploss.push_back({ l, 0.0f });
								_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ l, 0, _level });
							}
							itr++;
						}
					}
					break;
				case DDGoal::MaximizeSecondaryScore:
					{
						auto rinputs = sortsecondary();
						auto oracle = _origInput->GetOracleResult();
						auto itr = rinputs.begin();
						while (itr != rinputs.end()) {
							double l = lossSecondary(*itr);
							if (oracle == (*itr)->GetOracleResult()) {
								passing.push_back(*itr);
								ploss.push_back({ 0.0f, l });
								_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ l, 0, _level });
							}
							itr++;
						}
					}
					break;
				case DDGoal::MaximizeBothScores:
					{
						MaximizeBothScores* mbparams = (MaximizeBothScores*)_params;
						auto rinputs = sortprimary();
						auto oracle = _origInput->GetOracleResult();
						auto itr = rinputs.begin();
						while (itr != rinputs.end()) {
							double priml = lossPrimary(*itr);
							double seconl = lossSecondary(*itr);
							if (oracle == (*itr)->GetOracleResult()) {
								passing.push_back(*itr);
								ploss.push_back({ priml, seconl });
								_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ priml, seconl, _level });
							}
							itr++;
						}
					}
					break;
				case DDGoal::ReproduceResult:
				case DDGoal::None:
					{
						auto rinputs = sortlength();
						auto oracle = _origInput->GetOracleResult();
						auto itr = rinputs.begin();
						while (itr != rinputs.end()) {
							if (oracle == (*itr)->GetOracleResult()) {
								double priml = lossPrimary(*itr);
								double seconl = lossSecondary(*itr);
								passing.push_back(*itr);
								ploss.push_back({ 9, 0.0f });
								_results.insert_or_assign(*itr, std::tuple<double, double, int32_t>{ priml, seconl, _level });
							}
							itr++;
						}
					}
					break;
				}
			}
			break;
		case DDGoal::None:
			break;
		}
		loginfo("{} sort end", genCompData.batchident);

		// we have found all results, so free all inputs that we do not need anymore
		clearFlags();
		loginfo("{} cleared flags", genCompData.batchident);

		if (passing.size() == 0) {
			loginfo("{} no passing", genCompData.batchident);
			if (_level >= (int32_t)RangeIterator<size_t>(&_inputRanges, _sessiondata->_settings->dd.skipoptions, _skipRanges).GetLength()) {
				// we have already reached the maximum level
				// so we are effectively done with delta debugging.
				// Since our results are naturally integrated into the overall database we
				// don't need to do anything, but set the finished flag and we are done
				_finished = true;
				Finish();
				return;
			}

			if (_level * 2 >= (int32_t)RangeIterator<size_t>(&_inputRanges, _sessiondata->_settings->dd.skipoptions, _skipRanges).GetLength()) {
				std::cout << " ";
			}

			loginfo("{} prepare next level", genCompData.batchident);
			// no inputs were found that reproduce the original result
			// so we increase the level and try new sets of inputs
			_level = std::min(_level * 2, (int32_t)RangeIterator<size_t>(&_inputRanges, _sessiondata->_settings->dd.skipoptions, _skipRanges).GetLength());
			// run next generation if budget hasn't been exceeded
			if (_params->budget != 0 && _params->budget <= _totaltests) {
				_finished = true;
				Finish();
			} else
				ScoreProgressGenerateNextLevel_Async();
		} else {
			loginfo("{} passing: {}", genCompData.batchident, passing.size());
			// set new base _input
			if (passing[0]->GetSequenceLength() > _input->GetSequenceLength())
				logcritical("ooooopss");
			_input = passing[0];
			if (_bestScore.first <= _input->GetPrimaryScore())
				_bestScore = { _input->GetPrimaryScore(), _input->GetSecondaryScore() };
			_inputRanges = _input->FindIndividualPrimaryScoreRangesWithoutChanges();
			// adjust level
			//_level = std::max(_level - 1, 2);
			_level = 2;
			// we only deal in complements here
			//_level = std::max(_level - 1, 2);
			loginfo("{} prepare next level", genCompData.batchident);
			// run next generation if budget hasn't been exceeded
			if (_params->budget != 0 && _params->budget <= _totaltests) {
				_finished = true;
				Finish();
			} else {
				ScoreProgressGenerateNextLevel_Async();
				// free passing inputs we don't need anymore
				for (size_t i = 1; i < passing.size(); i++)
				{
					passing[i]->UnsetFlag(Form::FormFlags::DoNotFree);
					passing[i]->test->UnsetFlag(Form::FormFlags::DoNotFree);
					passing[i]->derive->UnsetFlag(Form::FormFlags::DoNotFree);
					// free memory to save reclaim time
					passing[i]->FreeMemory();
					if (passing[i]->GetGenerated() == false && passing[i]->test && passing[i]->test->IsValid() == false) {
						if (passing[i]->derive)
							passing[i]->derive->FreeMemory();
						if (passing[i]->test)
							passing[i]->test->FreeMemory();
					}
				}
			}
		}
	}

	void DeltaController::Finish()
	{
		// taint changed
		SetChanged();
		// set DD end time
		_DD_end = _sessiondata->data->GetRuntime();

		if (_params->mode != DDMode::Standard && _sessiondata->_settings->dd.runReproduceResultsAfterScoreApproxOnPositive && _params->GetGoal() == DDGoal::ReproduceResult) {
			// start normal delat debugging for test purposes
			if (_origInput == _input)
			{
				// couldn't reduce in ScoreProgress. Reset DD flag
				_input->UnsetFlag(Input::Flags::DeltaDebugged);
			}
			DeltaDebugging::DDParameters* params = nullptr;
			DeltaDebugging::ReproduceResult* par = new DeltaDebugging::ReproduceResult;
			par->secondarygoal = DDGoal::ReproduceResult;
			params = par;
			params->mode = DDMode::Standard;
			params->bypassTests = true;
			params->minimalSubsetSize = (int32_t)_sessiondata->_settings->dd.executeAboveLength + 1;
			auto control = _sessiondata->data->CreateForm<DeltaDebugging::DeltaController>();
			if (control->Start(params, _sessiondata, _input, nullptr)) {
				std::unique_lock<std::mutex> guard(_callbackLock);
				for (auto ptr : _callback) {
					if (ptr)
						control->AddCallback(ptr);
				}
				_callback.clear();
				_sessiondata->GetCurrentGeneration()->AddDDController(control);
			}
		}

		for (auto [ptr, pair] : _results) {
			if (ptr->HasFlag(Form::FormFlags::DoNotFree))
				ptr->UnsetFlag(Form::FormFlags::DoNotFree);
			if (ptr->derive &&  ptr->derive->HasFlag(Form::FormFlags::DoNotFree))
				ptr->derive->UnsetFlag(Form::FormFlags::DoNotFree);
			// unset flags and free
			//if (ptr->HasFlag(Form::FormFlags::DoNotFree))
			//	ptr->UnsetFlag(Form::FormFlags::DoNotFree);
			if (ptr->test && ptr->test->HasFlag(Form::FormFlags::DoNotFree))
				ptr->test->UnsetFlag(Form::FormFlags::DoNotFree);
			// free memory to save reclaim time
			ptr->FreeMemory();
			if (ptr->GetGenerated() == false && ptr->test && ptr->test->IsValid() == false) {
				if (ptr->derive)
					ptr->derive->FreeMemory();
				if (ptr->test)
					ptr->test->FreeMemory();
			}
		}
		// unset _input
		_input->derive->UnsetFlag(Form::FormFlags::DoNotFree);
		// unset and free originput
		_origInput->UnsetFlag(Form::FormFlags::DoNotFree);
		if (_origInput->derive)
			_origInput->derive->UnsetFlag(Form::FormFlags::DoNotFree);
		if (_origInput->test)
			_origInput->test->UnsetFlag(Form::FormFlags::DoNotFree);
		// free memory to save reclaim time
		_origInput->FreeMemory();
		if (_origInput->GetGenerated() == false && _origInput->test && _origInput->test->IsValid() == false) {
			if (_origInput->derive)
				_origInput->derive->FreeMemory();
			if (_origInput->test)
				_origInput->test->FreeMemory();
		}
		_activeInputs.clear();
		_completedTests.clear();
		{
			std::unique_lock<std::mutex> guard(_callbackLock);
			for (auto ptr : _callback) {
				if (ptr)
					_sessiondata->_controller->AddTask(ptr);
			}
			_callback.clear();
		}
	}

	bool DeltaController::AddCallback(std::shared_ptr<Functions::BaseFunction> callback)
	{
		// taint changed
		SetChanged();
		std::unique_lock<std::mutex> guard(_callbackLock);
		if (_finished == false) {
			_callback.push_back(callback);
			return true;
		} else
			return false;
	}

	bool DeltaController::HasCallback(uint64_t type)
	{
		std::unique_lock<std::mutex> guard(_callbackLock);
		if (_finished == false){
			for (auto call : _callback)
			{
				if (call && call->GetType() == type)
					return true;
			}
		} else
			return false;
		return false;
	}

	std::chrono::nanoseconds DeltaController::GetStartTime()
	{
		return _DD_begin;
	}
	std::chrono::nanoseconds DeltaController::GetEndTime()
	{
		if (_finished)
			return _DD_end;
		else
			return std::chrono::nanoseconds(0);
	}
	std::chrono::nanoseconds DeltaController::GetRunTime()
	{
		if (_finished)
			return _DD_end - _DD_begin;
		else
			return _sessiondata->data->GetRuntime() - _DD_begin;
	}

	size_t DeltaController::GetStaticSize(int32_t version)
	{
		static size_t size0x1 = 4      // version
		                        + 4    // tasks
		                        + 4    // remainingtasks
		                        + 4    // tests
		                        + 4    // remainingtests
		                        + 4    // totaltests
		                        + 1    // finished
		                        + 8    // origInput
		                        + 8    // _input
		                        + 4    // level
		                        + 1;   // has callback
		static size_t size0x2 = 4     // version
		                        + 4   // tasks
		                        + 4   // remainingtasks
		                        + 4   // tests
		                        + 4   // remainingtests
		                        + 4   // totaltests
		                        + 1   // finished
		                        + 8   // origInput
		                        + 8   // _input
		                        + 4   // level
		                        + 8   // number of callbacks
		                        + 16  // best score
		                        + 4   // _skippedTests
		                        + 4   // _prefixTests
		                        + 4   // _invalidTests
		                        + 4   // _activetests
		                        + 1;  // _stopbatch
		static size_t size0x3 = size0x2  // prior size
		                        + 8      // _skipRanges
		                        + 1      // GenerateComplementsData::active
		                        + 8      // GenerateComplementsData::Tasks::tasks
		                        + 1      // GenerateComplementsData::Tasks::sendEndEvent
		                        + 1      // GenerateComplementsData::Tasks::processedEndEvent
		                        + 8      // GenerateComplementsData::testqueue.size()
		                        + 8      // GenerateComplementsData::batchident
		                        + 8      // Time::_DD_begin
		                        + 8;     // Time::_DD_end
		static size_t size0x4 = size0x3  // prior size
		                        + 4;     // _approxTests


		switch (version)
		{
		case 0x1:
			return size0x1;
		case 0x2:
			return size0x2;
		case 0x3:
			return size0x3;
		case 0x4:
			return size0x4;
		default:
			return 0;
		}
	}

	size_t DeltaController::GetDynamicSize()
	{
		size_t sz = Form::GetDynamicSize()                                                                     // form stuff
		            + GetStaticSize(classversion)                                                              // static elements
		            + 8 /*size of results*/ + (8 + 8 + 8 + 4) * _results.size()                                // formids, lossPrimary, lossSecondary, and level in results
		            + 8 /*size of activeInputs*/ + 8 * _activeInputs.size()                                    // formids in activeInputs
		            + 8 /*size of completedTests*/ + 8 * _completedTests.size()                                //formids in completedTests
		            + 4 /*goal type*/ + 8                                                                      /*class size*/
		            + 8 /*size of waitingTests*/ + 8 * _waitingTests.size()                                    // formids in waitingTests
		            + 8 /*size of GenerateComplementsData::splits*/ + 8 * genCompData.splits.size()            // formids in GenerateComplementsData::splits
		            + 8 /*size of GenerateComplementsData::complements*/ + 8 * genCompData.complements.size()  // formids in GenerateComplementsData::complements
		            + 8 /*size of GenerateComplementsData::dinfo*/ + (4 + 4 + 1) * genCompData.dinfo.size();   // split info in GenerateComplementsData::dinfo

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
		case DDGoal::MaximizeBothScores:
			sz += sizeof(MaximizeBothScores);
			break;
		case DDGoal::None:
			sz += sizeof(DDParameters);
		default:
			break;
		} // size of param class

		std::unique_lock<std::mutex> guard(_callbackLock);
		for (auto ptr : _callback)
			if (ptr)
				sz += ptr->GetLength();

		for (auto ptr : genCompData.testqueue)
		{
			if (ptr)
				sz += ptr->GetLength();
		}
		return sz;
	}

	bool DeltaController::WriteData(std::ostream* buffer, size_t& offset, size_t length)
	{
		Buffer::Write(classversion, buffer, offset);
		Form::WriteData(buffer, offset, length);
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
		for (auto [ptr, tuple] : _results) {
			if (ptr) {
				Buffer::Write(ptr->GetFormID(), buffer, offset);
				Buffer::Write(std::get<0>(tuple), buffer, offset);
				Buffer::Write(std::get<1>(tuple), buffer, offset);
				Buffer::Write(std::get<2>(tuple), buffer, offset);
			} else {
				Buffer::Write((FormID)0, buffer, offset);
				Buffer::Write((double)0, buffer, offset);
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
			Buffer::Write(_params->budget, buffer, offset);
			Buffer::Write((int32_t)_params->mode, buffer, offset);
			Buffer::Write(((MaximizePrimaryScore*)_params)->acceptableLoss, buffer, offset);
			Buffer::Write(((MaximizePrimaryScore*)_params)->acceptableLossAbsolute, buffer, offset);
			break;
		case DDGoal::ReproduceResult:
			Buffer::WriteSize(sizeof(ReproduceResult), buffer, offset);
			//Buffer::Write((unsigned char*)_params, buffer, offset, sizeof(ReproduceResult));
			Buffer::Write(_params->minimalSubsetSize, buffer, offset);
			Buffer::Write(_params->bypassTests, buffer, offset);
			Buffer::Write(_params->budget, buffer, offset);
			Buffer::Write((int32_t)_params->mode, buffer, offset);
			Buffer::Write((int32_t)((ReproduceResult*)_params)->secondarygoal, buffer, offset);
			break;
		case DDGoal::MaximizeSecondaryScore:
			Buffer::WriteSize(sizeof(MaximizeSecondaryScore), buffer, offset);
			//Buffer::Write((unsigned char*)_params, buffer, offset, sizeof(MaximizeSecondaryScore));
			Buffer::Write(_params->minimalSubsetSize, buffer, offset);
			Buffer::Write(_params->bypassTests, buffer, offset);
			Buffer::Write(_params->budget, buffer, offset);
			Buffer::Write((int32_t)_params->mode, buffer, offset);
			Buffer::Write((float)((MaximizeSecondaryScore*)_params)->acceptableLossSecondary, buffer, offset);
			Buffer::Write((float)((MaximizeSecondaryScore*)_params)->acceptableLossSecondaryAbsolute, buffer, offset);
			break;
		case DDGoal::MaximizeBothScores:
			Buffer::WriteSize(sizeof(MaximizeBothScores), buffer, offset);
			//Buffer::Write((unsigned char*)_params, buffer, offset, sizeof(MaximizeSecondaryScore));
			Buffer::Write(_params->minimalSubsetSize, buffer, offset);
			Buffer::Write(_params->bypassTests, buffer, offset);
			Buffer::Write(_params->budget, buffer, offset);
			Buffer::Write((int32_t)_params->mode, buffer, offset);
			Buffer::Write((float)((MaximizeBothScores*)_params)->acceptableLossPrimary, buffer, offset);
			Buffer::Write((float)((MaximizeBothScores*)_params)->acceptableLossPrimaryAbsolute, buffer, offset);
			Buffer::Write((float)((MaximizeBothScores*)_params)->acceptableLossSecondary, buffer, offset);
			Buffer::Write((float)((MaximizeBothScores*)_params)->acceptableLossSecondaryAbsolute, buffer, offset);
			break;
		case DDGoal::None:
			Buffer::WriteSize(sizeof(DDParameters), buffer, offset);
			Buffer::Write(_params->minimalSubsetSize, buffer, offset);
			Buffer::Write(_params->bypassTests, buffer, offset);
			Buffer::Write(_params->budget, buffer, offset);
			Buffer::Write((int32_t)_params->mode, buffer, offset);
			//Buffer::Write((unsigned char*)_params, buffer, offset, sizeof(DDParameters));
			break;
		}
		// _callback
		std::unique_lock<std::mutex> guard(_callbackLock);
		size_t num = 0;
		for (auto ptr : _callback)
			if (ptr)
				num++;
		Buffer::WriteSize(num, buffer, offset);
		for (auto ptr : _callback)
		{
			if (ptr) {
				ptr->WriteData(buffer, offset);
			}
		}
		Buffer::Write(_bestScore.first, buffer, offset);
		Buffer::Write(_bestScore.second, buffer, offset);
		Buffer::Write(_skippedTests, buffer, offset);
		Buffer::Write(_prefixTests, buffer, offset);
		Buffer::Write(_invalidTests, buffer, offset);
		Buffer::Write(_activetests, buffer, offset);
		Buffer::Write(_stopbatch, buffer, offset);
		// _waitingTests
		Buffer::WriteSize(_waitingTests.size(), buffer, offset);
		for (auto ptr : _waitingTests)
			Buffer::Write(ptr->GetFormID(), buffer, offset);

		// VERSION 0x3
		Buffer::WriteSize(_skipRanges, buffer, offset);
		Buffer::Write(genCompData.active.load(), buffer, offset);
		if (auto tasks = genCompData.tasks.load(); tasks) {
			Buffer::Write(tasks->tasks.load(), buffer, offset);
			Buffer::Write(tasks->sendEndEvent, buffer, offset);
			Buffer::Write(tasks->processedEndEvent, buffer, offset);
		} else {
			Buffer::Write((int64_t)0, buffer, offset);
			Buffer::Write(false, buffer, offset);
			Buffer::Write(false, buffer, offset);
		}
		Buffer::WriteSize(genCompData.splits.size(), buffer, offset);
		for (size_t i = 0; i < genCompData.splits.size(); i++)
		{
			if (genCompData.splits[i])
				Buffer::Write(genCompData.splits[i]->GetFormID(), buffer, offset);
			else
				Buffer::Write((uint64_t)0, buffer, offset);
		}
		Buffer::WriteSize(genCompData.complements.size(), buffer, offset);
		for (size_t i = 0; i < genCompData.complements.size(); i++)
		{
			if (genCompData.complements[i])
				Buffer::Write(genCompData.complements[i]->GetFormID(), buffer, offset);
			else
				Buffer::Write((uint64_t)0, buffer, offset);
		}
		Buffer::WriteSize(genCompData.dinfo.size(), buffer, offset);
		for (size_t i = 0; i < genCompData.dinfo.size(); i++)
		{
			Buffer::Write(genCompData.dinfo[i].complement, buffer, offset);
			Buffer::Write(genCompData.dinfo[i].positionbegin, buffer, offset);
			Buffer::Write(genCompData.dinfo[i].length, buffer, offset);
		}
		Buffer::WriteSize(genCompData.testqueue.size(), buffer, offset);
		for (auto ptr : genCompData.testqueue)
			if (ptr)
				ptr->WriteData(buffer, offset);
		Buffer::Write(genCompData.batchident, buffer, offset);
		// time stuff
		Buffer::Write(_DD_begin, buffer, offset);
		Buffer::Write(_DD_end, buffer, offset);
		// VERSION 0x4
		Buffer::Write(_approxTests, buffer, offset);
		return true;
	}

	bool DeltaController::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
	{
		int32_t version = Buffer::ReadInt32(buffer, offset);
		if (_loadData)
			delete _loadData;
		_loadData = new LoadData();
		_loadData->version = version;
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
				_loadData->origInput = Buffer::ReadUInt64(buffer, offset);
				_loadData->input = Buffer::ReadUInt64(buffer, offset);
				_level = Buffer::ReadInt32(buffer, offset);
				// results
				size_t reslen = Buffer::ReadSize(buffer, offset);
				for (size_t i = 0; i < reslen; i++) {
					_loadData->res.push_back(Buffer::ReadUInt64(buffer, offset));
					_loadData->resloss.push_back(Buffer::ReadDouble(buffer, offset));
					_loadData->reslevel.push_back(Buffer::ReadInt32(buffer, offset));
				}
				// activeInputs
				size_t actIlen = Buffer::ReadSize(buffer, offset);
				for (size_t i = 0; i < actIlen; i++)
					_loadData->actI.push_back(Buffer::ReadUInt64(buffer, offset));
				// resolver
				// completedTests
				size_t cmpllen = Buffer::ReadSize(buffer, offset);
				for (size_t i = 0; i < cmpllen; i++)
					_loadData->complInp.push_back(Buffer::ReadUInt64(buffer, offset));
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
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					break;
				case DDGoal::ReproduceResult:
					_params = new ReproduceResult;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((ReproduceResult*)_params)->secondarygoal = (DDGoal)Buffer::ReadInt32(buffer, offset);
					break;
				case DDGoal::MaximizePrimaryScore:
					_params = new MaximizePrimaryScore;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((MaximizePrimaryScore*)_params)->acceptableLoss = Buffer::ReadFloat(buffer, offset);
					((MaximizePrimaryScore*)_params)->acceptableLossAbsolute = Buffer::ReadFloat(buffer, offset);
					break;
				case DDGoal::MaximizeSecondaryScore:
					_params = new MaximizeSecondaryScore;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					static_cast<void>(Buffer::ReadFloat(buffer, offset));
					((MaximizeSecondaryScore*)_params)->acceptableLossSecondary = Buffer::ReadFloat(buffer, offset);
					((MaximizeSecondaryScore*)_params)->acceptableLossSecondaryAbsolute = Buffer::ReadFloat(buffer, offset);
					break;
				case DDGoal::MaximizeBothScores:
					// shouldn't occur since this wasn't available back then
					_params = new MaximizeBothScores;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((MaximizeBothScores*)_params)->acceptableLossPrimary = Buffer::ReadFloat(buffer, offset);
					((MaximizeBothScores*)_params)->acceptableLossPrimaryAbsolute = Buffer::ReadFloat(buffer, offset);
					((MaximizeBothScores*)_params)->acceptableLossSecondary = Buffer::ReadFloat(buffer, offset);
					((MaximizeBothScores*)_params)->acceptableLossSecondaryAbsolute = Buffer::ReadFloat(buffer, offset);
				}
				//memcpy((void*)_params, dat, sz);
				//delete dat;
				// _callback
				bool hascall = Buffer::ReadBool(buffer, offset);
				if (hascall)
					_callback.push_back(Functions::BaseFunction::Create(buffer, offset, length, resolver));
				return true;
			}
			break;
		case 0x2:
		case 0x3:
		case 0x4:
			{
				Form::ReadData(buffer, offset, length, resolver);
				_tasks = Buffer::ReadInt32(buffer, offset);
				_remainingtasks = Buffer::ReadInt32(buffer, offset);
				_tests = Buffer::ReadInt32(buffer, offset);
				_remainingtests = Buffer::ReadInt32(buffer, offset);
				_totaltests = Buffer::ReadInt32(buffer, offset);
				_finished = Buffer::ReadBool(buffer, offset);
				_loadData->origInput = Buffer::ReadUInt64(buffer, offset);
				_loadData->input = Buffer::ReadUInt64(buffer, offset);
				_level = Buffer::ReadInt32(buffer, offset);
				// results
				size_t reslen = Buffer::ReadSize(buffer, offset);
				for (size_t i = 0; i < reslen; i++) {
					_loadData->res.push_back(Buffer::ReadUInt64(buffer, offset));
					_loadData->reslossprim.push_back(Buffer::ReadDouble(buffer, offset));
					_loadData->reslosssecon.push_back(Buffer::ReadDouble(buffer, offset));
					_loadData->reslevel.push_back(Buffer::ReadInt32(buffer, offset));
				}
				// activeInputs
				size_t actIlen = Buffer::ReadSize(buffer, offset);
				for (size_t i = 0; i < actIlen; i++)
					_loadData->actI.push_back(Buffer::ReadUInt64(buffer, offset));
				// completedTests
				size_t cmpllen = Buffer::ReadSize(buffer, offset);
				for (size_t i = 0; i < cmpllen; i++)
					_loadData->complInp.push_back(Buffer::ReadUInt64(buffer, offset));
				// params
				DDGoal goal = (DDGoal)Buffer::ReadInt32(buffer, offset);
				size_t sz = Buffer::ReadSize(buffer, offset);
				//unsigned char* dat = Buffer::ReadBuffer(buffer, offset, sz);
				switch (goal) {
				case DDGoal::None:
					_params = new DDParameters;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					break;
				case DDGoal::ReproduceResult:
					_params = new ReproduceResult;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((ReproduceResult*)_params)->secondarygoal = (DDGoal)Buffer::ReadInt32(buffer, offset);
					break;
				case DDGoal::MaximizePrimaryScore:
					_params = new MaximizePrimaryScore;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((MaximizePrimaryScore*)_params)->acceptableLoss = Buffer::ReadFloat(buffer, offset);
					((MaximizePrimaryScore*)_params)->acceptableLossAbsolute = Buffer::ReadFloat(buffer, offset);
					break;
				case DDGoal::MaximizeSecondaryScore:
					_params = new MaximizeSecondaryScore;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((MaximizeSecondaryScore*)_params)->acceptableLossSecondary = Buffer::ReadFloat(buffer, offset);
					((MaximizeSecondaryScore*)_params)->acceptableLossSecondaryAbsolute = Buffer::ReadFloat(buffer, offset);
					break;
				case DDGoal::MaximizeBothScores:
					_params = new MaximizeBothScores;
					_params->minimalSubsetSize = Buffer::ReadInt32(buffer, offset);
					_params->bypassTests = Buffer::ReadBool(buffer, offset);
					_params->budget = Buffer::ReadInt32(buffer, offset);
					_params->mode = (DDMode)Buffer::ReadInt32(buffer, offset);
					((MaximizeBothScores*)_params)->acceptableLossPrimary = Buffer::ReadFloat(buffer, offset);
					((MaximizeBothScores*)_params)->acceptableLossPrimaryAbsolute = Buffer::ReadFloat(buffer, offset);
					((MaximizeBothScores*)_params)->acceptableLossSecondary = Buffer::ReadFloat(buffer, offset);
					((MaximizeBothScores*)_params)->acceptableLossSecondaryAbsolute = Buffer::ReadFloat(buffer, offset);
					break;
				}
				//memcpy((void*)_params, dat, sz);
				//delete dat;
				// _callback
				_callback.clear();
				size_t hascall = Buffer::ReadSize(buffer, offset);
				for (size_t i = 0; i < hascall; i++)
				{
					_callback.push_back(Functions::BaseFunction::Create(buffer, offset, length, resolver));
				}
				
				double prim = Buffer::ReadDouble(buffer, offset);
				_bestScore = { prim,
					Buffer::ReadDouble(buffer, offset) };

				_skippedTests = Buffer::ReadInt32(buffer, offset);
				_prefixTests = Buffer::ReadInt32(buffer, offset);
				_invalidTests = Buffer::ReadInt32(buffer, offset);
				_activetests = Buffer::ReadInt32(buffer, offset);
				_stopbatch = Buffer::ReadBool(buffer, offset);
				// waitingTests
				size_t waitlen = Buffer::ReadSize(buffer, offset);
				for (size_t i = 0; i < waitlen; i++)
					_loadData->waitInp.push_back(Buffer::ReadUInt64(buffer, offset));

				if (version >= 0x3)
				{
					// VERSION 0x3
					_skipRanges = Buffer::ReadSize(buffer, offset);
					genCompData.active = Buffer::ReadBool(buffer, offset);
					genCompData.tasks = std::make_shared<Tasks>();
					auto tasks = genCompData.tasks.load();
					tasks->tasks = Buffer::ReadInt64(buffer, offset);
					tasks->sendEndEvent = Buffer::ReadBool(buffer, offset);
					tasks->processedEndEvent = Buffer::ReadBool(buffer, offset);
					size_t splitsize = Buffer::ReadSize(buffer, offset);
					for (size_t i = 0; i < splitsize; i++)
						_loadData->splitids.push_back(Buffer::ReadUInt64(buffer, offset));
					size_t complementsize = Buffer::ReadSize(buffer, offset);
					for (size_t i = 0; i < complementsize; i++)
						_loadData->complementids.push_back(Buffer::ReadUInt64(buffer, offset));
					size_t dinfosize = Buffer::ReadSize(buffer, offset);
					for (size_t i = 0; i < dinfosize; i++)
					{
						DeltaInformation dinfo;
						dinfo.complement = Buffer::ReadBool(buffer, offset);
						dinfo.positionbegin = Buffer::ReadInt32(buffer, offset);
						dinfo.length = Buffer::ReadInt32(buffer, offset);
						genCompData.dinfo.push_back(dinfo);
					}
					size_t testqueuesize = Buffer::ReadSize(buffer, offset);
					for (size_t i = 0; i < testqueuesize; i++) {
						genCompData.testqueue.push_back(Functions::BaseFunction::Create(buffer, offset, length, resolver));
					}
					genCompData.batchident = Buffer::ReadUInt64(buffer, offset);
					// time stuff
					_DD_begin = Buffer::ReadNanoSeconds(buffer, offset);
					_DD_end = Buffer::ReadNanoSeconds(buffer, offset);
				}
				if (version >= 0x4) {
					// VERSION 0x4
					_approxTests = Buffer::ReadInt32(buffer, offset);
				}
				return true;
			}
			break;
		default:
			return false;
		}
	}

	void DeltaController::InitializeEarly(LoadResolver* resolver)
	{
		if (_loadData) {
			switch (_loadData->version) {
			case 0x1:
				resolver->current = "DeltaDebugging";
				_sessiondata = resolver->ResolveFormID<SessionData>(Data::StaticFormIDs::SessionData);
				_self = resolver->ResolveFormID<DeltaController>(this->GetFormID());
				_input = resolver->ResolveFormID<Input>(_loadData->input);
				_bestScore = { _input->GetPrimaryScore(),
					_input->GetSecondaryScore() };
				if (_input->HasFlag(Form::FormFlags::DoNotFree) == false) {
					_input->SetFlag(Form::FormFlags::DoNotFree);
				}
				_origInput = resolver->ResolveFormID<Input>(_loadData->origInput);
				if (_origInput->HasFlag(Form::FormFlags::DoNotFree) == false) {
					_origInput->SetFlag(Form::FormFlags::DoNotFree);
				}
				// results
				for (size_t i = 0; i < _loadData->res.size(); i++) {
					auto ptr = resolver->ResolveFormID<Input>(_loadData->res[i]);
					if (ptr) {
						if (_params->GetGoal() == DDGoal::MaximizeSecondaryScore)
							_results.insert_or_assign(ptr, std::tuple<double, double, int32_t>{ 0.0f, _loadData->resloss[i], _loadData->reslevel[i] });
						else
							_results.insert_or_assign(ptr, std::tuple<double, double, int32_t>{ _loadData->resloss[i], 0.0f, _loadData->reslevel[i] });
						if (ptr->HasFlag(Form::FormFlags::DoNotFree) == false) {
							ptr->SetFlag(Form::FormFlags::DoNotFree);
						}
					}
				}
				// activeInputs
				for (size_t i = 0; i < _loadData->actI.size(); i++) {
					auto ptr = resolver->ResolveFormID<Input>(_loadData->actI[i]);
					if (ptr) {
						{
							Utility::SpinLock lock(_activeInputsFlag);
							_activeInputs.insert(ptr);
						}
						if (ptr->HasFlag(Form::FormFlags::DoNotFree) == false)
							ptr->SetFlag(Form::FormFlags::DoNotFree);
					}
				}
				// completedTests
				for (size_t i = 0; i < _loadData->complInp.size(); i++) {
					auto ptr = resolver->ResolveFormID<Input>(_loadData->complInp[i]);
					if (ptr)
						_completedTests.insert(ptr);
				}
				break;
			case 0x2:
			case 0x3:
			case 0x4:
				resolver->current = "DeltaDebugging 1";
				_sessiondata = resolver->ResolveFormID<SessionData>(Data::StaticFormIDs::SessionData);
				_self = resolver->ResolveFormID<DeltaController>(this->GetFormID());
				_input = resolver->ResolveFormID<Input>(_loadData->input);
				if (_input && _input->HasFlag(Form::FormFlags::DoNotFree) == false) {
					_input->SetFlag(Form::FormFlags::DoNotFree);
				}
				_origInput = resolver->ResolveFormID<Input>(_loadData->origInput);
				if (_origInput && _origInput->HasFlag(Form::FormFlags::DoNotFree) == false) {
					_origInput->SetFlag(Form::FormFlags::DoNotFree);
				}
				// results
				for (size_t i = 0; i < _loadData->res.size(); i++) {
					auto ptr = resolver->ResolveFormID<Input>(_loadData->res[i]);
					if (ptr) {
						_results.insert_or_assign(ptr, std::tuple<double, double, int32_t>{ _loadData->reslossprim[i], _loadData->reslosssecon[i], _loadData->reslevel[i] });
						if (ptr->HasFlag(Form::FormFlags::DoNotFree) == false) {
							ptr->SetFlag(Form::FormFlags::DoNotFree);
						}
					}
				}
				// activeInputs
				for (size_t i = 0; i < _loadData->actI.size(); i++) {
					auto ptr = resolver->ResolveFormID<Input>(_loadData->actI[i]);
					if (ptr) {
						{
							Utility::SpinLock lock(_activeInputsFlag);
							_activeInputs.insert(ptr);
						}
						if (ptr->HasFlag(Form::FormFlags::DoNotFree) == false)
							ptr->SetFlag(Form::FormFlags::DoNotFree);
					}
				}
				// completedTests
				for (size_t i = 0; i < _loadData->complInp.size(); i++) {
					auto ptr = resolver->ResolveFormID<Input>(_loadData->complInp[i]);
					if (ptr)
						_completedTests.insert(ptr);
				}
				// waitingTests
				for (size_t i = 0; i < _loadData->waitInp.size(); i++) {
					auto ptr = resolver->ResolveFormID<Input>(_loadData->waitInp[i]);
					if (ptr)
						_completedTests.insert(ptr);
				}
				// VERSION 0x3
				for (size_t i = 0; i < _loadData->splitids.size(); i++) {
					genCompData.splits.push_back(resolver->ResolveFormID<Input>(_loadData->splitids[i]));
				}
				for (size_t i = 0; i < _loadData->complementids.size(); i++)
					genCompData.complements.push_back(resolver->ResolveFormID<Input>(_loadData->complementids[i]));
				break;
			}
		}
	}

	void DeltaController::InitializeLate(LoadResolver* resolver)
	{
		if (_loadData) {
			switch (_loadData->version) {
			case 0x1:
				break;
			case 0x2:
			case 0x3:
			case 0x4:
				// create _inputRanges, _input, and _origInput
				if (_finished == false) {
					resolver->current = "DeltaDebugging Late 1";
					if (_input && _params->mode == DeltaDebugging::DDMode::ScoreProgress) {
						_inputRanges = _input->FindIndividualPrimaryScoreRangesWithoutChanges();
					}
					resolver->AddRegeneration(_origInput->GetFormID());
					/*if (_origInput->GetGenerated() == false) {
						// we are trying to add an _input that hasn't been generated or regenerated
						// try the generate it and if it succeeds add the test
						SessionFunctions::GenerateInput(_origInput, _sessiondata);
						if (_origInput->GetGenerated() == false)
							logcritical("DeltaDebugging original input could not be reconstructed");
					}*/
					resolver->AddRegeneration(_input->GetFormID());
					/*if (_input->GetGenerated() == false || _input->GetSequenceLength() == 0) {
						// we are trying to add an _input that hasn't been generated or regenerated
						// try the generate it and if it succeeds add the test
						_input->SetGenerated(false);
						SessionFunctions::GenerateInput(_input, _sessiondata);
						if (_input->GetGenerated() == false)
							logcritical("DeltaDebugging input could not be reconstructed");
					}*/
				}
				break;
			}


			delete _loadData;
			_loadData = nullptr;
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
			Functions::RegisterFactory(Functions::DDGenerateComplementCallback::GetTypeStatic(), Functions::DDGenerateComplementCallback::Create);
			Functions::RegisterFactory(Functions::DDGenerateCheckSplit::GetTypeStatic(), Functions::DDGenerateCheckSplit::Create);
		}
	}

	size_t DeltaController::MemorySize()
	{
		return sizeof(DeltaController) + _completedTests.size() * sizeof(std::shared_ptr<Input>) + _activeInputs.size() * sizeof(std::pair<std::shared_ptr<Input>, FormIDLess<Input>>) + sizeof(std::pair<size_t, size_t>) * _inputRanges.size() + sizeof(std::pair<std::shared_ptr<Input>, std::tuple<double, double, int32_t>>) * _results.size();
	}
}

template <class T>
RangeIterator<T>::RangeIterator(std::vector<std::pair<T, T>>* ranges, RangeSkipOptions skipfirst, size_t skipRanges)
{
	_ranges = ranges;
	_skipoptions = skipfirst;
	_length = GetLength();
	if (_ranges->size() >= skipRanges)
		_skipRanges = skipRanges;
	else
		_skipRanges = _ranges->size();

	// calc max range
	size_t result = 0;
	for (size_t i = _skipRanges; i < _ranges->size(); i++) {
		if (_skipoptions == RangeSkipOptions::SkipFirst && result < _ranges->at(i).second - 1)
			result = _ranges->at(i).second - 1;
		else if (_skipoptions == RangeSkipOptions::SkipLast && result < _ranges->at(i).second - 1)
			result = _ranges->at(i).second - 1;
		else if (_skipoptions == RangeSkipOptions::None && result < _ranges->at(i).second)
			result = _ranges->at(i).second;

	}
	_maxRange = result;
}

template <class T>
size_t RangeIterator<T>::GetLength()
{
	// don't count the first element, it is suspected to be unique
	T total = 0;
	for (size_t i = _skipRanges; i < _ranges->size(); i++) {
		if (_skipoptions == RangeSkipOptions::SkipFirst || _skipoptions == RangeSkipOptions::SkipLast)
			total += _ranges->at(i).second - 1;
		else
			total += _ranges->at(i).second;
	}
	return total;
}

template <class T>
std::vector<T> RangeIterator<T>::GetRange(size_t length)
{
	std::vector<size_t> result;
	while (length > 0 && _position < _length) {
		result.push_back(_ranges->at(_posX).first + _posY);
		_posY++;
		if (_posY >= _ranges->at(_posX).second) {
			_posX++;
			_posY = 0;
		}
		length--;
		_position++;
	}
	return result;
}

template <class T>
std::vector<std::pair<T, T>> RangeIterator<T>::GetRanges(size_t maxsize)
{
	if (maxsize <= 0)
		maxsize = 1;
	std::vector<std::pair<T, T>> result;
	for (size_t i = _skipRanges; i < _ranges->size(); i++) {
		if (_ranges->at(i).second < maxsize) {
			if (_skipoptions == RangeSkipOptions::SkipFirst)
				result.push_back({ _ranges->at(i).first + 1, _ranges->at(i).second - 1 });
			else if (_skipoptions == RangeSkipOptions::SkipLast)
				result.push_back({ _ranges->at(i).first, _ranges->at(i).second - 1 });
			else
				result.push_back({ _ranges->at(i).first, _ranges->at(i).second });
		} else {
			auto [begin, length] = _ranges->at(i);
			size_t position = 0;
			if (_skipoptions == RangeSkipOptions::SkipFirst)
				position = 1;
			else if (_skipoptions == RangeSkipOptions::SkipLast)
				length -= 1;
			while (position < length) {
				if (length - position > maxsize)
					result.push_back({ begin + position, maxsize });
				else
					result.push_back({ begin + position, length - position });

				position += maxsize;
			}
		}
	}
	return result;
}
template <class T>
std::vector<std::pair<T, T>> RangeIterator<T>::GetRangesAbove(size_t minsize)
{
	if (minsize <= 0)
		minsize = 1;
	std::vector<std::pair<T, T>> result;
	for (size_t i = _skipRanges; i < _ranges->size(); i++) {
		if (_ranges->at(i).second < minsize){
		/* if (_ranges->at(i).second > minsize) {
			if (_skipoptions == RangeSkipOptions::SkipFirst)
				result.push_back({ _ranges->at(i).first + 1, _ranges->at(i).second - 1 });
			else if (_skipoptions == RangeSkipOptions::SkipLast)
				result.push_back({ _ranges->at(i).first, _ranges->at(i).second - 1 });
			else
				result.push_back({ _ranges->at(i).first, _ranges->at(i).second });*/
		} else {
			auto [begin, length] = _ranges->at(i);
			size_t position = 0;
			if (_skipoptions == RangeSkipOptions::SkipFirst)
				position = 1;
			else if (_skipoptions == RangeSkipOptions::SkipLast)
				length -= 1;
			while (position < length) {
				if (length - position >= minsize)
					result.push_back({ begin + position, minsize });

				position += minsize;
			}
		}
	}
	return result;
}



template <class T>
RangeCalculator<T>::RangeCalculator(std::vector<std::pair<T, T>>* ranges, T orig_length)
{
	_ranges = ranges;
	_length = GetLength();
	_orig_length = orig_length;
}

template <class T>
size_t RangeCalculator<T>::GetLength()
{
	// don't count the first element, it is suspected to be unique
	T total = 0;
	for (size_t i = 0; i < _ranges->size(); i++) {
		total += _ranges->at(i).second;
	}
	return _orig_length - total;
}

template <class T>
bool RangeCalculator<T>::IsInRange(T pos)
{
	for (size_t i = 0; i < _ranges->size(); i++) {
		// if pos is before the range we check, its outside any ranges
		if (pos < _ranges->at(i).first)
			return false;
		// if pos is the beginning of a range or before begin+length its in the range
		if (pos >= _ranges->at(i).first && pos < _ranges->at(i).first + _ranges->at(i).second)
			return true;
	}
	return false;
}

template <class T>
T RangeCalculator<T>::Coord(T pos)
{
	// counter
	T c = 0;
	T coord = 0;
	if (c == pos)
		return coord;
	for (size_t i = 0; i < _ranges->size(); i++) {
		// while the counter is less or equal the last position in the next range to check
		while (c < _ranges->at(i).first + _ranges->at(i).second) {
			// if pos is before the range we check, its outside any ranges
			if (c < _ranges->at(i).first) {
				coord++;
				c++;
			}
			// if pos is the beginning of a range or before begin+length its in the range
			else if (c >= _ranges->at(i).first && c < _ranges->at(i).first + _ranges->at(i).second) {
				c++;
			}
			// pos is after the range -> skip to next range
			else
				break;
			// if we have reached the position, return coordinates
			if (c == pos)
				return coord;
		}
	}
	// we have arrived after the last range, so only iterate until we reach the orig_length
	while (c < _orig_length) {
		c++;
		coord++;
		// if we have reached the position, return coordinates
		if (c == pos)
			return coord;
	}
	return coord;
}

template <class T>
int32_t RangeCalculator<T>::GetMatchingRangeIdx(T pos, T count, bool& found, bool& addend)
{
	for (size_t i = 0; i < _ranges->size(); i++) {
		if (pos + count == _ranges->at(i).first)  // add at beginning
		{
			// may add at beginning of range
			found = true;
			addend = false;
			return (int32_t)i;
		} else if (pos == _ranges->at(i).first + _ranges->at(i).second)  // add at end
		{
			// may add at end of range
			found = true;
			addend = true;
			return (int32_t)i;
		} else if (pos + count < _ranges->at(i).first) {
			// cannot at at beginning of range, so create new range before the current index
			found = false;
			addend = false;
			return (int32_t)i - 1;
		}
	}
	// we are after the last range, so add as new range
	found = false;
	addend = false;
	return (int32_t)_ranges->size();
}

template <class T>
std::vector<std::pair<T, T>> RangeCalculator<T>::GetNewRangesWithout(T begin, T count)
{
	// return vector
	std::vector<std::pair<T, T>> segments;
	std::vector<std::pair<T, T>> segs;

	// positions to cut
	std::vector<T> tocut;
	for (T c = 0; c < _orig_length; c++) {
		if (IsInRange(c))
			// we cannot cut the current location
			void();
		if (!IsInRange(c)) {
			T coord = Coord(c);
			if (coord >= begin && coord < begin + count)
				tocut.push_back(c);
			else if (coord >= begin + count)
				break;  // all positions found
		}
	}

	for (auto pos : tocut) {
		bool found = false;
		for (size_t i = 0; i < segs.size(); i++) {
			if (segs[i].first + segs[i].second == pos) {
				segs[i] = { segs[i].first, segs[i].second + 1 };
				found = true;
			}
		}
		if (!found)
			segs.push_back({ pos, 1 });
	}

	for (size_t i = 0; i < _ranges->size(); i++) {
		segments.push_back(_ranges->at(i));
	}

	// add new segments to existing
	for (int32_t i = (int32_t)segs.size() - 1; i >= 0; i--) {
		bool found = false, addend = false;
		int32_t idx = GetMatchingRangeIdx(segs[i].first, segs[i].second, found, addend);
		if (found) {
			// add to existing range
			if (addend == false) {
				segments[idx] = { segs[i].first, segs[i].second + segments[idx].second };
			} else {
				segments[idx] = { segments[idx].first, segments[idx].second + segs[i].second };
			}
		} else {
			if (idx == _ranges->size()) {
				// append at end
				segments.push_back(segs[i]);
			} else if (idx == -1) {
				// insert at beginning
				segments.insert(segments.begin(), segs[i]);
			} else {
				// insert after idx
				segments.resize(segments.size() + 1);
				std::pair<T, T> tmp = segments[idx + 1];
				std::pair<T, T> tmp2{ 0, 0 };
				for (int32_t x = idx + 1; x < (int32_t)segments.size() - 1; x++) {
					tmp2 = tmp;
					tmp = segments[x + 1];
					segments[x + 1] = tmp2;
				}
				segments[idx + 1] = segs[i];
			}
		}
	}
	// merge segements
	for (int i = 0; i < (int32_t)segments.size() - 1; i++) {
		if (segments[i].first + segments[i].second == segments[i + 1].first) {
			segments[i] = { segments[i].first, segments[i].second + segments[i + 1].second };
			// iterate over all remaining segments and move them one closer, pop last
			for (int x = i + 1; x < (int32_t)segments.size() - 1; x++)
				segments[x] = segments[x + 1];
			segments.pop_back();
			i--;  // check the same segment again if there are any other merges to be performed
		}
	}

	return segments;
}
