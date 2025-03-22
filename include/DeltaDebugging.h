#pragma once

#include "Form.h"
#include "Function.h"
#include "Input.h"

#include <memory>
#include <vector>
#include <set>

class SessionData;

enum class RangeSkipOptions;

namespace DeltaDebugging
{
	class DeltaController;
}


namespace DeltaDebugging
{

	enum class DDMode
	{
		/// <summary>
		/// Standard mode is the standard implementation of DDmin as described in Andreas Zellers paper.
		/// It uses the standard binary division and complements to compute a result.
		/// Expected effectiveness: very low
		/// </summary>
		Standard = 0,
		/// <summary>
		/// Custom imlementation that attempts to remove parts of the input where no progress in terms of primary score
		/// is being made
		/// </summary>
		ScoreProgress = 1 << 0,
	};

	enum class DDGoal
	{
		None = 0x0,
		/// <summary>
		/// Attempts to reproduce the original output result (aka. standard)
		/// </summary>
		ReproduceResult = 1 << 0,
		/// <summary>
		/// Attempts to maximize the primary score of executed tests.
		/// </summary>
		MaximizePrimaryScore = 1 << 1,
		/// <summary>
		/// Attempts to maximize the secondary score of executed tests
		/// </summary>
		MaximizeSecondaryScore = 1 << 2,
		/// <summary>
		/// Attempts to maximize the primary score and the secondary score of executed tests
		/// </summary>
		MaximizeBothScores = 1 << 3,
	};

	struct DDParameters
	{
		virtual DDGoal GetGoal() { return DDGoal::None; }
		/// <summary>
		/// Minimal size of subsets that may be used for tests.
		/// This compensates for the total number of tests when a lot of input is given
		/// to the PUT in a short amount of time, with low expected gain on individual inputs
		/// </summary>
		int32_t minimalSubsetSize = 0;
		/// <summary>
		/// delta debugging tests bypass regular tests
		/// </summary>
		bool bypassTests = false;
		/// <summary>
		/// max number of tests to be run
		/// </summary>
		int32_t budget = 0;

		DDMode mode = DDMode::Standard;
	};

	struct ReproduceResult: public DDParameters
	{
		DDGoal GetGoal() override { return DDGoal::ReproduceResult; }
		// no really important parameters, as everything is standard
		// this is the variant best for winning strategies, as we want ot reproduce them
		// when minimizing

		/// <summary>
		/// secondary optimization goal
		/// if there are multiple viable subresults, the algorithm will try to
		/// further optimize the one adhering to this goal
		/// </summary>
		DDGoal secondarygoal = DDGoal::None;
	};

	struct MaximizePrimaryScore : public DDParameters
	{
		DDGoal GetGoal() override { return DDGoal::MaximizePrimaryScore; }
		/// <summary>
		/// Acceptable loss for total primary score for the test to be considered a valid result
		/// </summary>
		float acceptableLoss = 0.05f;
	};

	struct MaximizeSecondaryScore : public DDParameters
	{
		DDGoal GetGoal() override { return DDGoal::MaximizeSecondaryScore; }
		/// <summary>
		/// Acceptable loss for total secondary score for the test to be considered a valid result
		/// </summary>
		float acceptableLossSecondary = 0.05f;
	};

	struct MaximizeBothScores : public DDParameters
	{
		DDGoal GetGoal() override { return DDGoal::MaximizeBothScores; }
		/// <summary>
		/// Acceptable loss for total prmary score for the test to be considered a valid result
		/// </summary>
		float acceptableLossPrimary = 0.05f;
		/// <summary>
		/// Acceptable loss for total secondary score for the test to be considered a valid result
		/// </summary>
		float acceptableLossSecondary = 0.05f;
	};

	struct DeltaInformation
	{
		int32_t positionbegin = 0;
		int32_t length = 0;
		bool complement = false;
	};

	struct Tasks
	{
		std::atomic<int64_t> tasks = 0;
	};

	struct GenerateComplementsData
	{

		std::atomic<bool> active = false;
		std::shared_ptr<Tasks> tasks;
		std::vector<std::shared_ptr<Input>> splits;
		std::vector<std::shared_ptr<Input>> complements;
		std::vector<DeltaInformation> dinfo;

