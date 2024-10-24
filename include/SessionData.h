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

template <class Key, class Compare>
std::set<Key, Compare> make_set(Compare compare)
{
	return std::set<Key, Compare>(compare);
}



class SessionData
{
	struct InputNode
	{
		std::weak_ptr<Input> input;
		double weight;
	};

	struct InputNodeLess
	{
		bool operator()(const std::shared_ptr<InputNode>& lhs, const std::shared_ptr<InputNode>& rhs) const
		{
			return lhs->weight < rhs->weight;
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

	void AddInput(std::shared_ptr<Input>& input,EnumType list, double optionalweight = 0.0f);


	/// <summary>
	/// pointer to the session settings
	/// </summary>
	std::weak_ptr<Settings> _settings;

	void Delete(Data*) {}

	friend class SessionFunctions;
	friend class SessionStatistics;
	

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

	/// <summary>
	/// how often overall input generation has failed
	/// </summary>
	int64_t _generationfails = 0;
	/// <summary>
	/// number of inputs generated so far
	/// </summary>
	int64_t _generatedinputs = 0;
	/// <summary>
	/// Number of inputs generated that have been excluded via prefixing
	/// </summary>
	int64_t _generatedWithPrefix = 0;

	/// <summary>
	/// time_point of the last performed end and saving checks
	/// </summary>
	std::chrono::steady_clock::time_point lastchecks;

	const int32_t classversion = 0x1;

public:
	void Clear();
	~SessionData();

	/// <summary>
	/// Returns the size of the classes static members
	/// </summary>
	/// <param name="version"></param>
	/// <returns></returns>
	size_t GetStaticSize(int32_t version = 0x1);
	/// <summary>
	/// Returns the overall size of all members to be saved
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize();
	/// <summary>
	/// Writes all class data to the buffer at the given offset
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <returns></returns>
	bool WriteData(unsigned char* buffer, size_t& offset);
	/// <summary>
	/// Reads the class data from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="offset"></param>
	/// <param name="length"></param>
	/// <param name="resolver"></param>
	/// <returns></returns>
	bool ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver);
};
