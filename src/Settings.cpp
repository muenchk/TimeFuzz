#include <memory>

#include "Logging.h"
#include "Settings.h"
#include "SimpleIni.h"
#include "Utility.h"
#include "BufferOperations.h"

Settings* Settings::GetSingleton()
{
	static Settings session;
	return std::addressof(session);
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
	oracle.lua_path_cmd = std::string(ini.GetValue("Oracle", oracle.lua_path_cmd_NAME, oracle.lua_path_cmd.c_str()));
	loginfo("{}{} {}", "Oracle:           ", oracle.lua_path_cmd_NAME, oracle.lua_path_cmd);
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

	// saves
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

	// methods
	methods.deltadebugging = ini.GetBoolValue("Methods", methods.deltadebugging_NAME, methods.deltadebugging);
	loginfo("{}{} {}", "Methods:          ", methods.deltadebugging_NAME, methods.deltadebugging);

	// generation
	generation.generationsize = (int32_t)ini.GetLongValue("Generation", generation.generationsize_NAME, generation.generationsize);
	loginfo("{}{} {}", "Generation:       ", generation.generationsize_NAME, generation.generationsize);
	generation.generationtweakstart = (float)ini.GetDoubleValue("Generation", generation.generationtweakstart_NAME, generation.generationtweakstart);
	loginfo("{}{} {}", "Generation:       ", generation.generationtweakstart_NAME, generation.generationtweakstart);
	generation.generationtweakmax = (float)ini.GetDoubleValue("Generation", generation.generationtweakmax_NAME, generation.generationtweakmax);
	loginfo("{}{} {}", "Generation:       ", generation.generationtweakmax_NAME, generation.generationtweakmax);

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
	ini.SetValue("Oracle", oracle.lua_path_cmd_NAME, oracle.lua_path_cmd.c_str(), "\\\\ The lua script containing the cmdargs function.");
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

	// methods
	ini.SetBoolValue("Methods", methods.deltadebugging_NAME, methods.deltadebugging, "\\\\ Applies delta debugging to failing inputs, to reduce them as far as possible.");

	// generation
	ini.SetLongValue("Generation", generation.generationsize_NAME, generation.generationsize, "\\\\ Maximum generated inputs per generation cycle.");
	ini.SetDoubleValue("Generation", generation.generationtweakstart_NAME, generation.generationtweakstart, "\\\\ Starting parameter for automatic generationsize scaling.");
	ini.SetDoubleValue("Generation", generation.generationtweakmax_NAME, generation.generationtweakmax, "\\\\ Max parameter for automatic generationsize scaling.");

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
	size_t size0x1 = Form::GetDynamicSize()  // form base size
	                 + 4                     // version
	                 + 4                     // oracle
	                 + 1                     // General::usehardwarethreads
	                 + 4                     // General::numthreads
	                 + 4                     // General::numcomputingthreads
	                 + 4                     // General::concurrenttests
	                 + 1                     // SaveFiles::enablesaves
	                 + 8                     // SaveFiles::autosave_every_tests
	                 + 8                     // SaveFiles::autosave_every_seconds
	                 + 4                     // SaveFiles::compressionLevel
	                 + 1                     // SaveFiles::compressionExtreme
	                 + 1                     // Optimization::constructinputsiteratively
	                 + 1                     // Methods::deltadebugging
	                 + 4                     // Generation::generationsize
	                 + 4                     // Generation::generationtweakstart
	                 + 4                     // Generation::generationtweakmax
	                 + 1                     // EndConditions::use_foundnegatives
	                 + 8                     // EndConditions::foundnegatives
	                 + 1                     // EndConditions::use_foundpositives
	                 + 8                     // EndConditions::foundpositives
	                 + 1                     // EndConditions::use_timeout
	                 + 8                     // EndConditions::timeout
	                 + 1                     // EndConditions::use_overalltests
	                 + 8                     // EndConditions::overalltests
	                 + 1                     // Tests::executeFragments
	                 + 1                     // Tests::use_testtimeout
	                 + 8                     // Tests::testtimeout
	                 + 1                     // Tests::use_fragmenttimeout
	                 + 8                     // Tests::fragmenttimeout
	                 + 1                     // Tests::storePUToutput
	                 + 1                     // Tests::storePUToutputSuccessful
	                 + 8;                    // Tests::maxUsedMemory
	size_t size0x2 = size0x1;                // prior version size

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
	return GetStaticSize(classversion)                             // static size
	       + Buffer::CalcStringLength(oracle.oraclepath.string())  // oraclepath
	       + Buffer::CalcStringLength(saves.savepath)              // General::savepath
	       + Buffer::CalcStringLength(oracle.lua_path_cmd)         // Oracle::lua_path_cmd
	       + Buffer::CalcStringLength(oracle.lua_path_cmd_replay)  // Oracle::lua_path_cmd_replay
	       + Buffer::CalcStringLength(oracle.lua_path_oracle)      // Oracle::lua_path_oracle
	       + Buffer::CalcStringLength(oracle.grammar_path);     // Oracle::grammar_path
}

