#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <semaphore>
#include <boost/circular_buffer.hpp>
#include <mutex>
#include <atomic>
#include <set>
#include <list>

#include "Form.h"
#include "Utility.h"
#include "StableSet.h"
#include "OrderedSet.h"

class Input;
class Grammar;
class Generator;
class Settings;
class SessionFunctions;
class SessionStatistics;
class ExclusionTree;
class TaskController;
class ExecutionHandler;
class Oracle;
class Generation;

namespace Functions
{
	class MasterGenerationCallback;
}

struct TestExitStats
{
	uint64_t natural = 0;
	uint64_t lastinput = 0;
	uint64_t terminated = 0;
	uint64_t timeout = 0;
	uint64_t fragmenttimeout = 0;
	uint64_t memory = 0;
	uint64_t pipe = 0;
	uint64_t initerror = 0;
	uint64_t repeat = 0;
};

template <class Key, class Compare>
std::set<Key, Compare> make_set(Compare compare)
{
	return std::set<Key, Compare>(compare);
}



class SessionData : public Form
{
	TestExitStats exitstats;

	struct InputNode
	{
		std::weak_ptr<Input> input;
		double primary;
		double secondary;
		double weight;
		size_t length;
	};

	struct InputNodeLess
	{
		bool operator()(const std::shared_ptr<InputNode>& lhs, const std::shared_ptr<InputNode>& rhs) const
		{
			if (!rhs)
				return false;
			if (lhs->primary == rhs->primary)
				if (lhs->secondary == rhs->secondary)
					return lhs->length < rhs->length;
				else
					return lhs->secondary > rhs->secondary;
			else
				return lhs->primary > rhs->primary;
		}
	};

	struct InputNodeLessSecondary
	{
		bool operator()(const std::shared_ptr<InputNode>& lhs, const std::shared_ptr<InputNode>& rhs) const
		{
			if (!rhs)
				return false;
			if (lhs->secondary == rhs->secondary)
				if (lhs->primary == rhs->primary)
					return lhs->length < rhs->length;
				else
					return lhs->primary > rhs->primary;
			else
				return lhs->secondary > rhs->secondary;
		}
	};

	struct InputNodeLength
	{
		bool operator()(const std::shared_ptr<InputNode>& lhs, const std::shared_ptr<InputNode>& rhs) const
		{
			if (!rhs)
				return false;
			if (lhs->length == rhs->length)
				if (lhs->primary == rhs->primary)
					return lhs->secondary > rhs->secondary;
				else
					return lhs->primary > rhs->primary;
			else
				return lhs->length > rhs->length;
		}
	};

	/// <summary>
	/// total number of positive test results
	/// </summary>
	std::atomic<int64_t> _positiveInputNumbers = 0;
	/// <summary>
	/// total number of negative test results
	/// </summary>
	std::atomic<int64_t> _negativeInputNumbers = 0;
	/// <summary>
	/// total number of unfinished test results
	/// </summary>
	std::atomic<int64_t> _unfinishedInputNumbers = 0;
	/// <summary>
	/// total number of undefined test results
	/// </summary>
	std::atomic<int64_t> _undefinedInputNumbers = 0;

	/// <summary>
	/// list of input formids that have positive test results
	/// </summary>
	std::vector<std::shared_ptr<Input>> _positiveInputs;
	//std::ordered_multiset<std::shared_ptr<Input>> _positiveInputs;
	/// <summary>
	/// shared lock for the list of positive input formids
	/// </summary>
	std::shared_mutex _positiveInputsLock;
	/// <summary>
	/// list of input formids that have negative test results
	/// </summary>
	std::ordered_multiset<std::shared_ptr<InputNode>, InputNodeLength, InputNodeLess, InputNodeLessSecondary> _negativeInputs;
	/// <summary>
	/// shared lock for the list of negative input formids
	/// </summary>
	std::shared_mutex _negativeInputsLock;
	/// <summary>
	/// list of input formids that are unfinished sequences
	/// </summary>
	std::ordered_multiset<std::shared_ptr<InputNode>, InputNodeLength, InputNodeLess, InputNodeLessSecondary> _unfinishedInputs;
	/// <summary>
	/// shared lock for the list of unfinished inputs
	/// </summary>
	std::shared_mutex _unfinishedInputsLock;
	/// <summary>
	/// list of input formids that have an undefined result
	/// </summary>
	std::list<FormID> _undefinedInputs;
	/// <summary>
	/// shared lock for the list of undefined inputs
	/// </summary>
	std::shared_mutex _undefinedInputLock;

