#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

class Input;
class Grammar;
class Generator;
class Settings;

class SessionData
{
	struct Iteration
	{
		/// <summary>
		/// vector holding all new inputs generated in the last iteration
		/// </summary>
		std::vector<std::shared_ptr<Input>> _newinputs;
	};

	struct Session
	{
		/// <summary>
		/// vector holding all new inputs generated in the last call to the session
		/// </summary>
		std::vector<std::shared_ptr<Input>> _newinputs;
		/// <summary>
		/// number of positives in _newinputs
		/// </summary>
		uint32_t positives = 0;
		/// <summary>
		/// number of negatives in _newinputs
		/// </summary>
		uint32_t negatives = 0;
		/// <summary>
		/// number of inputs with an undefined result in _newinputs;
		/// </summary>
		uint32_t undefined = 0;
	};

	struct Alltime
	{
		/// <summary>
		/// hashmap holding all inputs generated so far
		/// </summary>
		std::unordered_map<std::size_t, std::shared_ptr<Input>> _inputs;
		/// <summary>
		/// number of positives in _inputs
		/// </summary>
		uint32_t positives = 0;
		/// <summary>
		/// number of negatives in _inputs
		/// </summary>
		uint32_t negatives = 0;
		/// <summary>
		/// number of inputs with an undefined result in _newinputs;
		/// </summary>
		uint32_t undefined = 0;
	};

	/// <summary>
	/// pointer to the session settings
	/// </summary>
	std::shared_ptr<Settings> _settings;

	SessionData::Iteration _iteration;

	SessionData::Session _session;

	SessionData::Alltime _alltime;

public:
	void Clear();
	~SessionData();
};
