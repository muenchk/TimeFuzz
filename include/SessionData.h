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

struct TestExitStats
{
	uint64_t natural = 0;
	uint64_t lastinput = 0;
	uint64_t terminated = 0;
	uint64_t timeout = 0;
	uint64_t fragmenttimeout = 0;
	uint64_t memory = 0;
	uint64_t initerror = 0;
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
	};

	struct InputNodeLess
	{
		bool operator()(const std::shared_ptr<InputNode>& lhs, const std::shared_ptr<InputNode>& rhs) const
		{
			if (lhs->primary == rhs->primary)
				return lhs->secondary > rhs->secondary;
			else
				return lhs->primary > rhs->primary;
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
	std::list<FormID> _positiveInputs;
	/// <summary>
	/// shared lock for the list of positive input formids
	/// </summary>
	std::shared_mutex _positiveInputsLock;
	/// <summary>
	/// list of input formids that have negative test results
	/// </summary>
	std::set<std::shared_ptr<InputNode>, InputNodeLess> _negativeInputs;
	/// <summary>
	/// shared lock for the list of negative input formids
	/// </summary>
	std::shared_mutex _negativeInputsLock;
	/// <summary>
	/// list of input formids that are unfinished sequences
	/// </summary>
	std::set<std::shared_ptr<InputNode>, InputNodeLess> _unfinishedInputs;
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

	void AddInput(std::shared_ptr<Input>& input, EnumType list, double optionalweight = 0.0f);

	friend class SessionFunctions;
	friend class SessionStatistics;

public:
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
	/// Number of tests that couldn't be added
	/// </summary>
	std::atomic<int64_t> _addtestFails = 0;

	/// <summary>
	/// time_point of the last performed end and saving checks
	/// </summary>
	std::chrono::steady_clock::time_point lastchecks;

	/// <summary>
	/// time_point of the last performed memory sweep
	/// </summary>
	std::chrono::steady_clock::time_point lastmemorysweep;

	uint64_t memory_mem = 0;
	bool memory_outofmemory = false;
	std::chrono::steady_clock::time_point memory_outofmemory_timer;
	bool memory_ending = false;

	const int32_t classversion = 0x3;

	inline static bool _registeredFactories = false;

public:
	void Clear();
	~SessionData();

	Data* data;

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
	/// returns the failure rate of input generation
	/// </summary>
	/// <returns></returns>
	double GetGenerationFailureRate();
	/// <summary>
	/// returns the testexitstats
	/// </summary>
	/// <returns></returns>
	TestExitStats& GetTestExitStats();

	std::vector<std::shared_ptr<Input>> GetTopK(int32_t k);

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
	bool WriteData(unsigned char* buffer, size_t& offset) override;
	/// <summary>
	/// Reads the class data from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="length"></param>
	/// <param name="resolver"></param>
	/// <returns></returns>
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;

	bool ReadData0x1(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);
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

	#pragma endregion
};
