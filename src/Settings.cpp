#include <memory>

#include "Logging.h"
#include "Settings.h"
#include "SimpleIni.h"
#include "Utility.h"
#include "BufferOperations.h"
#include "DeltaDebugging.h"

Settings* Settings::GetSingleton()
{
	static Settings session;
	return std::addressof(session);
}

std::filesystem::path Settings::GetOraclePath()
{
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	return oracle.oraclepath;
#else
	return oracle.oraclepath_Unix;
#endif
}

void Settings::Load(std::wstring path, bool reload)
{
	if (initialized && !reload)
		return;
	loginfo("Begin loading settings.");
	initialized = true;

	constexpr auto defpath = L"config.ini";
	if (path == L"")
		path = (CmdArgs::workdir / std::wstring(defpath)).wstring();
	if (path == L".")
		path = (std::filesystem::path(path) / defpath).wstring();
	if (std::filesystem::is_directory(path))
		path = (std::filesystem::path(path) / defpath).wstring();


	CSimpleIniA ini;

	ini.SetUnicode();

#if defined(unix) || defined(__unix__) || defined(__unix)
	std::ifstream istr(std::filesystem::path(path), std::ios_base::in);
	ini.LoadData(istr);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	ini.LoadFile(path.c_str());
#endif

	//oracle
	oracle.oracle = ::Oracle::ParseType(ini.GetValue("Oracle", oracle.oracle_NAME, "Script"));
	loginfo("{}{} {}", "Oracle:           ", oracle.oracle_NAME, ::Oracle::TypeString(oracle.oracle));
	oracle.oraclepath = std::filesystem::path(ini.GetValue("Oracle", oracle.oraclepath_NAME, "."));
	loginfo("{}{} {}", "Oracle:           ", oracle.oraclepath_NAME, oracle.oraclepath.string());
	oracle.oraclepath_Unix = std::filesystem::path(ini.GetValue("Oracle", oracle.oraclepath_Unix_NAME, "."));
	loginfo("{}{} {}", "Oracle:           ", oracle.oraclepath_Unix_NAME, oracle.oraclepath_Unix.string());
	oracle.lua_path_cmd = std::string(ini.GetValue("Oracle", oracle.lua_path_cmd_NAME, oracle.lua_path_cmd.c_str()));
	loginfo("{}{} {}", "Oracle:           ", oracle.lua_path_cmd_NAME, oracle.lua_path_cmd);
	oracle.lua_path_script = std::string(ini.GetValue("Oracle", oracle.lua_path_script_NAME, oracle.lua_path_script.c_str()));
	loginfo("{}{} {}", "Oracle:           ", oracle.lua_path_script_NAME, oracle.lua_path_script);
	oracle.lua_path_cmd_replay = std::string(ini.GetValue("Oracle", oracle.lua_path_cmd_replay_NAME, oracle.lua_path_cmd_replay.c_str()));
	loginfo("{}{} {}", "Oracle:           ", oracle.lua_path_cmd_replay_NAME, oracle.lua_path_cmd_replay);
	oracle.lua_path_oracle = std::string(ini.GetValue("Oracle", oracle.lua_path_oracle_NAME, oracle.lua_path_oracle.c_str()));
	loginfo("{}{} {}", "Oracle:           ", oracle.lua_path_oracle_NAME, oracle.lua_path_oracle);
	oracle.grammar_path = std::string(ini.GetValue("Oracle", oracle.grammar_path_NAME, oracle.grammar_path.c_str()));
	loginfo("{}{} {}", "Oracle:           ", oracle.grammar_path_NAME, oracle.grammar_path);


	// general
	general.usehardwarethreads = ini.GetBoolValue("General", general.usehardwarethreads_NAME, general.usehardwarethreads);
	loginfo("{}{} {}", "General:          ", general.usehardwarethreads_NAME, general.usehardwarethreads);
	general.numthreads = ini.GetBoolValue("General", general.numthreads_NAME, general.numthreads);
	loginfo("{}{} {}", "General:          ", general.numthreads_NAME, general.numthreads);
	general.numcomputingthreads = (int32_t)ini.GetLongValue("General", general.numcomputingthreads_NAME, general.numcomputingthreads);
	loginfo("{}{} {}", "General:          ", general.numcomputingthreads_NAME, general.numcomputingthreads);
	general.concurrenttests = (int32_t)ini.GetLongValue("General", general.concurrenttests_NAME, general.concurrenttests);
	loginfo("{}{} {}", "General:          ", general.concurrenttests_NAME, general.concurrenttests);
	general.memory_limit = (int64_t)ini.GetLongValue("General", general.memory_limit_NAME, (long)general.memory_limit);
	loginfo("{}{} {}", "General:          ", general.memory_limit_NAME, general.memory_limit);
	general.memory_softlimit = (int64_t)ini.GetLongValue("General", general.memory_softlimit_NAME, (long)general.memory_softlimit);
	loginfo("{}{} {}", "General:          ", general.memory_softlimit_NAME, general.memory_softlimit);
	general.memory_sweep_period = std::chrono::milliseconds((int64_t)ini.GetLongValue("General", general.memory_sweep_period_NAME, (long)general.memory_sweep_period.count()));
	loginfo("{}{} {}", "General:          ", general.memory_sweep_period_NAME, general.memory_sweep_period.count());

	// saves
	saves.enablesaves = ini.GetBoolValue("SaveFiles", saves.enablesaves_NAME, saves.enablesaves);
	loginfo("{}{} {}", "SaveFiles:          ", saves.enablesaves_NAME, saves.enablesaves);
	saves.autosave_every_tests = (int64_t)ini.GetLongValue("SaveFiles", saves.autosave_every_tests_NAME, (long)saves.autosave_every_tests);
	loginfo("{}{} {}", "SaveFiles:          ", saves.autosave_every_tests_NAME, saves.autosave_every_tests);
	saves.autosave_every_seconds = (int64_t)ini.GetLongValue("SaveFiles", saves.autosave_every_seconds_NAME, (long)saves.autosave_every_seconds);
	loginfo("{}{} {}", "SaveFiles:          ", saves.autosave_every_seconds_NAME, saves.autosave_every_seconds);
	saves.savepath = (std::string)ini.GetValue("SaveFiles", saves.savepath_NAME, saves.savepath.c_str());
	loginfo("{}{} {}", "SaveFiles:          ", saves.savepath_NAME, saves.savepath);
	saves.savename = (std::string)ini.GetValue("SaveFiles", saves.savename_NAME, saves.savename.c_str());
	loginfo("{}{} {}", "SaveFiles:          ", saves.savepath_NAME, saves.savename);
	saves.compressionLevel = (int32_t)ini.GetLongValue("SaveFiles", saves.compressionLevel_NAME, saves.compressionLevel);
	loginfo("{}{} {}", "SaveFiles:          ", saves.compressionLevel_NAME, saves.compressionLevel);
	saves.compressionExtreme = ini.GetBoolValue("SaveFiles", saves.compressionExtreme_NAME, saves.compressionExtreme);
	loginfo("{}{} {}", "SaveFiles:          ", saves.compressionExtreme_NAME, saves.compressionExtreme);

	// optimization
	optimization.constructinputsiteratively = ini.GetBoolValue("Optimization", optimization.constructinputsiteratively_NAME, optimization.constructinputsiteratively);
	loginfo("{}{} {}", "Optimization:     ", optimization.constructinputsiteratively_NAME, optimization.constructinputsiteratively);
	optimization.disableExclusionTree = ini.GetBoolValue("Optimization", optimization.disableExclusionTree_NAME, optimization.disableExclusionTree);
	loginfo("{}{} {}", "Optimization:     ", optimization.disableExclusionTree_NAME, optimization.disableExclusionTree);

	// delta debugging
	dd.deltadebugging = ini.GetBoolValue("DeltaDebugging", dd.deltadebugging_NAME, dd.deltadebugging);
	loginfo("{}{} {}", "DeltaDebugging:          ", dd.deltadebugging_NAME, dd.deltadebugging);
	dd.executeAboveLength = ini.GetLongValue("DeltaDebugging", dd.executeAboveLength_NAME, (long)dd.executeAboveLength);
	loginfo("{}{} {}", "DeltaDebugging:          ", dd.executeAboveLength_NAME, dd.executeAboveLength);
	dd.mode = (::DeltaDebugging::DDMode)ini.GetLongValue("DeltaDebugging", dd.mode_NAME, (long)dd.mode);
	loginfo("{}{} {}", "DeltaDebugging:          ", dd.mode_NAME, (long)dd.mode);
	dd.allowScoreOptimization = ini.GetBoolValue("DeltaDebugging", dd.allowScoreOptimization_NAME, dd.allowScoreOptimization);
	loginfo("{}{} {}", "DeltaDebugging:          ", dd.allowScoreOptimization_NAME, dd.allowScoreOptimization);
	dd.optimizationLossThreshold = ini.GetDoubleValue("DeltaDebugging", dd.optimizationLossThreshold_NAME, dd.optimizationLossThreshold);
	loginfo("{}{} {}", "DeltaDebugging:          ", dd.optimizationLossThreshold_NAME, dd.optimizationLossThreshold);
	dd.approximativeTestExecution = ini.GetBoolValue("DeltaDebugging", dd.approximativeTestExecution_NAME, dd.approximativeTestExecution);
	loginfo("{}{} {}", "DeltaDebugging:          ", dd.approximativeTestExecution_NAME, dd.approximativeTestExecution);
	dd.approximativeExecutionThreshold = ini.GetDoubleValue("DeltaDebugging", dd.approximativeExecutionThreshold_NAME, dd.approximativeExecutionThreshold);
	loginfo("{}{} {}", "DeltaDebugging:          ", dd.approximativeExecutionThreshold_NAME, dd.approximativeExecutionThreshold);

	// generation
	generation.generationsize = (int32_t)ini.GetLongValue("Generation", generation.generationsize_NAME, generation.generationsize);
	generation.generationstep = (int32_t)std::lround(std::log(generation.generationsize) / std::log(1.3));
	if (generation.generationstep < 10)
		generation.generationstep = 10;
	loginfo("{}{} {}", "Generation:       ", generation.generationsize_NAME, generation.generationsize);
	generation.generationalMode = ini.GetBoolValue("Generation", generation.generationalMode_NAME, generation.generationalMode);
	loginfo("{}{} {}", "Generation:       ", generation.generationalMode_NAME, generation.generationalMode);
	generation.activeGeneratedInputs = (int32_t)ini.GetLongValue("Generation", generation.activeGeneratedInputs_NAME, generation.activeGeneratedInputs);
	loginfo("{}{} {}", "Generation:       ", generation.activeGeneratedInputs_NAME, generation.activeGeneratedInputs);

	generation.generationtweakstart = (float)ini.GetDoubleValue("Generation", generation.generationtweakstart_NAME, generation.generationtweakstart);
	loginfo("{}{} {}", "Generation:       ", generation.generationtweakstart_NAME, generation.generationtweakstart);
	generation.generationtweakmax = (float)ini.GetDoubleValue("Generation", generation.generationtweakmax_NAME, generation.generationtweakmax);
	loginfo("{}{} {}", "Generation:       ", generation.generationtweakmax_NAME, generation.generationtweakmax);

	generation.allowBacktrackFailedInputs = ini.GetBoolValue("Generation", generation.allowBacktrackFailedInputs_NAME, generation.allowBacktrackFailedInputs);
	loginfo("{}{} {}", "Generation:       ", generation.allowBacktrackFailedInputs_NAME, generation.allowBacktrackFailedInputs);
	generation.numberOfInputsToDeltaDebugPerGeneration = (int32_t)ini.GetLongValue("Generation", generation.numberOfInputsToDeltaDebugPerGeneration_NAME, generation.numberOfInputsToDeltaDebugPerGeneration);
	loginfo("{}{} {}", "Generation:       ", generation.numberOfInputsToDeltaDebugPerGeneration_NAME, generation.numberOfInputsToDeltaDebugPerGeneration);
	generation.numberOfSourcesPerGeneration = (int32_t)ini.GetLongValue("Generation", generation.numberOfSourcesPerGeneration_NAME, generation.numberOfSourcesPerGeneration);
	loginfo("{}{} {}", "Generation:       ", generation.numberOfSourcesPerGeneration_NAME, generation.numberOfSourcesPerGeneration);
	generation.sourcesType = (GenerationSourcesType)ini.GetLongValue("Generation", generation.sourcesType_NAME, (int32_t)generation.sourcesType);
	loginfo("{}{} {}", "Generation:       ", generation.sourcesType_NAME, (int32_t)generation.sourcesType);

	generation.generationLengthMin = (int32_t)ini.GetLongValue("Generation", generation.generationLengthMin_NAME, generation.generationLengthMin);
	loginfo("{}{} {}", "Generation:       ", generation.generationLengthMin_NAME, generation.generationLengthMin);
	generation.generationLengthMax = (int32_t)ini.GetLongValue("Generation", generation.generationLengthMax_NAME, generation.generationLengthMax);
	loginfo("{}{} {}", "Generation:       ", generation.generationLengthMax_NAME, generation.generationLengthMax);

	// endconditions
	conditions.use_foundnegatives = ini.GetBoolValue("EndConditions", conditions.use_foundnegatives_NAME, conditions.use_foundnegatives);
	loginfo("{}{} {}", "EndConditions:    ", conditions.use_foundnegatives_NAME, conditions.use_foundnegatives);
	conditions.foundnegatives = (uint64_t)ini.GetLongValue("EndConditions", conditions.foundnegatives_NAME, (long)conditions.foundnegatives);
	loginfo("{}{} {}", "EndConditions:    ", conditions.foundnegatives_NAME, conditions.foundnegatives);
	conditions.use_foundpositives = ini.GetBoolValue("EndConditions", conditions.use_foundpositives_NAME, conditions.use_foundpositives);
	loginfo("{}{} {}", "EndConditions:    ", conditions.use_foundpositives_NAME, conditions.use_foundpositives);
	conditions.foundpositives = (uint64_t)ini.GetLongValue("EndConditions", conditions.foundpositives_NAME, (long)conditions.foundpositives);
	loginfo("{}{} {}", "EndConditions:    ", conditions.foundpositives_NAME, conditions.foundpositives);
	conditions.use_timeout = ini.GetBoolValue("EndConditions", conditions.use_timeout_NAME, conditions.use_timeout);
	loginfo("{}{} {}", "EndConditions:    ", conditions.use_timeout_NAME, conditions.use_timeout);
	conditions.timeout = ini.GetLongValue("EndConditions", conditions.timeout_NAME, (long)conditions.timeout);
	loginfo("{}{} {}", "EndConditions:    ", conditions.timeout_NAME, conditions.timeout);
	conditions.use_overalltests = ini.GetBoolValue("EndConditions", conditions.use_overalltests_NAME, conditions.use_overalltests);
	loginfo("{}{} {}", "EndConditions:    ", conditions.use_overalltests_NAME, conditions.use_overalltests);
	conditions.overalltests = (uint64_t)ini.GetLongValue("EndConditions", conditions.overalltests_NAME, (long)conditions.overalltests);
	loginfo("{}{} {}", "EndConditions:    ", conditions.overalltests_NAME, conditions.overalltests);

	// tests
	tests.executeFragments = ini.GetBoolValue("Tests", tests.executeFragments_NAME, tests.executeFragments);
	loginfo("{}{} {}", "Tests:       ", tests.executeFragments_NAME, tests.executeFragments);
	tests.use_testtimeout = ini.GetBoolValue("Tests", tests.use_testtimeout_NAME, tests.use_testtimeout);
	loginfo("{}{} {}", "Tests:       ", tests.use_testtimeout_NAME, tests.use_testtimeout);
	tests.testtimeout = (int64_t)ini.GetLongValue("Tests", tests.testtimeout_NAME, (long)tests.testtimeout);
	loginfo("{}{} {}", "Tests:       ", tests.testtimeout_NAME, tests.testtimeout);
	tests.use_fragmenttimeout = ini.GetBoolValue("Tests", tests.use_fragmenttimeout_NAME, tests.use_fragmenttimeout);
	loginfo("{}{} {}", "Tests:       ", tests.use_fragmenttimeout_NAME, tests.use_fragmenttimeout);
	tests.fragmenttimeout = (int64_t)ini.GetLongValue("Tests", tests.fragmenttimeout_NAME, (long)tests.fragmenttimeout);
	loginfo("{}{} {}", "Tests:       ", tests.fragmenttimeout_NAME, tests.fragmenttimeout);
	tests.storePUToutput = ini.GetBoolValue("Tests", tests.storePUToutput_NAME, tests.storePUToutput);
	loginfo("{}{} {}", "Tests:       ", tests.storePUToutput_NAME, tests.storePUToutput);
	tests.storePUToutputSuccessful = ini.GetBoolValue("Tests", tests.storePUToutputSuccessful_NAME, tests.storePUToutputSuccessful);
	loginfo("{}{} {}", "Tests:       ", tests.storePUToutputSuccessful_NAME, tests.storePUToutputSuccessful);
	tests.maxUsedMemory = (int64_t)ini.GetLongValue("Tests", tests.maxUsedMemory_NAME, (long)tests.maxUsedMemory);
	loginfo("{}{} {}", "Tests:       ", tests.maxUsedMemory_NAME, tests.maxUsedMemory);

	loginfo("Finished loading settings.");
}

