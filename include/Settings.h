#pragma once

#include <filesystem>
#include <chrono>

#include "Generator.h"
#include "Oracle.h"
#include "Form.h"

#define SI_NO_CONVERSION
#define SI_SUPPORT_IOSTREAMS


namespace DeltaDebugging
{

	enum class DDMode;
}

class CmdArgs
{
public:
	static inline bool _settings = false;
	static inline std::wstring _settingspath = L"";
	static inline bool _load = false;
	static inline std::string _loadname = "";
	static inline bool _print = false;
	static inline bool _num = false;
	static inline int32_t _number;
	static inline bool _dry = false;
	static inline bool _dryi = false;
	static inline std::string _dryinput = "";
	static inline bool _responsive = false;
	static inline bool _ui = false;
	static inline bool _reloadConfig = false;
	static inline std::filesystem::path workdir = L"";
	static inline bool _debug = false;
	static inline bool _updateGrammar = false;
	static inline bool _doNotLoadExclusionTree = false;
	static inline bool _clearTasks = false;
	static inline bool _consoleUI = false;
	static inline bool _saveStatus = false;
	static inline int32_t _saveStatusSeconds = 60;
	static inline bool _customsavepath = false;
	static inline std::filesystem::path _savepath;
};

enum class RangeSkipOptions
{
	None = 0,
	SkipFirst = 1,
	SkipLast = 2,
};

class Settings : public Form
{
private:
	bool initialized = false;
	const int32_t classversion = 0x4;
	/// <summary>
	/// skip reading from savefile
	/// </summary>

public:
	enum GenerationSourcesType
	{
		/// <summary>
		/// Sources for the next generation are taken from the inputs with the best primary score
		/// </summary>
		FilterPrimaryScore = 0,
		/// <summary>
		/// Sources for the next generation are taken from the inputs with the best primary score
		/// </summary>
		FilterPrimaryScoreRelative = 1 << 0,
		/// <summary>
		/// Sources for the next generation are taken from the inputs with the best secondary score
		/// </summary>
		FilterSecondaryScore = 1 << 1,
		/// <summary>
		/// Sources for the next generation are taken from the inputs with the best secondary score
		/// </summary>
		FilterSecondaryScoreRelative = 1 << 2,
		/// <summary>
		/// Sources for the next generation are taken from the longest inputs
		/// </summary>
		FilterLength = 1 << 3,
	};

	/// <summary>
	/// Returns a singleton the the session
	/// </summary>
	/// <returns></returns>
	static Settings* GetSingleton();

	/// <summary>
	/// Loads the settings for the working directory
	/// </summary>
	void Load(std::wstring path = L"", bool reload = false);
	/// <summary>
	/// Saves the settings in the working directory
	/// </summary>
	void Save(std::wstring path = L"");