		std::mutex testqueuelock;
		std::list<std::shared_ptr<Functions::BaseFunction>> testqueue;

		uint64_t batchident = 0;

		void Reset()
		{
			active = false;
			tasks = 0;
			complements.clear();
			dinfo.clear();
			splits.clear();
			testqueue.clear();
		}
	};

	class DeltaController : public Form
	{
	public:
		DeltaController() {}
		~DeltaController();

		/// <summary>
		/// starts delta debugging for an input on a specific taskcontroller
		/// </summary>
		bool Start(DDParameters* params, std::shared_ptr<SessionData> sessiondata, std::shared_ptr<Input> input, std::shared_ptr<Functions::BaseFunction> callback);

		void CallbackTest(std::shared_ptr<Input> input, uint64_t batchident, std::shared_ptr<Tasks> tasks);
		void CallbackExplicitEvaluate();

		DDGoal GetGoal()
		{
			if (_params)
				return _params->GetGoal();
			else
				return DDGoal::None;
		}

		DDMode GetMode()
		{
			if (_params)
				return _params->mode;
			else
				return DDMode::Standard;
		}

		int32_t GetTests() { return _tests; }
		int32_t GetTestsRemaining() { 
			if (genCompData.tasks)
				return (int32_t)genCompData.tasks->tasks.load() + (int32_t)genCompData.testqueue.size();
			else
				return (int32_t)genCompData.testqueue.size();
		}
		int32_t GetTestsTotal() { return _totaltests; }
		int32_t GetLevel() { return _level; }
		int32_t GetSkippedTests() { return _skippedTests; }
		int32_t GetPrefixTests() { return _prefixTests; }
		int32_t GetInvalidTests() { return _invalidTests; }
		bool Finished() { return _finished; }
		std::unordered_map<std::shared_ptr<Input>, std::tuple<double, double, int32_t>>* GetResults() { return &_results; }
		std::shared_ptr<Input> GetInput() { return _input; }
		std::shared_ptr<Input> GetOriginalInput() { return _origInput; }
		std::set<std::shared_ptr<Input>, FormIDLess<Input>>* GetActiveInputs() { return &_activeInputs; }

		/// <summary>
		/// Adds a callback to the controller [if the controller has already finished the callback is not called
		/// </summary>
		/// <param name="callback"></param>
		bool AddCallback(std::shared_ptr<Functions::BaseFunction> callback);
		bool HasCallback(uint64_t type);

		uint64_t GetBatchIdent()
		{
			return genCompData.batchident;
		}

		std::shared_ptr<Tasks> GetBatchTasks()
		{
			return genCompData.tasks;
		}
		
		/// <summary>
		/// returns the total size of the fields with static size
		/// </summary>
		/// <returns></returns>
		size_t GetStaticSize(int32_t version = 0) override;
		/// <summary>
		/// returns the total size of all fields of this instance
		/// </summary>
		/// <returns></returns>
		size_t GetDynamicSize() override;
		/// <summary>
		/// saves all relevant information of this instance to the given buffer
		/// </summary>
		/// <param name="buffer"></param>
		/// <returns></returns>
		bool WriteData(std::ostream* buffer, size_t& offset) override;
		/// <summary>
		/// reads all relevant information of this instance from the buffer
		/// </summary>
		/// <param name="buffer"></param>
		/// <param name="length"></param>
		bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		/// <summary>
		/// Deletes all relevant for fields
		/// </summary>
		void Delete(Data* data) override;
		static void RegisterFactories();
		static int32_t GetTypeStatic() { return FormType::DeltaController; }
		int32_t GetType() override { return FormType::DeltaController; }
		size_t MemorySize() override;

		/// <summary>
		/// Clears all internals
		/// </summary>
		void Clear();

	private:
		const int32_t classversion = 0x3;
		static inline bool _registeredFactories = false;

		/// <summary>
		/// generates a single complement
		/// </summary>
		/// <param name="begin"></param>
		/// <param name="end"></param>
		/// <param name="approxthreshold"></param>
		/// <returns></returns>
		std::shared_ptr<Input> GetComplement(int32_t begin, int32_t end, double approxthreshold, std::shared_ptr<Input>& parent);
		bool CheckInput(std::shared_ptr<Input> parent, std::shared_ptr<Input> inp, double approxthreshold);
		/// <summary>
		/// Add tests to the sessions executionhanler
		/// </summary>
		/// <param name="inputs"></param>
		void AddTests(std::vector<std::shared_ptr<Input>>& inputs);

