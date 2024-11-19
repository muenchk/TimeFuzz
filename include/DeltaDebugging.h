#pragma once

#include "Form.h"
#include "Function.h"
#include "Input.h"

#include <memory>
#include <vector>
#include <set>

class SessionData;

namespace DeltaDebugging
{
	class DeltaController;
}

namespace Functions
{
	class DDTestCallback : public BaseFunction{
	public:
		std::shared_ptr<SessionData> _sessiondata;
		std::shared_ptr<DeltaDebugging::DeltaController> _DDcontroller;
		std::shared_ptr<Input> _input;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'DDSC'; }
		uint64_t GetType() override { return 'DDSC'; }

		FunctionType GetFunctionType() override { return FunctionType::Light; }

		bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool WriteData(unsigned char* buffer, size_t& offset) override;

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<DDTestCallback>()); }
		void Dispose() override;
		size_t GetLength() override;
	};

	class DDEvaluateExplicitCallback : public BaseFunction
	{
	public:
		std::shared_ptr<DeltaDebugging::DeltaController> _DDcontroller;

		void Run() override;
		static uint64_t GetTypeStatic() { return 'DDEC'; }
		uint64_t GetType() override { return 'DDEC'; }

		FunctionType GetFunctionType() override { return FunctionType::Medium; }

		bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		bool WriteData(unsigned char* buffer, size_t& offset) override;

		static std::shared_ptr<BaseFunction> Create() { return dynamic_pointer_cast<BaseFunction>(std::make_shared<DDEvaluateExplicitCallback>()); }
		void Dispose() override;
		size_t GetLength() override;
	};
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
		/// Acceptable loss for total prmary score for the test to be considered a valid result
		/// </summary>
		float acceptableLossPrimary = 0.05f;
		/// <summary>
		/// Acceptable loss for total secondary score for the test to be considered a valid result
		/// </summary>
		float acceptableLossSecondary = 0.05f;
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

		void CallbackTest(std::shared_ptr<Input> input);
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
		int32_t GetTestsRemaining() { return _remainingtests; }
		int32_t GetTestsTotal() { return _totaltests; }
		int32_t GetLevel() { return _level; }
		bool Finished() { return _finished; }
		std::unordered_map<std::shared_ptr<Input>, std::pair<double, int32_t>>* GetResults() { return &_results; }
		std::shared_ptr<Input> GetInput() { return _input; }
		std::shared_ptr<Input> GetOriginalInput() { return _origInput; }
		std::set<std::shared_ptr<Input>, FormIDLess<Input>>* GetActiveInputs() { return &_activeInputs; }

		
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
		bool WriteData(unsigned char* buffer, size_t& offset) override;
		/// <summary>
		/// reads all relevant information of this instance from the buffer
		/// </summary>
		/// <param name="buffer"></param>
		/// <param name="length"></param>
		bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
		/// <summary>
		/// Deletes all relevant for fields
		/// </summary>
		void Delete(Data* data) override;
		static void RegisterFactories();
		static int32_t GetTypeStatic() { return FormType::DeltaController; }
		virtual int32_t GetType() { return FormType::DeltaController; }

	private:
		const int32_t classversion = 0x1;
		static inline bool _registeredFactories = false;

		struct DeltaInformation
		{
			int32_t positionbegin = 0;
			int32_t length = 0;
			bool complement = false;
		};

		/// <summary>
		/// Generates [number] subset from [::_input]
		/// </summary>
		/// <param name="number"></param>
		/// <returns></returns>
		std::vector<std::shared_ptr<Input>> GenerateSplits(int32_t number, std::vector<DeltaInformation>& splitinfo);
		std::vector<std::shared_ptr<Input>> GenerateComplements(std::vector<DeltaInformation>& splitinfo);


		/// <summary>
		/// generates the first level of tests for standard mode
		/// </summary>
		void StandardGenerateFirstLevel();
		/// <summary>
		/// generates the next level of tests for standard mode
		/// </summary>
		void StandardGenerateNextLevel();
		/// <summary>
		/// evaluates a completed level for standard mode
		/// </summary>
		void StandardEvaluateLevel();

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
		/// number of tests skipped
		/// </summary>
		std::atomic<int32_t> _remainingtests = 0;
		/// <summary>
		/// total number of tests executed
		/// </summary>
		std::atomic<int32_t> _totaltests = 0;

		/// <summary>
		/// whether we are completely done with delta debugging
		/// </summary>
		bool _finished = false;

		/// <summary>
		/// map of results, corresponding loss if available, and the level
		/// </summary>
		std::unordered_map<std::shared_ptr<Input>, std::pair<double, int32_t>> _results;

		std::shared_ptr<Input> _origInput;

		std::shared_ptr<Input> _input;

		std::shared_ptr<SessionData> _sessiondata;

		std::shared_ptr<DeltaController> _self;

		std::set<std::shared_ptr<Input>, FormIDLess<Input>> _activeInputs;

		std::set<std::shared_ptr<Input>> _completedTests;

		std::mutex _completedTestsLock;

		std::shared_ptr<Functions::BaseFunction> _callback;

		/// <summary>
		/// start parameters of the instance
		/// </summary>
		DDParameters* _params;

		/// <summary>
		/// Level of the dd-algorithm. This is a progress variable used differently by the different algorithms
		/// </summary>
		int32_t _level = 2;

		/// <summary>
		/// Clears all internals
		/// </summary>
		void Clear();
	};
}