	size_t GetStaticSize(int32_t version) override;
	size_t GetDynamicSize() override;
	bool WriteData(std::ostream* buffer, size_t& offset) override;
	bool ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver) override;
	void Delete(Data* data);
	void Clear() override;
	inline static bool _registeredFactories = false;
	static void RegisterFactories();
	size_t MemorySize() override;
	
	int32_t GetType() override
	{
		return FormType::Settings;
	}
	static int32_t GetTypeStatic()
	{
		return FormType::Settings;
	}

	struct Runtime
	{
		bool enableExclusionTree = true;
	};

	Runtime runtime;

	struct Oracle
	{
		/// <summary>
		/// Type of the oracle that is executed
		/// </summary>
		::Oracle::PUTType oracle = ::Oracle::PUTType::Undefined;
		const char* oracle_NAME = "PUTType";
		/// <summary>
		/// path the the oracle executing program/script OR the PUT itself
		/// </summary>
		std::filesystem::path oraclepath;
		const char* oraclepath_NAME = "Path";
		/// <summary>
		/// path the the oracle executing program/script OR the PUT itself
		/// </summary>
		std::filesystem::path oraclepath_Unix;
		const char* oraclepath_Unix_NAME = "PathUnix";
		/// <summary>
		/// path to the lua script containing the cmd Args
		/// </summary>
		std::string lua_path_cmd = "LuaCmdArgs.lua";
		const char* lua_path_cmd_NAME = "LuaCmdScript";
		/// <summary>
		/// path to the lua script containing the script Args
		/// </summary>
		std::string lua_path_script = "LuaScriptArgs.lua";
		const char* lua_path_script_NAME = "LuaScriptArgsScript";
		/// <summary>
		/// path to the lua script containing the cmd Args
		/// </summary>
		std::string lua_path_cmd_replay = "LuaCmdArgsReplay.lua";
		const char* lua_path_cmd_replay_NAME = "LuaCmdScriptReplay";
		/// <summary>
		/// path to the lua script containing the lua functions
		/// </summary>
		std::string lua_path_oracle = "LuaOracle.lua";
		const char* lua_path_oracle_NAME = "LuaOracleScript";
		/// <summary>
		/// path to the grammar
		/// </summary>
		std::string grammar_path = "Grammar.scala";
		const char* grammar_path_NAME = "Grammar";
	};

	Settings::Oracle oracle;

	std::filesystem::path GetOraclePath();

	struct Controller
	{
		/// <summary>
		/// whether to enable settings for the TaskController
		/// </summary>
		bool activateSettings = false;
		const char* activateSettings_NAME = "ActivateFineGrainedControl";

		/// <summary>
		/// number of threads executing light tasks only
		/// </summary>
		int32_t numLightThreads = 1;
		const char* numLightThreads_NAME = "NumberOfLightThreads";

		/// <summary>
		/// number of threads executing medium tasks
		/// </summary>
		int32_t numMediumThreads = 1;
		const char* numMediumThreads_NAME = "NumberOfMediumThreads";

		/// <summary>
		/// number of threads executing heavy tasks
		/// </summary>
		int32_t numHeavyThreads = 1;
		const char* numHeavyThreads_NAME = "NumberOfHeavyThreads";

		/// <summary>
		/// number of threads executing light, medium, and heavy tasks
		/// </summary>
		int32_t numAllThreads = 0;
		const char* numAllThreads_NAME = "NumberOfAllThreads";
	};

	Controller controller;

	struct General
	{
		/// <summary>
		/// uses the number of available hardware threads for background computation
		/// </summary>
		bool usehardwarethreads = false;
		const char* usehardwarethreads_NAME = "UseHardwareThreads";
		/// <summary>
		/// the number of backgroundthreads to use for computations if [usehardwarethreads] is set to false
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
		/// Maximum memory to be used by the host process in MB
		/// </summary>
		int64_t memory_limit = 25600;
		const char* memory_limit_NAME = "MemoryLimit";

		/// <summary>
		/// If the soft limit for host process memory consumption is exceeded, the program will
		/// periodically try to free up memory
		/// </summary>
		int64_t memory_softlimit = 1024;
		const char* memory_softlimit_NAME = "MemorySoftLimit";

		/// <summary>
		/// minimal interval between two sweeps of the memory to reduce consumption
		/// </summary>
		std::chrono::milliseconds memory_sweep_period = std::chrono::milliseconds(10000);
		const char* memory_sweep_period_NAME = "MemorySweepPeriod";

		/// <summary>
		/// period of the execution handler
		/// </summary>
		std::chrono::nanoseconds testEnginePeriodWindows = std::chrono::nanoseconds(500000);
		std::chrono::nanoseconds testEnginePeriodUnix = std::chrono::nanoseconds(500000);
		const char* testEnginePeriodWindows_NAME = "TestEnginePeriodWindows";
		const char* testEnginePeriodUnix_NAME = "TestEnginePeriodUnix";

		std::chrono::nanoseconds testEnginePeriod()
		{
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
			return testEnginePeriodWindows;
#else
			return testEnginePeriodUnix;
#endif
		}


	};

	General general;

	struct SaveFiles
	{
		/// <summary>
		/// whether to enable saves
		/// </summary>
		bool enablesaves = true;
		const char* enablesaves_NAME = "EnableSaves";
		/// <summary>
		/// automatically saves after x tests [set to 0 to disable]
		/// </summary>
		int64_t autosave_every_tests = 1000;
		const char* autosave_every_tests_NAME = "AutosavePeriodTests";
		/// <summary>
		/// automatically saves after x seconds [set to 0 to disable]
		/// </summary>
		int64_t autosave_every_seconds = 300;
		const char* autosave_every_seconds_NAME = "AutosavePeriodSeconds";
		/// <summary>
		/// automatically saves after a generation has been completed
		/// </summary>
		bool saveAfterEachGeneration = false;
		const char* saveAfterEachGeneration_NAME = "SaveAfterEachGeneration";
		/// <summary>
		/// the path at which saves are made
		/// </summary>
		std::string savepath = "./saves";
		const char* savepath_NAME = "SavePath";
		/// <summary>
		/// the name of the savefiles
		/// </summary>
		std::string savename = "USS Enterprise";
		const char* savename_NAME = "SaveName";
		/// <summary>
		/// compression level for LZMA compression
		/// set to -1 to disable compression
		/// </summary>
		int32_t compressionLevel = -1;
		const char* compressionLevel_NAME = "CompressionLevel";
		/// <summary>
		/// Whether to use the extreme compression preset for LZMA save file compression.
		/// Using this reduces save file size but increases the time it takes to save
		/// drastically.
		/// </summary>
		bool compressionExtreme = false;
		const char* compressionExtreme_NAME = "CompressionExtreme";
	};

	SaveFiles saves;

	struct Optimization
	{
		/// <summary>
		/// input sequences or partial input sequences, that do not result in either a bug or a successful run, may be used as starting point for generation and expanded, until the produce a clear result
		/// [This should not be used with PUT that produce UNDEFINED oracle results]
		/// </summary>
		bool constructinputsiteratively = true;
		const char* constructinputsiteratively_NAME = "ConstructInputsIteratively";

		bool disableExclusionTree = false;
		const char* disableExclusionTree_NAME = "DisableExclusionTree";

		/// <summary>
		/// only inserts inputs into the exclusiontree with length less than value
		/// </summary>
		int32_t exclusionTreeLengthLimit = 1000;
		const char* exclusionTreeLengthLimit_NAME = "ExclusionTreeLengthLimit";
	};

	Optimization optimization;

	struct Methods
	{
		// ----- These settings are final for the session, otherwise reproduction is impossible -----

		/// <summary>
		/// When iteratively construction inputs, they are generated from already existing derivation trees
		/// by backtracking a certain number of nodes and expanding the tree afterwards, this setting
		/// provide the minimum amount of sequence nodes to be backtracked [cannot be set less than one]
		/// </summary>
		int32_t IterativeConstruction_Extension_Backtrack_min = 0;
		const char* IterativeConstruction_Extension_Backtrack_min_NAME = "IterativeConstruction_Extension_Backtrack_Min";

		/// <summary>
		/// This sets the maximum number of sequence nodes to be backtracked when extending an unfinished
		/// input
		/// </summary>
		int32_t IterativeConstruction_Extension_Backtrack_max = 0;
		const char* IterativeConstruction_Extension_Backtrack_max_NAME = "IterativeConstruction_Extension_Backtrack_Max";

		/// <summary>
		/// When iteratively construction inputs, failing input must be backtracked before they can be 
		/// extended to new inputs.
		/// This setting sets the minimum number of sequence nodes to backtrack before expanding the derivation
		/// tree once more.
		/// </summary>
		int32_t IterativeConstruction_Backtrack_Backtrack_min = 10;
		const char* IterativeConstruction_Backtrack_Backtrack_min_NAME = "IterativeConstruction_Backtrack_Backtrack_Min";

		/// <summary>
		/// This setting sets the maximum number of sequence nodes to backtrack on failing inputs before 
		/// expanding the derivation tree once more.
		/// </summary>
		int32_t IterativeConstruction_Backtrack_Backtrack_max = 30;
		const char* IterativeConstruction_Backtrack_Backtrack_max_NAME = "IterativeConstruction_Backtrack_Backtrack_Max";
	};

	Methods methods;

	struct DeltaDebugging
	{
		/// <summary>
		/// whether to use delta debugging
		/// </summary>
		bool deltadebugging = true;
		const char* deltadebugging_NAME = "DeltaDebugging";

		/// <summary>
		/// only executes generates subsets above this length
		/// </summary>
		int64_t executeAboveLength = 10;
		const char* executeAboveLength_NAME = "ExecuteAboveLength";

		/// <summary>
		/// the delta debugging algorithm to use
		/// </summary>
		::DeltaDebugging::DDMode mode;
		const char* mode_NAME = "DDMode";

		/// <summary>
		/// Wether delta debugging may be run with the goal of score optimization
		/// </summary>
		bool allowScoreOptimization = true;
		const char* allowScoreOptimization_NAME = "AllowScoreOptimization";

		/// <summary>
		/// Runs standard DD after Score Optimization DD on positiv generated inputs
		/// </summary>
		bool runReproduceResultsAfterScoreApproxOnPositive = false;
		const char* runReproduceResultsAfterScoreApproxOnPositive_NAME = "RunStandardDDAfterScoreApproxOnPositive";

		/// <summary>
		/// loss threshold under which inputs are considered acceptable 
		/// </summary>
		double optimizationLossThreshold = 0.05f;
		const char* optimizationLossThreshold_NAME = "OptmizationLossThreshold";

		/// <summary>
		/// absolute loss value for which inputs are considered acceptable
		/// </summary>
		double optimizationLossAbsolute = 50;
		const char* optimizationLossAbsolute_NAME = "OptimizationLossAbsolute";

		/// <summary>
		/// For [PrimaryScore] and [SecondaryScore] focused delta debugging only.
		/// Generated inputs primary score is approximated by using the monotonity of the primary Score function.
		/// An input for which an extended version exists, must have less than or equal the primary score of the 
		/// shortest extension that exists.
		/// </summary>
		bool approximativeTestExecution = true;
		const char* approximativeTestExecution_NAME = "ApproximativeTestExecution";

		/// <summary>
		/// For [PrimaryScore] and [SecondaryScore] focused delta debugging only.
		/// Generated inputs minimum approximated score must be greater than [Delta Debugged inputs primary score * thisvalue]
		/// </summary>
		double approximativeExecutionThreshold = 0.2f;
		const char* approximativeExecutionThreshold_NAME = "ApproximativeExecutionThreshold";

		RangeSkipOptions skipoptions = RangeSkipOptions::SkipFirst;
		const char* skipoptions_NAME = "ScoreProgressSkipOptions";

		bool ScoreProgressSkipDeltaDebuggedParts = true;
		const char* ScoreProgressSkipDeltaDebuggedParts_NAME = "ScoreProgressSkipDeltaDebuggedParts";

		/// <summary>
		/// size of batches that are processed at a time
		/// </summary>
		int32_t batchprocessing = 0;
		const char* batchprocessing_NAME = "BatchProcessing";

		/// <summary>
		/// max number of tests that may be run by any instance of DD that is executed at the end of each iteration
		/// </summary>
		int32_t budgetGenEnd = 0;
		const char* budgetGenEnd_NAME = "Budget";
	};

	DeltaDebugging dd;

	struct Generation
	{
		/// <summary>
		/// maximum generated inputs per generation cycle
		/// </summary>
		int32_t generationsize = 1000;
		const char* generationsize_NAME = "GenerationSize";

		/// <summary>
		/// maximum number of generated inputs that may be waiting to be tested at any time
		/// [this is the number of inputs the program will generate, if a test finishes
		/// a new test will be generated, until the generationsize has been reached]
		/// [this applies even if generationalMode has been disabled]
		/// </summary>
		int32_t activeGeneratedInputs = 100;
		const char* activeGeneratedInputs_NAME = "ActiveGeneratedInputs";

		/// <summary>
		/// allowance of new inputs generated by the same task 
		/// </summary>
		int32_t generationstep = 10;

		/// <summary>
		/// [true] Inputs will be generated in generations
		/// [false] Inputs will be generated continuously until a goal is reached
		/// </summary>
		bool generationalMode = true;
		const char* generationalMode_NAME = "GenerationalMode";
		
		/// <summary>
		/// Chance that a completely new random input is being generated instead
		/// of extending or backtracking an existing prior input.
		/// This only applies to the second generation and later, as only random 
		/// inputs are being generated in the first generation.
		/// [Max: 100, Min:0]
		/// </summary>
		int32_t chance_new_generations = 10;
		const char* chance_new_generations_NAME = "ChanceGenerateNewInput";

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

		/// <summary>
		/// The number of inputs that get delta debugged at the end of a generation
		/// </summary>
		int32_t numberOfInputsToDeltaDebugPerGeneration = 5;
		const char* numberOfInputsToDeltaDebugPerGeneration_NAME = "NumberOfInputsToDeltaDebugPerGeneration";

		/// <summary>
		/// Whether backtracking may be used on failing inputs, [also includes failing inputs in delta debugging]
		/// </summary>
		bool allowBacktrackFailedInputs = true;
		const char* allowBacktrackFailedInputs_NAME = "AllowBacktrackingFailedInputs";

		/// <summary>
		/// The number of inputs that are being carried over from one generation to the next as sources for input
		/// extension and backtracking
		/// </summary>
		int32_t numberOfSourcesPerGeneration = 20;
		const char* numberOfSourcesPerGeneration_NAME = "NumberOfSourcesPerGeneration";

		/// <summary>
		/// how sources for the next generation are being chosen
		/// </summary>
		GenerationSourcesType sourcesType = GenerationSourcesType::FilterPrimaryScore;
		const char* sourcesType_NAME = "SourcesType";

		/// <summary>
		/// minimum input length generated (/ extended) in each generation
		/// </summary>
		int32_t generationLengthMin = 1000;
		const char* generationLengthMin_NAME = "MinimumGenerationLength";

		/// <summary>
		/// maximum input length generated (/extended) in each generation
		/// </summary>
		int32_t generationLengthMax = 10000;
		const char* generationLengthMax_NAME = "MaximumGenerationLength";

		/// <summary>
		/// the maximum number of derived inputs that can fail for an input to be elligible to be a source
		/// </summary>
		uint64_t maxNumberOfFailsPerSource = 100;
		const char* maxNumberOfFailsPerSource_NAME = "MaximumNumberOfFailsPerSource";

		/// <summary>
		/// the maximum number of total derived inputs for an input to be elligible to be a source
		/// </summary>
		uint64_t maxNumberOfGenerationsPerSource = 100;
		const char* maxNumberOfGenerationsPerSource_NAME = "MaximumNumberOfGenerationsPerSource";

		/// <summary>
		/// at the beginning of a new generation the globally best inputs are chosen, instead of choosing only inputs from 
		/// the last generation.
		/// </summary>
		bool globalSources = false;
		const char* globalSources_NAME = "UseGlobalSources";
	};

	Generation generation;

	struct EndConditions
	{
		/// <summary>
		/// whether to stop the execution after a set amount of failing inputs have been found
		/// </summary>
		bool use_foundnegatives = false;
		const char* use_foundnegatives_NAME = "UseFoundNegatives";
		/// <summary>
		/// the number of failing inputs to generate
		/// </summary>
		uint64_t foundnegatives = 100;
		const char* foundnegatives_NAME = "FoundNegatives";
		/// <summary>
		/// whether to stop the execution after a set amount of failing inputs have been found
		/// </summary>
		bool use_foundpositives = false;
		const char* use_foundpositives_NAME = "UseFoundPositives";
		/// <summary>
		/// the number of failing inputs to generate
		/// </summary>
		uint64_t foundpositives = 100;
		const char* foundpositives_NAME = "FoundPositives";
		/// <summary>
		/// whether to stop the execution after a certain time has passed
		/// </summary>
		bool use_timeout = false;
		const char* use_timeout_NAME = "UseTimeout";
		/// <summary>
		/// the time after which to stop the execution
		/// </summary>
		int64_t timeout = 60;
		const char* timeout_NAME = "Timeout";
		/// <summary>
		/// whether the program ends when a sepcific number of tests have been executed
		/// </summary>
		bool use_overalltests = false;
		const char* use_overalltests_NAME = "UseOverallTests";
		/// <summary>
		/// the number of overal tests that have been executed
		/// </summary>
		uint64_t overalltests = 10000;
		const char* overalltests_NAME = "OverallTests";

		/// <summary>
		/// whether to stop the execution after a set amount of generations have been done
		/// </summary>
		bool use_generations = false;
		const char* use_generations_NAME = "UseGenerations";
		/// <summary>
		/// the number of generations after which to stop generating
		/// </summary>
		uint64_t generations = 100;
		const char* generations_NAME = "Generations";
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

	struct Fixes
	{
		/// <summary>
		/// disables sleeping of the execution handler
		/// </summary>
		bool disableExecHandlerSleep = false;
		const char* disableExecHandlerSleep_NAME = "DisableExecHandlerSleeping";

		/// <summary>
		/// sometimes using g++ results in tests running into timeouts even though they shouldn't.
		/// This setting repeats timeouted tests to get a better result.
		/// Only use if you are sure the PUT does not actually run into any timeouts.
		/// </summary>
		bool repeatTimeoutedTests = false;
		const char* repeatTimeoutedTests_NAME = "RepeatTimeoutedTests";
	};

	Fixes fixes;
};