		/// <summary>
		/// Starts a test
		/// </summary>
		/// <param name="input"></param>
		bool DoTest(std::shared_ptr<Input>& input, uint64_t batchident, std::shared_ptr<Tasks> tasks);

		/// <summary>
		/// Generates [number] subset from [::_input]
		/// </summary>
		/// <param name="number"></param>
		/// <returns></returns>
		std::vector<std::shared_ptr<Input>> GenerateSplits(int32_t number, std::vector<DeltaInformation>& splitinfo);
		std::vector<std::shared_ptr<Input>> GenerateComplements(std::vector<DeltaInformation>& splitinfo);


		// both methods
		void GenerateSplits_Async(int32_t number);
		void GenerateComplements_Async(std::vector<DeltaInformation>& splitinfo);

		GenerateComplementsData genCompData;

	public:
		void GenerateSplits_Async_Callback(std::shared_ptr<Input>& input, double approxthreshold,uint64_t batchident, std::shared_ptr<Tasks> tasks);
		void GenerateComplements_Async_Callback(int32_t begin, int32_t length, double approx, uint64_t batchident, std::shared_ptr<Input>& parent, std::shared_ptr<Tasks> tasks);

	private:

		/// <summary>
		/// generates the first level of tests for standard mode
		/// </summary>
		void StandardGenerateFirstLevel();
		/// <summary>
		/// generates the next level of tests for standard mode
		/// </summary>
		void StandardGenerateNextLevel();
		void StandardGenerateNextLevel_Async();
		void StandardGenerateNextLevel_Inter();
		void StandardGenerateNextLevel_End();
		/// <summary>
		/// evaluates a completed level for standard mode
		/// </summary>
		void StandardEvaluateLevel();
		/// <summary>
		/// evaluates a single input and returns whether it satisfies the DDs requirements
		/// </summary>
		/// <param name="input"></param>
		/// <returns></returns>
		bool StandardEvaluateInput(std::shared_ptr<Input> input);

		/// <summary>
		/// Generates subsets from [::_input] with maximum removed length being size
		/// </summary>
		/// <param name="splitinfo"></param>
		/// <returns></returns>
		std::vector<std::shared_ptr<Input>> ScoreProgressGenerateComplements(int32_t size);
		void ScoreProgressGenerateComplements_Async(int32_t size);

		void ScoreProgressGenerateFirstLevel();

		void ScoreProgressGenerateNextLevel();
		void ScoreProgressGenerateNextLevel_Async();

		void ScoreProgressGenerateNextLevel_End();

		void ScoreProgressEvaluateLevel();

		/// <summary>
		/// evaluates a single input and returns whether it satisfies the DDs requirements
		/// </summary>
		/// <param name="input"></param>
		/// <returns></returns>
		bool ScoreProgressEvaluateInput(std::shared_ptr<Input> input);

		/// <summary>
		/// generic cleanup operations
		/// </summary>
		void Finish();

		/// <summary>
		/// number of tasks that were started
		/// </summary>
		std::atomic<int32_t> _tasks = 0;
		/// <summary>
		/// number of tasks that need to be completed
		/// </summary>
		std::atomic<int32_t> _remainingtasks = 0;
		/// <summary>
		/// number of tests executed
		/// </summary>
		std::atomic<int32_t> _tests = 0;
		/// <summary>
		/// number of tests that need to be completed
		/// </summary>
		std::atomic<int32_t> _remainingtests = 0;
		/// <summary>
		/// total number of tests executed
		/// </summary>
		std::atomic<int32_t> _totaltests = 0;

		/// <summary>
		/// number of tests skipped due to batch processing
		/// </summary>
		int32_t _skippedTests = 0;
		/// <summary>
		/// number of tests skipped due to prefix
		/// </summary>
		int32_t _prefixTests = 0;
		/// <summary>
		/// number of generated inputs that are not valid under the grammar
		/// </summary>
		int32_t _invalidTests = 0;

		/// <summary>
		/// whether we are completely done with delta debugging
		/// </summary>
		bool _finished = false;

