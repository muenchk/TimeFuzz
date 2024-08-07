#pragma once

#include <filesystem>

#include "Generator.h"
#include "Oracle.h"

#define SI_NO_CONVERSION
#define SI_SUPPORT_IOSTREAMS

class Settings
{
private:
	bool initialized = false;

public:
	/// <summary>
	/// Returns a singleton the the session
	/// </summary>
	/// <returns></returns>
	static Settings* GetSingleton();

	/// <summary>
	/// Loads the settings for the working directory
	/// </summary>
	void Load(std::wstring path = L"");
	/// <summary>
	/// Saves the settings in the working directory
	/// </summary>
	void Save(std::wstring path = L"");

	const int32_t classversion = 0x1;
	/// <summary>
	/// returns the total size of the fields with static size
	/// </summary>
	/// <returns></returns>
	size_t GetStaticSize(int32_t version = 0x1);
	/// <summary>
	/// returns the total size of all fields of this instance
	/// </summary>
	/// <returns></returns>
	size_t GetDynamicSize();
	/// <summary>
	/// saves all relevant information of this instance to the given buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <returns></returns>
	bool WriteData(unsigned char* buffer, size_t offset);
	/// <summary>
	/// reads all relevant information of this instance from the buffer
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	bool ReadData(unsigned char* buffer, size_t offset, size_t length);
	
	static int32_t GetType()
	{
		return 'SETT';
	}

	/// <summary>
	/// Type of the oracle that is executed
	/// </summary>
	Oracle::PUTType oracle = Oracle::PUTType::Undefined;
	const char * oracle_NAME = "PUTType";
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
		int32_t numthreads = 20;
		const char* numthreads_NAME = "NumThreads";

		/// <summary>
		/// number of threads used for computational purposes
		/// </summary>
		int32_t numcomputingthreads = 1;
		const char* numcomputingthreads_NAME = "NumComputeThreads";
		
		/// <summary>
		/// Number of tests run concurrently
		/// </summary>
		int32_t concurrenttests = 1;
		const char* concurrenttests_NAME = "ConcurrentTests";

		/// <summary>
		/// whether to enable saves
		/// </summary>
		bool enablesaves = true;
		/// <summary>
		/// automatically saves after x tests [set to 0 to disable]
		/// </summary>
		int64_t autosave_every_tests = 1000;
		/// <summary>
		/// automatically saves after x seconds [set to 0 to disable]
		/// </summary>
		int64_t autosave_every_seconds = 300;
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
		int32_t generationsize = 100;
		const char* generationsize_NAME = "GenerationSize";

		/// <summary>
		/// starting parameter for automatic generationsize scaling
		/// </summary>
		float generationtweakstart = 0.2f;
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
		int32_t maxiterations = 10;
		const char* maxiterations_NAME = "MaxIterations";
		/// <summary>
		/// whether to stop the execution after a set amount of failing inputs have been found
		/// </summary>
		bool use_foundnegatives = true;
		const char* use_foundnegatives_NAME = "UseFoundNegatives";
		/// <summary>
		/// the number of failing inputs to generate
		/// </summary>
		int64_t foundnegatives = 100;
		const char* foundnegatives_NAME = "FoundNegatives";
		/// <summary>
		/// whether to stop the execution after a certain time has passed
		/// </summary>
		bool use_timeout = true;
		const char* use_timeout_NAME = "UseTimeout";
		/// <summary>
		/// the time after which to stop the execution
		/// </summary>
		int64_t timeout = 60;
		const char* timeout_NAME = "Timeout";
		/// <summary>
		/// whether the program ends when a sepcific number of tests have been executed
		/// </summary>
		bool use_overalltests = true;
		const char* use_overalltests_NAME = "UseOverallTests";
		/// <summary>
		/// the number of overal tests that have been executed
		/// </summary>
		uint64_t overalltests = 10000;
		const char* overalltests_NAME = "OverallTests";
	};

	EndConditions conditions;

	struct Tests
	{
		/// <summary>
		/// whether to execute tests as fragments or complete
		/// </summary>
		bool executeFragments = true;
		const char* executeFragments_NAME = "ExecuteFragments";
		/// <summary>
		/// whether to apply a timeout to a complete testcase
		/// </summary>
		bool use_testtimeout = true;
		const char* use_testtimeout_NAME = "UseTestTimeout";
		/// <summary>
		/// the timeout for a complete testcase in microseconds
		/// </summary>
		int64_t testtimeout = 60000000;
		const char* testtimeout_NAME = "TestTimeout";
		/// <summary>
		/// whether to apply a timeout to every run fragement
		/// </summary>
		bool use_fragmenttimeout = false;
		const char* use_fragmenttimeout_NAME = "UseFragmentTimeout";
		/// <summary>
		/// the timeout for a fragment in microseconds
		/// </summary>
		int64_t fragmenttimeout = 1000000;
		const char* fragmenttimeout_NAME = "FragmentTimeout";

		/// <summary>
		/// whether to store the PUT outputs for all run tests [doesn't disable option below when false]
		/// </summary>
		bool storePUToutput = true;
		const char* storePUToutput_NAME = "StorePUTOutput";
		/// <summary>
		/// whether to store the PUT outputs for successful test runs [applies even if above is false]
		/// </summary>
		bool storePUToutputSuccessful = true;
		const char* storePUToutputSuccessful_NAME = "StorePUTOutputSuccessful";

		/// <summary>
		/// maximum of memory the PUT may consume, it will be killed above this value
		/// [set to 0 to disable]
		/// </summary>
		int64_t maxUsedMemory = 0;
		const char* maxUsedMemory_NAME = "MaxUsedMemory";
	};

	Tests tests;
};