bool Settings::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	// oracle
	Buffer::Write((int32_t)oracle.oracle, buffer, offset);
	Buffer::Write(oracle.oraclepath.string(), buffer, offset);
	Buffer::Write(oracle.lua_path_cmd, buffer, offset);
	Buffer::Write(oracle.lua_path_cmd_replay, buffer, offset);
	Buffer::Write(oracle.lua_path_oracle, buffer, offset);
	Buffer::Write(oracle.grammar_path, buffer, offset);
	// general
	Buffer::Write(general.usehardwarethreads, buffer, offset);
	Buffer::Write(general.numthreads, buffer, offset);
	Buffer::Write(general.numcomputingthreads, buffer, offset);
	Buffer::Write(general.concurrenttests, buffer, offset);
	// saves
	Buffer::Write(saves.enablesaves, buffer, offset);
	Buffer::Write(saves.autosave_every_tests, buffer, offset);
	Buffer::Write(saves.autosave_every_seconds, buffer, offset);
	Buffer::Write(saves.savepath, buffer, offset);
	Buffer::Write(saves.compressionLevel, buffer, offset);
	Buffer::Write(saves.compressionExtreme, buffer, offset);
	// optimization
	Buffer::Write(optimization.constructinputsiteratively, buffer, offset);
	// methods
	Buffer::Write(methods.deltadebugging, buffer, offset);
	// generation
	Buffer::Write(generation.generationsize, buffer, offset);
	Buffer::Write(generation.generationtweakstart, buffer, offset);
	Buffer::Write(generation.generationtweakmax, buffer, offset);
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
	return true;
}

bool Settings::ReadData(unsigned char* buffer, size_t& offset, size_t length, LoadResolver* resolver)
{
	if (_skipread)
		return true;
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			// oracle
			oracle.oracle = (::Oracle::PUTType)Buffer::ReadInt32(buffer, offset);
			oracle.oraclepath = std::filesystem::path(Buffer::ReadString(buffer, offset));
			// general
			general.usehardwarethreads = Buffer::ReadBool(buffer, offset);
			general.numthreads = Buffer::ReadInt32(buffer, offset);
			general.numcomputingthreads = Buffer::ReadInt32(buffer, offset);
			general.concurrenttests = Buffer::ReadInt32(buffer, offset);
			// saves
			saves.enablesaves = Buffer::ReadBool(buffer, offset);
			saves.autosave_every_tests = Buffer::ReadInt64(buffer, offset);
			saves.autosave_every_seconds = Buffer::ReadInt64(buffer, offset);
			saves.savepath = Buffer::ReadString(buffer, offset);
			saves.compressionLevel = Buffer::ReadInt32(buffer, offset);
			saves.compressionExtreme = Buffer::ReadBool(buffer, offset);
			// optimization
			optimization.constructinputsiteratively = Buffer::ReadBool(buffer, offset);
			// methods
			methods.deltadebugging = Buffer::ReadBool(buffer, offset);
			// generation
			generation.generationsize = Buffer::ReadInt32(buffer, offset);
			generation.generationtweakstart = Buffer::ReadFloat(buffer, offset);
			generation.generationtweakmax = Buffer::ReadFloat(buffer, offset);
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
			return true;
		}
		break;
	case 0x2:
		{
			Form::ReadData(buffer, offset, length, resolver);
			// oracle
			oracle.oracle = (::Oracle::PUTType)Buffer::ReadInt32(buffer, offset);
			oracle.oraclepath = std::filesystem::path(Buffer::ReadString(buffer, offset));
			oracle.lua_path_cmd = Buffer::ReadString(buffer, offset);
			oracle.lua_path_cmd_replay = Buffer::ReadString(buffer, offset);
			oracle.lua_path_oracle = Buffer::ReadString(buffer, offset);
			oracle.grammar_path = Buffer::ReadString(buffer, offset);
			// general
			general.usehardwarethreads = Buffer::ReadBool(buffer, offset);
			general.numthreads = Buffer::ReadInt32(buffer, offset);
			general.numcomputingthreads = Buffer::ReadInt32(buffer, offset);
			general.concurrenttests = Buffer::ReadInt32(buffer, offset);
			// saves
			saves.enablesaves = Buffer::ReadBool(buffer, offset);
			saves.autosave_every_tests = Buffer::ReadInt64(buffer, offset);
			saves.autosave_every_seconds = Buffer::ReadInt64(buffer, offset);
			saves.savepath = Buffer::ReadString(buffer, offset);
			saves.compressionLevel = Buffer::ReadInt32(buffer, offset);
			saves.compressionExtreme = Buffer::ReadBool(buffer, offset);
			// optimization
			optimization.constructinputsiteratively = Buffer::ReadBool(buffer, offset);
			// methods
			methods.deltadebugging = Buffer::ReadBool(buffer, offset);
			// generation
			generation.generationsize = Buffer::ReadInt32(buffer, offset);
			generation.generationtweakstart = Buffer::ReadFloat(buffer, offset);
			generation.generationtweakmax = Buffer::ReadFloat(buffer, offset);
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
			return true;
		}
		break;
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