	/// <summary>
	/// list of last run inputs
	/// </summary>
	boost::circular_buffer<std::weak_ptr<Input>> _lastrun = boost::circular_buffer<std::weak_ptr<Input>>(10);

	std::atomic_flag _lastrunFlag = ATOMIC_FLAG_INIT;

	/// <summary>
	/// multiset with a stable size holding the top 100 unfinished inputs sorted after primary score
	/// </summary>
	std::stable_multiset<std::shared_ptr<InputNode>, InputNodeLess> _topK_primary_Unfinished{ 100 };
	std::stable_multiset<std::shared_ptr<InputNode>, InputNodeLessSecondary> _topK_secondary_Unfinished{ 100 };
	std::stable_multiset<std::shared_ptr<InputNode>, InputNodeLess> _topK_primary{ 100 };
	std::stable_multiset<std::shared_ptr<InputNode>, InputNodeLessSecondary> _topK_secondary{ 100 };
	std::stable_multiset<std::shared_ptr<InputNode>, InputNodeLength> _topK_length{ 100 };
	std::stable_multiset<std::shared_ptr<InputNode>, InputNodeLength> _topK_length_Unfinished{ 100 };

	static void SetInsert(std::shared_ptr<InputNode>);
	static void SetRemove(std::shared_ptr<InputNode>);

	std::shared_mutex _multiset_lock;

	void AddInput(std::shared_ptr<Input>& input, EnumType list, double optionalweight = 0.0f);

	/// <summary>
	/// map holding all prior generations for fast access
	/// </summary>
	std::unordered_map<FormID, std::shared_ptr<Generation>> _generations;
	/// <summary>
	/// lock for threadsafe access to _generations
	/// </summary>
	std::shared_mutex _generationsLock;
	/// <summary>
	/// indicates whether the current generation is ending, aka. the end generation callback has been deployed
	/// </summary>
	std::atomic<bool> _generationEnding = false;

	/// <summary>
	/// currently active generation
	/// </summary>
	std::shared_ptr<Generation> _generation;
	/// <summary>
	/// flag for spinlock for _generation
	/// </summary>
	std::atomic_flag _generationFlag = ATOMIC_FLAG_INIT;
	/// <summary>
	/// thread safe access to _generation
	/// </summary>
	/// <returns></returns>
	std::shared_ptr<Generation> GetGen();
	/// <summary>
	/// thread safe updaate of _generation
	/// </summary>
	/// <param name="gen"></param>
	void SetGen(std::shared_ptr<Generation> gen);
	/// <summary>
	/// thread safe exchange of _generation
	/// </summary>
	/// <param name="newgen"></param>
	/// <returns></returns>
	std::shared_ptr<Generation> ExchangeGen(std::shared_ptr<Generation> newgen);
	/// <summary>
	/// ID of the current generation
	/// </summary>
	FormID _generationID;
	/// <summary>
	/// ID of the last generation
	/// </summary>
	FormID _lastGenerationID = 0;
	/// <summary>
	/// generation used when generational mode is disabled
	/// </summary>
	std::shared_ptr<Generation> _defaultGen;