		/// <summary>
		/// map of results, corresponding loss if available, and the level
		/// </summary>
		std::unordered_map<std::shared_ptr<Input>, std::tuple<double, double, int32_t>> _results;

		std::shared_ptr<Input> _origInput;

		std::shared_ptr<Input> _input;

		std::vector<std::pair<size_t, size_t>> _inputRanges;

		size_t _skipRanges = 0;

		std::shared_ptr<SessionData> _sessiondata;

		std::shared_ptr<DeltaController> _self;

		std::set<std::shared_ptr<Input>, FormIDLess<Input>> _activeInputs;

		std::set<std::shared_ptr<Input>> _completedTests;

		std::mutex _completedTestsLock;

		std::vector<std::shared_ptr<Functions::BaseFunction>> _callback;
		std::mutex _callbackLock;

		std::pair<double, double> _bestScore = { 0.0f, 0.0f };


		/// <summary>
		/// tests waiting to be executed
		/// </summary>
		std::list<std::shared_ptr<Input>> _waitingTests;
		/// <summary>
		/// number of currently active tests
		/// </summary>
		int32_t _activetests = 0;
		/// <summary>
		/// if true, after the current has been completed the iteration is stopped
		/// </summary>
		bool _stopbatch = false;

		std::mutex _batchlock;

		/// <summary>
		/// start parameters of the instance
		/// </summary>
		DDParameters* _params;

		/// <summary>
		/// Level of the dd-algorithm. This is a progress variable used differently by the different algorithms
		/// </summary>
		int32_t _level = 2;

		#pragma region HelperFunctions

		double lossPrimary(std::shared_ptr<Input> input)
		{
			return 1 - (input->GetPrimaryScore() / _bestScore.first);
		}

		double lossSecondary(std::shared_ptr<Input> input)
		{
			return 1 - (input->GetSecondaryScore() / _bestScore.second);
		}

		#pragma endregion
		
		#pragma region runtime

		std::chrono::steady_clock::time_point __NextGenTime;

		#pragma endregion
	};
}

template <class T>
class RangeIterator
{
	std::vector<std::pair<T, T>>* _ranges;
	RangeSkipOptions _skipoptions;
	/// <summary>
	/// Absolute position relative to length
	/// </summary>
	size_t _position = 0;
	/// <summary>
	/// current range
	/// </summary>
	size_t _posX = 0;
	/// <summary>
	/// current position in range
	/// </summary>
	size_t _posY = 0;
	/// <summary>
	/// total length
	/// </summary>
	T _length = 0;
	/// <summary>
	/// longest range
	/// </summary>
	T _maxRange = 0;
	/// <summary>
	/// number of ranges to skip
	/// </summary>
	size_t _skipRanges = 0;

public:
	RangeIterator(std::vector<std::pair<T, T>>* ranges, RangeSkipOptions skipfirst, size_t skipRanges);
	
	/// <summary>
	/// returns the length of all non-unique items in the range
	/// </summary>
	/// <returns></returns>
	size_t GetLength();

	/// <summary>
	/// Returns the next range with size [length]
	/// This function iterastes over all individual ranges in this iterator
	/// </summary>
	/// <param name="length"></param>
	/// <returns></returns>
	std::vector<T> GetRange(size_t length);

	/// <summary>
	/// Returns a list of ranges that are at most [maxsize] long.
	/// It does not concatenate ranges in this list iterator, instead if they are shorter than [maxsize] they are returned completely
	/// </summary>
	/// <param name="maxsize"></param>
	/// <returns></returns>
	std::vector<std::pair<T, T>> GetRanges(size_t maxsize);
	std::vector<std::pair<T, T>> GetRangesAbove(size_t minsize);

	T GetMaxRange()
	{
		return _maxRange;
	}
};



template <class T>
class RangeCalculator
{
	std::vector<std::pair<T, T>>* _ranges;
	/// <summary>
	/// Absolute position relative to length
	/// </summary>
	size_t _position = 0;
	/// <summary>
	/// current range
	/// </summary>
	size_t _posX = 0;
	/// <summary>
	/// current position in range
	/// </summary>
	size_t _posY = 0;
	/// <summary>
	/// total length
	/// </summary>
	T _length = 0;