void Settings::Save(std::wstring _path)
{
#ifdef NDEBUG
#if defined(unix) || defined(__unix__) || defined(__unix)
	constexpr auto defpath = "config.ini";
	std::string path;
	if (_path == L"")
		path = (CmdArgs::workdir / std::string(defpath)).string();
	else
		path = std::filesystem::path(_path).string();
	if (std::filesystem::is_directory(path))
		path = (std::filesystem::path(path) / defpath).string();
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::wstring path;
	constexpr auto defpath = L"config.ini";
	if (_path == L"")
		path = (CmdArgs::workdir / std::wstring(defpath)).wstring();
	else
		path = std::filesystem::path(_path).wstring();
	if (std::filesystem::is_directory(path))
		path = (std::filesystem::path(path) / defpath).wstring();
#endif

	CSimpleIniA ini;

	ini.SetUnicode();

	// oracle
	ini.SetValue("Oracle", oracle.oracle_NAME, ::Oracle::TypeString(oracle.oracle).c_str(),
		"\\\\ Type of oracle used to execute the Program Under Test. \n"
		"\\\\ \tCMD\t-\tExecutes an unresponsive program. All Inputs are dumped as cmd arguments\n"
		"\\\\ \tScript\t-\tExecutes a script that itself executes the PUT and returns relevant information on STDOUT\n"
		"\\\\ \tSTDIN_Responsive\t-\tExecutes a responsive program. One input is given via STDIN at a time, and a response is awaited.\n"
		"\\\\ \tSTDIN_Dump\t-\tExecutes an unresponsive program. All Inputs are dumped into the standardinput immediately.");
	ini.SetValue("Oracle", oracle.oraclepath_NAME, oracle.oraclepath.string().c_str(), "\\\\ The path to the oracle.");
	ini.SetValue("Oracle", oracle.oraclepath_Unix_NAME, oracle.oraclepath_Unix.string().c_str(), "\\\\ The path to the oracle.");
	ini.SetValue("Oracle", oracle.lua_path_cmd_NAME, oracle.lua_path_cmd.c_str(), "\\\\ The lua script containing the cmdargs function.");
	ini.SetValue("Oracle", oracle.lua_path_script_NAME, oracle.lua_path_script.c_str(), "\\\\ The lua script containing the ScriptArgs function.");
	ini.SetValue("Oracle", oracle.lua_path_cmd_replay_NAME, oracle.lua_path_cmd_replay.c_str(), "\\\\ The lua script containing the cmdargs function for replay inputs.");
	ini.SetValue("Oracle", oracle.lua_path_oracle_NAME, oracle.lua_path_oracle.c_str(), "\\\\ The lua script containing the oracle function.");
	ini.SetValue("Oracle", oracle.grammar_path_NAME, oracle.grammar_path.c_str(), "\\\\ Path to the Grammar.");

	// general
	ini.SetBoolValue("General", general.usehardwarethreads_NAME, general.usehardwarethreads,
		"\\\\ Use the number of available hardware threads for the task scheduler.");
	ini.SetLongValue("General", general.numthreads_NAME, general.numthreads,
		"\\\\ The number of threads to use for the task schedular\n"
		"\\\\ if UseHardwareThreads is set to false.");
	ini.SetLongValue("General", general.numcomputingthreads_NAME, (long)general.numcomputingthreads,
		"\\\\ Number of threads used for computational purposes.");
	ini.SetLongValue("General", general.concurrenttests_NAME, (long)general.concurrenttests,
		"\\\\ Number of tests to be run concurrently.");
	ini.SetLongValue("General", general.memory_limit_NAME, (long)general.memory_limit,
		"\\\\ Maximum memory to be used by the application. [in MB]");
	ini.SetLongValue("General", general.memory_softlimit_NAME, (long)general.memory_softlimit,
		"\\\\ When memory consumption exceeds the soft limit, the application will periodically free used memory. [in MB]");
	ini.SetLongValue("General", general.memory_sweep_period_NAME, (long)general.memory_sweep_period.count(),
		"\\\\ The period of memory sweeps trying to the free up space. [in milliseconds]");

	// saves
	ini.SetBoolValue("SaveFiles", saves.enablesaves_NAME, saves.enablesaves,
		"\\\\ Enables automatic saving.");
	ini.SetLongValue("SaveFiles", saves.autosave_every_tests_NAME, (long)saves.autosave_every_tests,
		"\\\\ Automatically saves after [x] tests have been run."
		"\\\\ Set to 0 to disable.");
	ini.SetLongValue("SaveFiles", saves.autosave_every_seconds_NAME, (long)saves.autosave_every_seconds,
		"\\\\ Automatically saves after [x] seconds."
		"\\\\ Set to 0 to disable.");
	ini.SetValue("SaveFiles", saves.savepath_NAME, saves.savepath.c_str(),
		"\\\\ The path at which saves will be stored.");
	ini.SetValue("SaveFiles", saves.savename_NAME, saves.savename.c_str(),
		"\\\\ The name of savefiles.");
	ini.SetLongValue("SaveFiles", saves.compressionLevel_NAME, saves.compressionLevel,
		"\\\\ CompressionLevel used for LZMA savefile compression\n"
		"\\\\ Set to -1 to disable compression.");
	ini.SetBoolValue("SaveFiles", saves.compressionExtreme_NAME, saves.compressionExtreme,
		"\\\\ Whether to use the extreme compression preset for LZMA save file compression.\n"
		"\\\\ Using this reduces save file size but increases the time it takes to save drastically.");

	// optimization
	ini.SetBoolValue("Optimization", optimization.constructinputsiteratively_NAME, optimization.constructinputsiteratively,
		"\\\\ Input sequences or partial input sequences, that do not result in either a bug or a successful run,\n"
		"\\\\ may be used as a starting point for generation and may be expanded until a result is produced.\n"
		"\\\\ [This should not be used with a PUT that produces undefined oracle results.]");
	ini.SetBoolValue("Optimization", optimization.disableExclusionTree_NAME, optimization.disableExclusionTree,
		"\\\\ Disables the ExclusionTree.");

	// delta debugging
	ini.SetBoolValue("DeltaDebugging", dd.deltadebugging_NAME, dd.deltadebugging, "\\\\ Applies delta debugging to passing inputs, to reduce them as far as possible.");
	ini.SetLongValue("DeltaDebugging", dd.executeAboveLength_NAME, (long)dd.executeAboveLength, "\\\\ Only executes subsets that are longer than this value to save on the number of tests executed.");
	ini.SetLongValue("DeltaDebugging", dd.mode_NAME, (long)dd.mode,
		"\\\\ Algorithm to use for delta debugging.\n"
		"0 = Standard - DDmin developed by Andreas Zeller executing subsets and their complements\n"
		"[currently not selectable, activate AllowScoreOptimization instead] 1 = ScoreProgress - DDmin based custom algorithm that removes entries with that show no score progress");
	ini.SetBoolValue("DeltaDebugging", dd.allowScoreOptimization_NAME, (long)dd.allowScoreOptimization, "\\\\ Allows the application of Delta debugging to reduce generated inputs while optimizing their score instead of reproducing their result.");
	ini.SetDoubleValue("DeltaDebugging", dd.optimizationLossThreshold_NAME, dd.optimizationLossThreshold, "\\\\ The maximum loss when optimizing score to be considered a success.");
	ini.SetBoolValue("DeltaDebugging", dd.approximativeTestExecution_NAME, dd.approximativeTestExecution,
		"\\\\ [For Primary Score Optimization only]\n"
		"\\\\ Generated inputs primary score is approximated by using the monotonity of the primary score function.\n"
		"\\\\ An input for which an extended version exists, must have less than or equal the primary score \n"
		"\\\\ of the shortest extension that exists.");
	ini.SetDoubleValue("DeltaDebugging", dd.approximativeExecutionThreshold_NAME, dd.approximativeExecutionThreshold,
		"\\\\ [For Primary Score Optimization only]\n"
		"\\\\ Generated inputs minimum approximated score must be greater than [Delta Debugged inputs primary score * thisvalue].\n");


	// generation
	ini.SetLongValue("Generation", generation.activeGeneratedInputs_NAME, generation.activeGeneratedInputs, "\\\\ Number of inputs that may be generated at a time. [Applies even if GenerationalMode has been disabled]");
	ini.SetBoolValue("Generation", generation.generationalMode_NAME, generation.generationalMode,
		"\\\\ Inputs are generated in generations of size [GenerationSize].\n"
		"After a generation has been finished additional methods are applied and \n"
		"a new generation is generated based on the prior one.");
	ini.SetLongValue("Generation", generation.generationsize_NAME, generation.generationsize, "\\\\ Maximum generated inputs per generation cycle.");
	ini.SetDoubleValue("Generation", generation.generationtweakstart_NAME, generation.generationtweakstart, "\\\\ Starting parameter for automatic generationsize scaling.");
	ini.SetDoubleValue("Generation", generation.generationtweakmax_NAME, generation.generationtweakmax, "\\\\ Max parameter for automatic generationsize scaling.");

	ini.SetBoolValue("Generation", generation.allowBacktrackFailedInputs_NAME, generation.allowBacktrackFailedInputs,
		"\\\\ Allows backtrackng on failed inputs to generate new inputs");
	ini.SetLongValue("Generation", generation.numberOfInputsToDeltaDebugPerGeneration_NAME, generation.numberOfInputsToDeltaDebugPerGeneration, "\\\\ This defines the number of inputs that are minimized using delta debugging after a generation has been finished.");
	ini.SetLongValue("Generation", generation.numberOfSourcesPerGeneration_NAME, generation.numberOfSourcesPerGeneration, 
		"\\\\ In each generation new inputs are generated from a few sources, which\n"
		"\\\\ stem from prior generations. This setting defines the number of inputs\n"
		"\\\\ that are carries over from one generation into the next as sources.");
	ini.SetLongValue("Generation", generation.sourcesType_NAME, (int32_t)generation.sourcesType,
		"\\\\ 0 - Sources for the next generation are taken from the inputs with the best primary score.\n"
		"\\\\ 1 - Sources for the next generation are taken from the inputs with the best secondary score.\n"
		"\\\\ 2 - Sources for the next generation are taken from the longest inputs.\n");
	ini.SetLongValue("Generation", generation.generationLengthMin_NAME, generation.generationLengthMin, "\\\\ Minimum input length generated in each generation.");
	ini.SetLongValue("Generation", generation.generationLengthMax_NAME, generation.generationLengthMax, "\\\\ Maximum input length generated in each generation.");

	// endconditions
	ini.SetBoolValue("EndConditions", conditions.use_foundnegatives_NAME, conditions.use_foundnegatives, "\\\\ Stop execution after foundnegatives failing inputs have been found.");
	ini.SetLongValue("EndConditions", conditions.foundnegatives_NAME, (long)conditions.foundnegatives, "\\\\ The number of failing inputs to generate.");
	ini.SetBoolValue("EndConditions", conditions.use_foundpositives_NAME, conditions.use_foundpositives, "\\\\ Stop execution after foundpositives positive inputs have been found.");
	ini.SetLongValue("EndConditions", conditions.foundpositives_NAME, (long)conditions.foundpositives, "\\\\ The number of positive inputs to generate.");
	ini.SetBoolValue("EndConditions", conditions.use_timeout_NAME, conditions.use_timeout, "\\\\ Stop execution after a certain time period has passed.");
	ini.SetLongValue("EndConditions", conditions.timeout_NAME, (long)conditions.timeout, "\\\\ The time after which to stop execution. [seconds]");
	ini.SetBoolValue("EndConditions", conditions.use_overalltests_NAME, conditions.use_overalltests, "\\\\ Stop execution after a certain number of tests have been executed.");
	ini.SetLongValue("EndConditions", conditions.overalltests_NAME, (long)conditions.overalltests, "\\\\ The number of overall tests to run");

	// tests
	ini.SetBoolValue("Tests", tests.executeFragments_NAME, tests.executeFragments, "\\\\ Execute tests as fragments.");
	ini.SetBoolValue("Tests", tests.use_testtimeout_NAME, tests.use_testtimeout, "\\\\ Stop test execution after a certain time has passed.");
	ini.SetLongValue("Tests", tests.testtimeout_NAME, (long)tests.testtimeout, "\\\\ Stop test execution after [microseconds] have passed.");
	ini.SetBoolValue("Tests", tests.use_fragmenttimeout_NAME, tests.use_fragmenttimeout, "\\\\ Stop fragment execution after a certain time has passed.");
	ini.SetLongValue("Tests", tests.fragmenttimeout_NAME, (long)tests.fragmenttimeout, "\\\\ Stop fragment execution after [microseconds] have passed.");
	ini.SetBoolValue("Tests", tests.storePUToutput_NAME, tests.storePUToutput,
		"\\\\ Store the output [std::cout, std::cerr] of test runs.\n"
		"\\\\ [Doesn't disable option below if false.]");
	ini.SetBoolValue("Tests", tests.storePUToutputSuccessful_NAME, tests.storePUToutputSuccessful,
		"\\\\ Store the output [std::cout, std::cerr] for successful test runs.\n"
		"\\\\ [Applies even if option above is false.]");
	ini.SetLongValue("Tests", tests.maxUsedMemory_NAME, (long)tests.maxUsedMemory,
		"\\\\ Memory limit for PUT. If PUT goes above this limit it will be killed.\n"
		"\\\\ Set to 0 to disable.");

	ini.SaveFile(path.c_str());
#endif
}