	/// <summary>
	/// This lock allows to regulate input generation. Any function that generates new inputs from the current generation
	/// must hold a readlock to this mutex. Any function that handles Generational transfers or sources must
	/// hold a writelock to this mutex.
	/// </summary>
	std::shared_mutex _InputGenerationLock;

	std::atomic<bool> _blockInputGeneration = false;

	friend class Session;
	friend class SessionFunctions;
	friend class SessionStatistics;
	friend class Functions::MasterGenerationCallback;

public:
	std::atomic<bool> _generationFinishing = false;

	/// <summary>
	/// the number of retries for single input generations
	/// </summary>
	const uint64_t GENERATION_RETRIES = 1000;
	/// <summary>
	/// the number of recent generations to consider when calculating the failing percentage
	/// </summary>
	const uint64_t GENERATION_WEIGHT_BUFFER_SIZE = 1000;
	/// <summary>
	/// limit for the recent amount of generation fails in comparison to  successful generations
	/// </summary>
	const double GENERATION_WEIGHT_LIMIT = 0.9f;

private:
	/// <summary>
	/// the period of end session and save checks are performed
	/// </summary>
	std::chrono::nanoseconds _checkperiod = std::chrono::nanoseconds(1000);

	/// <summary>
	/// lock used for session functions operating from SessionFunctions class
	/// </summary>
	std::binary_semaphore _sessionFunctionLock{ 1 };
	/// <summary>
	/// buffer saving results of recent generations
	/// </summary>
	boost::circular_buffer<unsigned char> _recentfailes = boost::circular_buffer<unsigned char>(GENERATION_WEIGHT_BUFFER_SIZE, 0);
	std::mutex _lockRecentFails;
	/// <summary>
	/// generation failure rate
	/// </summary>
	double _failureRate = 0.f;

	/// <summary>
	/// how often overall input generation has failed
	/// </summary>
	std::atomic<int64_t> _generationfails = 0;
	/// <summary>
	/// number of inputs generated so far
	/// </summary>
	std::atomic<int64_t> _generatedinputs = 0;
	/// <summary>
	/// Number of inputs generated that have been excluded via prefixing
	/// </summary>
	std::atomic<int64_t> _generatedWithPrefix = 0;
	/// <summary>
	/// Number of inputs that were generated but not run due to exclusion by core approximation
	/// </summary>
	std::atomic<int64_t> _generatedExcludedApproximation = 0;
	/// <summary>
	/// Number of tests that couldn't be added
	/// </summary>
	std::atomic<int64_t> _addtestFails = 0;

	/// <summary>
	/// time_point of the last performed end and saving checks
	/// </summary>
	std::chrono::steady_clock::time_point _lastchecks;

	/// <summary>
	/// time_point of the last performed memory sweep
	/// </summary>
	std::chrono::steady_clock::time_point _lastMemorySweep;

	uint64_t _memory_mem = 0;
	bool _memory_outofmemory = false;
	std::chrono::steady_clock::time_point _memory_outofmemory_timer;
	bool _memory_ending = false;

	std::chrono::steady_clock::time_point _cleanup;
	std::chrono::seconds _cleanup_period = std::chrono::seconds(60);

	int32_t _max_generation_callbacks = 0;
	std::atomic_int_fast32_t _active_generation_callbacks = 0;

	const int32_t classversion = 0x2;

	inline static bool _registeredFactories = false;

	/// <summary>
	/// Lock used to make sure only one thread gets to end the session
	/// </summary>
	std::mutex _endMutex;

public:
	void Clear();
	~SessionData();

	Data* data;

	void Init();

	void SetMaxGenerationCallbacks(int32_t max);

	/// <summary>
	/// The Task used to execute the PUT and retrieve the oracle result
	/// </summary>
	std::shared_ptr<Oracle> _oracle;

	/// <summary>
	/// Task controller for the active session
	/// </summary>
	std::shared_ptr<TaskController> _controller;

	/// <summary>
	/// Executionhandler for the active session
	/// </summary>
	std::shared_ptr<ExecutionHandler> _exechandler;

