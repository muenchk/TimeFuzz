#pragma once

#include <filesystem>

#include "Generator.h"
#include "Oracle.h"

class Settings
{
public:
	/// <summary>
	/// Returns a singleton the the session
	/// </summary>
	/// <returns></returns>
	Settings* GetSingleton();

	/// <summary>
	/// Loads the settings for the working directory
	/// </summary>
	void Load();
	/// <summary>
	/// Saves the settings in the working directory
	/// </summary>
	void Save();

	/// <summary>
	/// Type of the oracle that is executed
	/// </summary>
	Oracle::OracleType oracle = Oracle::OracleType::PythonScript;
	const char * oracle_NAME = "OracleType";
	/// <summary>
	/// path the the oracle executing program/script OR the PUT itself
	/// </summary>
	std::filesystem::path oraclepath;
	const char* oraclepath_NAME = "Path";

	struct General
	{
		/// <summary>
		/// uses the number of available hardware threads for background computation
		/// </summary>
		bool usehardwarethreads = false;
		const char* usehardwarethreads_NAME = "UseHardwareThreads";
		/// <summary>
		/// the number of backgroundthreads to use if [usehardwarethreads] is set to false
		/// </summary>
		int numthreads = 20;
		const char* numthreads_NAME = "NumThreads";

		/// <summary>
		/// whether to enable saves
		/// </summary>
		bool enablesaves = true;
		/// <summary>
		/// the path at which saves are made
		/// </summary>
		std::string savepath = ".";
	};

	General general;

	struct Optimization
	{
		/// <summary>
		/// input sequences or partial input sequences, that do not result in either a bug or a successful run, may be used as starting point for generation and expanded, until the produce a clear result
		/// [This should not be used with PUT that produce UNDEFINED oracle results]
		/// </summary>
		bool constructinputsiteratively = true;
		const char* constructinputsiteratively_NAME = "ConstructInputsIteratively";
	};

	Optimization optimization;

	struct Methods
	{
		/// <summary>
		/// whether to use delta debugging
		/// </summary>
		bool deltadebugging = true;
		const char* deltadebugging_NAME = "DeltaDebugging";
	};

	Methods methods;

	struct Generation
	{
		/// <summary>
		/// maximum generated inputs per generation cycle
		/// </summary>
		int generationsize = 100;
		const char* generationsize_NAME = "GenerationSize";

		/// <summary>
		/// starting parameter for automatic generationsize scaling
		/// </summary>
		float generationtweakstart = 0.2;
		const char* generationtweakstart_NAME = "GenerationTweakStart";

		/// <summary>
		/// max parameter for automatic generationsize scaling
		/// </summary>
		float generationtweakmax = 10;
		const char* generationtweakmax_NAME = "GenerationTweakMax";
	};

	Generation generation;

	struct EndConditions
	{
		/// <summary>
		/// whether to stop the execution after a set amount of iterations
		/// </summary>
		bool use_maxiterations = true;
		const char* use_maxiterations_NAME = "UseMaxIterations";
		/// <summary>
		/// the number of iterations to run
		/// </summary>
		int maxiterations = 10;
		const char* maxiterations_NAME = "MaxIterations";
		/// <summary>
		/// whether to stop the execution after a set amount of failing inputs have been found
		/// </summary>
		bool use_foundnegatives = true;
		const char* use_foundnegatives_NAME = "UseFoundNegatives";
		/// <summary>
		/// the number of failing inputs to generate
		/// </summary>
		int foundnegatives = 100;
		const char* foundnegatives_NAME = "FoundNegatives";
		/// <summary>
		/// whether to stop the execution after a certain time has passed
		/// </summary>
		bool use_timeout = true;
		const char* use_timeout_NAME = "UseTimeout";
		/// <summary>
		/// the time after which to stop the execution
		/// </summary>
		int timeout = 60;
		const char* timeout_NAME = "Timeout";
	};

	EndConditions conditions;
};