size_t Settings::GetStaticSize(int32_t version)
{
	size_t size0x1 = 4     // version
	                 + 4   // oracle
	                 + 1   // General::usehardwarethreads
	                 + 4   // General::numthreads
	                 + 4   // General::numcomputingthreads
	                 + 4   // General::concurrenttests
	                 + 8   // General::memory_limit
	                 + 8   // General::memory_softlimit
	                 + 8   // General::memory_sweep_period
	                 + 1   // SaveFiles::enablesaves
	                 + 8   // SaveFiles::autosave_every_tests
	                 + 8   // SaveFiles::autosave_every_seconds
	                 + 4   // SaveFiles::compressionLevel
	                 + 1   // SaveFiles::compressionExtreme
	                 + 1   // Optimization::constructinputsiteratively
	                 + 1   // DeltaDebugging::deltadebugging
	                 + 8   // DeltaDebugging::executeAboveLength
	                 + 4   // DeltaDebugging::mode
	                 + 1   // DeltaDebugging::allowScoreOptimization
	                 + 8   // DeltaDebugging::optimizationLossThreshold
	                 + 1   // DeltaDebugging::approximativeTestExecution
	                 + 8   // DeltaDebugging::approximativeExecutionThreshold
	                 + 4   // Generation::generationsize
	                 + 4   // Generation::generationtweakstart
	                 + 4   // Generation::generationtweakmax
	                 + 4   // Generation::activeGeneratedInputs
	                 + 1   // Generation::generationalMode
	                 + 1   // Generation::allowBacktrackFailedInputs
	                 + 4   // Generation::numberOfInputsToDeltaDebugPerGeneration
	                 + 4   // Generation::numberOfSourcesPerGeneration
	                 + 1   // EndConditions::use_foundnegatives
	                 + 8   // EndConditions::foundnegatives
	                 + 1   // EndConditions::use_foundpositives
	                 + 8   // EndConditions::foundpositives
	                 + 1   // EndConditions::use_timeout
	                 + 8   // EndConditions::timeout
	                 + 1   // EndConditions::use_overalltests
	                 + 8   // EndConditions::overalltests
	                 + 1   // Tests::executeFragments
	                 + 1   // Tests::use_testtimeout
	                 + 8   // Tests::testtimeout
	                 + 1   // Tests::use_fragmenttimeout
	                 + 8   // Tests::fragmenttimeout
	                 + 1   // Tests::storePUToutput
	                 + 1   // Tests::storePUToutputSuccessful
	                 + 8;  // Tests::maxUsedMemory
	size_t size0x2 = size0x1  // prior stuff
	                 + 4      // Generation::generationLengthMin
	                 + 4      // Generation::generationLengthMax
	                 + 4      // Generation::sourcesType
	                 + 1;     // Optimization::disableExclusionTree

	switch (version) {
	case 0x1:
		return size0x1;
	case 0x2:
		return size0x2;
	default:
		return 0;
	}
}