	/// <summary>
	/// Generator for fuzzing
	/// </summary>
	std::shared_ptr<Generator> _generator;

	/// <summary>
	/// The grammar for the generation
	/// </summary>
	std::shared_ptr<Grammar> _grammar;

	/// <summary>
	/// the settings for this session
	/// </summary>
	std::shared_ptr<Settings> _settings;

	/// <summary>
	/// ExclusionTree of the session
	/// </summary>
	std::shared_ptr<ExclusionTree> _excltree;

	/// <summary>
	/// Returns the FormID of the current generation
	/// </summary>
	/// <returns></returns>
	FormID GetCurrentGenerationID()
	{
		return _generationID;
	}
	/// <summary>
	/// Returns the current generation
	/// </summary>
	/// <returns></returns>
	std::shared_ptr<Generation> GetCurrentGeneration();
	/// <summary>
	/// Returns the specified generation
	/// </summary>
	/// <returns></returns>
	std::shared_ptr<Generation> GetGeneration(FormID generationID);
	/// <summary>
	/// Returns the specified generation
	/// </summary>
	/// <returns></returns>
	std::shared_ptr<Generation> GetGenerationByNumber(int32_t generationNumber);
	/// <summary>
	/// Returns the last generation
	/// </summary>
	/// <returns></returns>
	std::shared_ptr<Generation> GetLastGeneration();
	/// <summary>
	/// Returns the FormIDs of all generations
	/// </summary>
	/// <param name="generations"></param>
	void GetGenerationIDs(std::vector<std::pair<FormID, int32_t>>& generations, size_t& size);

	/// <summary>
	/// Sets up a new generation
	/// </summary>
	void SetNewGeneration(bool force = false);

	/// <summary>
	/// checks wether the current generation has ended and initiates proper procedures
	/// </summary>
	bool CheckGenerationEnd(bool toomanyfails = false);
	/// <summary>
	/// returns whether the current generation is ending
	/// </summary>
	/// <returns></returns>
	bool GetGenerationEnding();
	bool GetGenerationFinishing();

	/// <summary>
	/// get the number of inputs that may be generated by the calling function
	/// </summary>
	/// <returns></returns>
	int64_t GetNumberInputsToGenerate();
	int64_t CheckNumberInputsToGenerate(int64_t generated, int64_t failcount, int64_t togenerate);
	void FailNumberInputsToGenerate(int64_t fails, int64_t generated);

	void IncGenerationFails() {
		_generationfails++;
	}
	void PushRecenFails(unsigned char value)
	{
		std::unique_lock guard(_lockRecentFails);
		_recentfailes.push_back(value);
	}
	void IncAddTestFails()
	{
		_addtestFails++;
	}
	void IncGeneratedWithPrefix()
	{
		_generatedWithPrefix++;
	}
	void IncGeneratedInputs()
	{
		_generatedinputs++;
	}
	void IncExcludedApproximation()
	{
		_generatedExcludedApproximation++;
	}

	/// <summary>
	/// returns the number of attempts that failed at generating any inputs
	/// </summary>
	/// <returns></returns>
	int64_t GetGenerationFails();
	/// <summary>
	/// returns the number of overall generated inputs
	/// </summary>
	/// <returns></returns>
	int64_t GetGeneratedInputs();
	/// <summary>
	/// returns the number of inputs excluded based on prefix
	/// </summary>
	/// <returns></returns>
	int64_t GetGeneratedPrefix();
	/// <summary>
	/// returns the number of inputs excluded based on prefix
	/// </summary>
	/// <returns></returns>
	int64_t GetAddTestFails();
	/// <summary>
	/// returns the number of inputs excluded based on primary score approximation
	/// </summary>
	/// <returns></returns>
	int64_t GetExcludedApproximation();
	/// <summary>
	/// returns the failure rate of input generation
	/// </summary>
	/// <returns></returns>
	double GetGenerationFailureRate();
	/// <summary>
	/// returns the testexitstats
	/// </summary>
	/// <returns></returns>
	TestExitStats& GetTestExitStats();