	T _orig_length = 0;

public:
	RangeCalculator(std::vector<std::pair<T, T>>* ranges, T orig_length);

	/// <summary>
	/// returns the length of all non-unique items in the range
	/// </summary>
	/// <returns></returns>
	size_t GetLength();

	std::vector<std::pair<T, T>> GetNewRangesWithout(T begin, T count);

private:
	/// <summary>
	/// returns whether the given position is in a range
	/// </summary>
	/// <returns></returns>
	bool IsInRange(T pos);

	/// <summary>
	/// returns the coordinates in the current input of the given position in the original input
	/// [only works for positions outside of ranges]
	/// </summary>
	/// <returns></returns>
	T Coord(T pos);

	/// <summary>
	/// returns the index of the range that pos can be added to
	/// </summary>
	/// <returns></returns>
	int32_t GetMatchingRangeIdx(T pos, T count, bool& found, bool& addend);
};

namespace Functions
{
	class DDTestCallback : public BaseFunction
	{
	public:
		std::shared_ptr<SessionData> _sessiondata;
		std::shared_ptr<DeltaDebugging::DeltaController> _DDcontroller;
		std::shared_ptr<Input> _input;
		std::shared_ptr<DeltaDebugging::Tasks> _batchtasks;

		uint64_t _batchident = 0;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'DDSC'; }
		uint64_t GetType() override { return 'DDSC'; }

		virtual std::shared_ptr<BaseFunction> DeepCopy() override;

		FunctionType GetFunctionType() override { return FunctionType::Light; }

		bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool WriteData(std::ostream* buffer, size_t& offset) override;

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<DDTestCallback>()); }
		void Dispose() override;
		size_t GetLength() override;

		virtual const char* GetName() override
		{
			return "DDTestCallback";
		}
	};

	class DDEvaluateExplicitCallback : public BaseFunction
	{
	public:
		std::shared_ptr<DeltaDebugging::DeltaController> _DDcontroller;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'DDEC'; }
		uint64_t GetType() override { return 'DDEC'; }

		FunctionType GetFunctionType() override { return FunctionType::Medium; }

		virtual std::shared_ptr<BaseFunction> DeepCopy() override;

		bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool WriteData(std::ostream* buffer, size_t& offset) override;

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<DDEvaluateExplicitCallback>()); }
		void Dispose() override;
		size_t GetLength() override;

		virtual const char* GetName() override
		{
			return "DDEvaluateExplicitCallback";
		}
	};

	class DDGenerateComplementCallback : public BaseFunction
	{
	public:
		std::shared_ptr<DeltaDebugging::DeltaController> _DDcontroller;
		std::shared_ptr<Input> _input;
		int32_t _begin = 0;
		int32_t _length = 0;
		double _approxthreshold = 0.f;
		uint64_t _batchident = 0;
		std::shared_ptr<DeltaDebugging::Tasks> _batchtasks;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'DGCC'; }
		uint64_t GetType() override { return 'DGCC'; }

		FunctionType GetFunctionType() override { return FunctionType::Medium; }

		virtual std::shared_ptr<BaseFunction> DeepCopy() override;

		bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool WriteData(std::ostream* buffer, size_t& offset) override;

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<DDGenerateComplementCallback>()); }
		void Dispose() override;
		size_t GetLength() override;

		virtual const char* GetName() override
		{
			return "DDGenerateComplementCallback";
		}
	};

	class DDGenerateCheckSplit : public BaseFunction
	{
	public:
		std::shared_ptr<DeltaDebugging::DeltaController> _DDcontroller;
		std::shared_ptr<Input> _input;
		double _approxthreshold = 0.f;
		uint64_t _batchident = 0;
		std::shared_ptr<DeltaDebugging::Tasks> _batchtasks;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'DGCS'; }
		uint64_t GetType() override { return 'DGCS'; }

		FunctionType GetFunctionType() override { return FunctionType::Medium; }

		virtual std::shared_ptr<BaseFunction> DeepCopy() override;

		bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool WriteData(std::ostream* buffer, size_t& offset) override;

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<DDGenerateCheckSplit>()); }
		void Dispose() override;
		size_t GetLength() override;

		virtual const char* GetName() override
		{
			return "DDGenerateCheckSplit";
		}
	};
}