size_t Settings::GetDynamicSize()
{
	return Form::GetDynamicSize()                                        // form stuff
	       + GetStaticSize(classversion)                                 // static size
	       + Buffer::CalcStringLength(oracle.oraclepath.string())        // oraclepath
	       + Buffer::CalcStringLength(saves.savepath)                    // General::savepath
	       + Buffer::CalcStringLength(oracle.lua_path_cmd)               // Oracle::lua_path_cmd
	       + Buffer::CalcStringLength(oracle.lua_path_script)            // Oracle::lua_path_script
	       + Buffer::CalcStringLength(oracle.lua_path_cmd_replay)        // Oracle::lua_path_cmd_replay
	       + Buffer::CalcStringLength(oracle.lua_path_oracle)            // Oracle::lua_path_oracle
	       + Buffer::CalcStringLength(oracle.grammar_path)               // Oracle::grammar_path
	       + Buffer::CalcStringLength(oracle.oraclepath_Unix.string());  // Oracle::oraclepath_unix
}

bool Settings::WriteData(std::ostream* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	// oracle
	Buffer::Write((int32_t)oracle.oracle, buffer, offset);
	Buffer::Write(oracle.oraclepath.string(), buffer, offset);
	Buffer::Write(oracle.lua_path_cmd, buffer, offset);
	Buffer::Write(oracle.lua_path_script, buffer, offset);
	Buffer::Write(oracle.lua_path_cmd_replay, buffer, offset);
	Buffer::Write(oracle.lua_path_oracle, buffer, offset);
	Buffer::Write(oracle.grammar_path, buffer, offset);
	// general
	Buffer::Write(general.usehardwarethreads, buffer, offset);
	Buffer::Write(general.numthreads, buffer, offset);
	Buffer::Write(general.numcomputingthreads, buffer, offset);
	Buffer::Write(general.concurrenttests, buffer, offset);
	Buffer::Write(general.memory_limit, buffer, offset);
	Buffer::Write(general.memory_softlimit, buffer, offset);
	Buffer::Write((int64_t)general.memory_sweep_period.count(), buffer, offset);
	// saves
	Buffer::Write(saves.enablesaves, buffer, offset);
	Buffer::Write(saves.autosave_every_tests, buffer, offset);
	Buffer::Write(saves.autosave_every_seconds, buffer, offset);
	Buffer::Write(saves.savepath, buffer, offset);
	Buffer::Write(saves.compressionLevel, buffer, offset);
	Buffer::Write(saves.compressionExtreme, buffer, offset);
	// optimization
	Buffer::Write(optimization.constructinputsiteratively, buffer, offset);
	// deltadebugging
	Buffer::Write(dd.deltadebugging, buffer, offset);
	Buffer::Write(dd.executeAboveLength, buffer, offset);
	Buffer::Write((int32_t)dd.mode, buffer, offset);
	Buffer::Write(dd.allowScoreOptimization, buffer, offset);
	Buffer::Write(dd.optimizationLossThreshold, buffer, offset);
	Buffer::Write(dd.approximativeTestExecution, buffer, offset);
	Buffer::Write(dd.approximativeExecutionThreshold, buffer, offset);
	// generation
	Buffer::Write(generation.generationsize, buffer, offset);
	Buffer::Write(generation.generationtweakstart, buffer, offset);
	Buffer::Write(generation.generationtweakmax, buffer, offset);
	Buffer::Write(generation.activeGeneratedInputs, buffer, offset);
	Buffer::Write(generation.generationalMode, buffer, offset);
	Buffer::Write(generation.allowBacktrackFailedInputs, buffer, offset);
	Buffer::Write(generation.numberOfInputsToDeltaDebugPerGeneration, buffer, offset);
	Buffer::Write(generation.numberOfSourcesPerGeneration, buffer, offset);
	// endconditions
	Buffer::Write(conditions.use_foundnegatives, buffer, offset);
	Buffer::Write(conditions.foundnegatives, buffer, offset);
	Buffer::Write(conditions.use_foundpositives, buffer, offset);
	Buffer::Write(conditions.foundpositives, buffer, offset);
	Buffer::Write(conditions.use_timeout, buffer, offset);
	Buffer::Write(conditions.timeout, buffer, offset);
	Buffer::Write(conditions.use_overalltests, buffer, offset);
	Buffer::Write(conditions.overalltests, buffer, offset);
	// tests
	Buffer::Write(tests.executeFragments, buffer, offset);
	Buffer::Write(tests.use_testtimeout, buffer, offset);
	Buffer::Write(tests.testtimeout, buffer, offset);
	Buffer::Write(tests.use_fragmenttimeout, buffer, offset);
	Buffer::Write(tests.fragmenttimeout, buffer, offset);
	Buffer::Write(tests.storePUToutput, buffer, offset);
	Buffer::Write(tests.storePUToutputSuccessful, buffer, offset);
	Buffer::Write(tests.maxUsedMemory, buffer, offset);

	// VERSION 0x2
	// oracle
	Buffer::Write(oracle.oraclepath_Unix.string(), buffer, offset);
	// generation
	Buffer::Write(generation.generationLengthMin, buffer, offset);
	Buffer::Write(generation.generationLengthMax, buffer, offset);
	Buffer::Write((int32_t)generation.sourcesType, buffer, offset);
	// optimization
	Buffer::Write(optimization.disableExclusionTree, buffer, offset);
	return true;
}