	/// <summary>
	/// Returns the currently used memory in MB
	/// </summary>
	/// <returns></returns>
	uint64_t GetUsedMemory();

	std::vector<std::shared_ptr<Input>> GetTopK(int32_t k, size_t min_length_unfinished = 0, size_t min_length_failing = 0);
	std::vector<std::shared_ptr<Input>> GetTopK_Unfinished(int32_t k, size_t min_length = 0);
	std::vector<std::shared_ptr<Input>> GetTopK_Secondary(int32_t k, size_t min_length_unfinished = 0, size_t min_length_failing = 0);
	std::vector<std::shared_ptr<Input>> GetTopK_Secondary_Unfinished(int32_t k, size_t min_length = 0);
	std::vector<std::shared_ptr<Input>> GetTopK_Length(int32_t k, size_t min_length_unfinished = 0, size_t min_length_failing = 0);
	std::vector<std::shared_ptr<Input>> GetTopK_Length_Unfinished(int32_t k, size_t min_length = 0);

	std::vector<std::shared_ptr<Input>> FindKSources(int32_t k, std::set<std::shared_ptr<Input>> exclusionlist, bool allowFailing, size_t min_length_unfinished = 0, size_t min_length_failing = 0);

	void GetPositiveInputs(int32_t k, std::vector<std::shared_ptr<Input>>& posinputs);

	/// <summary>
	/// Visits all positive inputs
	/// </summary>
	/// <param name="visitor">return value indicates whether the next input should be visited next</param>
	void VisitPositiveInputs(std::function<bool(std::shared_ptr<Input>)> visitor, size_t begin = 0);

	/// <summary>
	/// Visits last run inputs
	/// </summary>
	/// <param name="visitor">return value indicates whether the next input should be visited next</param>
	void VisitLastRun(std::function<bool(std::shared_ptr<Input>)> visitor);

	/// <summary>
	/// aqcuires a readers lock for input generation
	/// </summary>
	void Acquire_InputGenerationReadersLock();
	/// <summary>
	/// releases a readers lock for input generation
	/// </summary>
	void Release_InputGenerationReadersLock();

	/// <summary>
	/// aqcuires a writers lock for input generation
	/// </summary>
	void Acquire_InputGenerationWritersLock();
	/// <summary>
	/// releases a writers lock for input generation
	/// </summary>
	void Release_InputGenerationWritersLock();

	/// <summary>
	/// blocks generation of new inputs
	/// </summary>
	bool BlockInputGeneration();
	/// <summary>
	/// returns whether new inputs can be generated
	/// </summary>
	/// <returns></returns>
	bool CanGenerate();

	bool TryLockEnd()
	{
		return _endMutex.try_lock();
	}

	void UnlockEnd()
	{
		return _endMutex.unlock();
	}

	#pragma region Form

	/// <summary>
	/// Returns the size of the classes static members
	/// </summary>
	/// <param name="version"></param>
	/// <returns></returns>
	size_t GetStaticSize(int32_t version) override;
	/// <summary>
	/// Returns the overall size of all members to be saved
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize() override;
	/// <summary>
	/// Writes all class data to the buffer at the given offset
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	bool WriteData(std::ostream* buffer, size_t& offset, size_t length) override;
	/// <summary>
	/// Reads the class data from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="length"></param>
	/// <param name="resolver"></param>
	/// <returns></returns>
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;

	bool ReadData0x1(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver);
	int32_t GetType() override
	{
		return FormType::SessionData;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::SessionData;
	}
	void Delete(Data*) override { Clear(); }
	static void RegisterFactories();
	size_t MemorySize() override;

	#pragma endregion

private:

	#pragma region HelperFunctions

	#pragma endregion
};