bool Settings::ReadData(std::istream* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	if (_skipread)
		return true;
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
	case 0x2:
		{
			Form::ReadData(buffer, offset, length, resolver);
			// oracle
			oracle.oracle = (::Oracle::PUTType)Buffer::ReadInt32(buffer, offset);
			oracle.oraclepath = std::filesystem::path(Buffer::ReadString(buffer, offset));
			oracle.lua_path_cmd = Buffer::ReadString(buffer, offset);
			oracle.lua_path_script = Buffer::ReadString(buffer, offset);
			oracle.lua_path_cmd_replay = Buffer::ReadString(buffer, offset);
			oracle.lua_path_oracle = Buffer::ReadString(buffer, offset);
			oracle.grammar_path = Buffer::ReadString(buffer, offset);
			// general
			general.usehardwarethreads = Buffer::ReadBool(buffer, offset);
			general.numthreads = Buffer::ReadInt32(buffer, offset);
			general.numcomputingthreads = Buffer::ReadInt32(buffer, offset);
			general.concurrenttests = Buffer::ReadInt32(buffer, offset);
			general.memory_limit = Buffer::ReadInt64(buffer, offset);
			general.memory_softlimit = Buffer::ReadInt64(buffer, offset);
			general.memory_sweep_period = std::chrono::milliseconds(Buffer::ReadInt64(buffer, offset));
			// saves
			saves.enablesaves = Buffer::ReadBool(buffer, offset);
			saves.autosave_every_tests = Buffer::ReadInt64(buffer, offset);
			saves.autosave_every_seconds = Buffer::ReadInt64(buffer, offset);
			saves.savepath = Buffer::ReadString(buffer, offset);
			saves.compressionLevel = Buffer::ReadInt32(buffer, offset);
			saves.compressionExtreme = Buffer::ReadBool(buffer, offset);
			// optimization
			optimization.constructinputsiteratively = Buffer::ReadBool(buffer, offset);
			// delta debugging
			dd.deltadebugging = Buffer::ReadBool(buffer, offset);
			dd.executeAboveLength = Buffer::ReadInt64(buffer, offset);
			dd.mode = (::DeltaDebugging::DDMode)Buffer::ReadInt32(buffer, offset);
			dd.allowScoreOptimization = Buffer::ReadBool(buffer, offset);
			dd.optimizationLossThreshold = Buffer::ReadDouble(buffer, offset);
			dd.approximativeTestExecution = Buffer::ReadBool(buffer, offset);
			dd.approximativeExecutionThreshold = Buffer::ReadDouble(buffer, offset);
			// generation
			generation.generationsize = Buffer::ReadInt32(buffer, offset);
			generation.generationstep = (int32_t)std::lround(std::log(generation.generationsize) / std::log(1.3));
			if (generation.generationstep < 10)
				generation.generationstep = 10;
			generation.generationtweakstart = Buffer::ReadFloat(buffer, offset);
			generation.generationtweakmax = Buffer::ReadFloat(buffer, offset);
			generation.activeGeneratedInputs = Buffer::ReadInt32(buffer, offset);
			generation.generationalMode = Buffer::ReadBool(buffer, offset);
			generation.allowBacktrackFailedInputs = Buffer::ReadBool(buffer, offset);
			generation.numberOfInputsToDeltaDebugPerGeneration = Buffer::ReadInt32(buffer, offset);
			generation.numberOfSourcesPerGeneration = Buffer::ReadInt32(buffer, offset);
			// endconditions
			conditions.use_foundnegatives = Buffer::ReadBool(buffer, offset);
			conditions.foundnegatives = Buffer::ReadUInt64(buffer, offset);
			conditions.use_foundpositives = Buffer::ReadBool(buffer, offset);
			conditions.foundpositives = Buffer::ReadInt64(buffer, offset);
			conditions.use_timeout = Buffer::ReadBool(buffer, offset);
			conditions.timeout = Buffer::ReadUInt64(buffer, offset);
			conditions.use_overalltests = Buffer::ReadBool(buffer, offset);
			conditions.overalltests = Buffer::ReadUInt64(buffer, offset);
			// tests
			tests.executeFragments = Buffer::ReadBool(buffer, offset);
			tests.use_testtimeout = Buffer::ReadBool(buffer, offset);
			tests.testtimeout = Buffer::ReadInt64(buffer, offset);
			tests.use_fragmenttimeout = Buffer::ReadBool(buffer, offset);
			tests.fragmenttimeout = Buffer::ReadInt64(buffer, offset);
			tests.storePUToutput = Buffer::ReadBool(buffer, offset);
			tests.storePUToutputSuccessful = Buffer::ReadBool(buffer, offset);
			tests.maxUsedMemory = Buffer::ReadInt64(buffer, offset);
		}
		if (version == 0x2)
		{
			// oracle
			oracle.oraclepath_Unix = std::filesystem::path(Buffer::ReadString(buffer, offset));
			// generation
			generation.generationLengthMin = Buffer::ReadInt32(buffer, offset);
			generation.generationLengthMax = Buffer::ReadInt32(buffer, offset);
			generation.sourcesType = (GenerationSourcesType)Buffer::ReadInt32(buffer, offset);
			// optimization
			optimization.disableExclusionTree = Buffer::ReadBool(buffer, offset);
		}
		return true;
	default:
		return false;
	}
}

void Settings::Delete(Data*)
{
	Clear();
}

void Settings::Clear()
{

}

void Settings::RegisterFactories()
{
	if (!_registeredFactories) {
		_registeredFactories = !_registeredFactories;
	}
}
