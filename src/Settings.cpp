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

void Settings::Load(std::wstring path)
{
	if (initialized)
		return;
	loginfo("Begin loading settings.");
	initialized = true;

	constexpr auto defpath = L"config.ini";
	if (path == L"")
		path = std::wstring(defpath);

	CSimpleIniA ini;

	ini.SetUnicode();

#if defined(unix) || defined(__unix__) || defined(__unix)
	std::ifstream istr(std::filesystem::path(path), std::ios_base::in);
	ini.LoadData(istr);
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	ini.LoadFile(path.c_str());
#endif

	//oracle
	oracle = Oracle::ParseType(ini.GetValue("Oracle", oracle_NAME, "PythonScript"));
	loginfo("{}{} {}", "Oracle:           ", oracle_NAME, Oracle::TypeString(oracle));
	oraclepath = std::filesystem::path(ini.GetValue("Oracle", oraclepath_NAME, ""));
	loginfo("{}{} {}", "Oracle:           ", oraclepath_NAME, oraclepath.string());

	// general
	general.usehardwarethreads = ini.GetBoolValue("General", general.usehardwarethreads_NAME, general.usehardwarethreads);
	loginfo("{}{} {}", "General:          ", general.usehardwarethreads_NAME, general.usehardwarethreads);
	general.numthreads = ini.GetBoolValue("General", general.numthreads_NAME, general.numthreads);
	loginfo("{}{} {}", "General:          ", general.numthreads_NAME, general.numthreads);

	// optimization
	optimization.constructinputsiteratively = ini.GetBoolValue("General", optimization.constructinputsiteratively_NAME, optimization.constructinputsiteratively);
	loginfo("{}{} {}", "Optimization:     ", optimization.constructinputsiteratively_NAME, optimization.constructinputsiteratively);

	// methods
	methods.deltadebugging = ini.GetBoolValue("Methods", methods.deltadebugging_NAME, methods.deltadebugging);
	loginfo("{}{} {}", "Methods:          ", methods.deltadebugging_NAME, methods.deltadebugging);

	// generation
	generation.generationsize = ini.GetBoolValue("Generation", generation.generationsize_NAME, generation.generationsize);
	loginfo("{}{} {}", "Generation:       ", generation.generationsize_NAME, generation.generationsize);
	generation.generationtweakstart = ini.GetBoolValue("Generation", generation.generationtweakstart_NAME, generation.generationtweakstart);
	loginfo("{}{} {}", "Generation:       ", generation.generationtweakstart_NAME, generation.generationtweakstart);
	generation.generationtweakmax = ini.GetBoolValue("Generation", generation.generationtweakmax_NAME, generation.generationtweakmax);
	loginfo("{}{} {}", "Generation:       ", generation.generationtweakmax_NAME, generation.generationtweakmax);

	// endconditions
	conditions.use_maxiterations = ini.GetBoolValue("EndConditions", conditions.use_maxiterations_NAME, conditions.use_maxiterations);
	loginfo("{}{} {}", "EndConditions:    ", conditions.use_maxiterations_NAME, conditions.use_maxiterations);
	conditions.maxiterations = ini.GetBoolValue("EndConditions", conditions.maxiterations_NAME, conditions.maxiterations);
	loginfo("{}{} {}", "EndConditions:    ", conditions.maxiterations_NAME, conditions.maxiterations);
	conditions.use_foundnegatives = ini.GetBoolValue("EndConditions", conditions.use_foundnegatives_NAME, conditions.use_foundnegatives);
	loginfo("{}{} {}", "EndConditions:    ", conditions.use_foundnegatives_NAME, conditions.use_foundnegatives);
	conditions.foundnegatives = ini.GetBoolValue("EndConditions", conditions.foundnegatives_NAME, conditions.foundnegatives);
	loginfo("{}{} {}", "EndConditions:    ", conditions.foundnegatives_NAME, conditions.foundnegatives);

	loginfo("Finished loading settings.");
}

void Settings::Save(std::wstring _path)
{
#if defined(unix) || defined(__unix__) || defined(__unix)
	constexpr auto defpath = "config.ini";
	std::string path;
	if (_path == L"")
		path = std::string(defpath);
	else
		path = Utility::ConvertToString(_path).value();
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	std::wstring path;
	constexpr auto defpath = L"config.ini";
	if (_path == L"")
		path = std::wstring(defpath);
	else
		path = _path;
#endif
		          
	CSimpleIniA ini;

	ini.SetUnicode();

	// oracle
	ini.SetValue("Oracle", oracle_NAME, Oracle::TypeString(oracle).c_str(),
		"\\ Type of oracle used to execute the Program Under Test. \n"
		"\\	\tCMD\t-\tExecutes an unresponsive program. All Inputs are dumped as cmd arguments\n"
		"\\	\tScript\t-\tExecutes a script that itself executes the PUT and returns relevant information on STDOUT\n"
		"\\ \tSTDIN_Responsive\t-\tExecutes a responsive program. One input is given via STDIN at a time, and a response is awaited.\n"
		"\\ \tSTDIN_Dump\t-\tExecutes an unresponsive program. All Inputs are dumped into the standardinput immediately.");
	ini.SetValue("Oracle", oraclepath_NAME, oraclepath.string().c_str(), "\\ The path to the oracle.");

	// general
	ini.SetBoolValue("General", general.usehardwarethreads_NAME, general.usehardwarethreads,
		"\\ Use the number of available hardware threads for the task scheduler.");
	ini.SetLongValue("General", general.numthreads_NAME, general.numthreads,
		"\\ The number of threads to use for the task schedular\n"
		"\\ if UseHardwareThreads is set to false.");

	// optimization
	ini.SetBoolValue("Optimization", optimization.constructinputsiteratively_NAME, optimization.constructinputsiteratively,
		"\\ Input sequences or partial input sequences, that do not result in either a bug or a successful run,\n"
		"\\ may be used as a starting point for generation and may be expanded until a result is produced.\n"
		"\\ [This should not be used with a PUT that produces undefined oracle results.]");

	// methods
	ini.SetBoolValue("Methods", methods.deltadebugging_NAME, methods.deltadebugging, "\\ Applies delta debugging to failing inputs, to reduce them as far as possible.");

	// generation
	ini.SetLongValue("Generation", generation.generationsize_NAME, generation.generationsize, "\\ Maximum generated inputs per generation cycle");
	ini.SetDoubleValue("Generation", generation.generationtweakstart_NAME, generation.generationtweakstart, "\\ Starting parameter for automatic generationsize scaling");
	ini.SetDoubleValue("Generation", generation.generationtweakmax_NAME, generation.generationtweakmax, "\\ Max parameter for automatic generationsize scaling");

	// endconditions
	ini.SetBoolValue("EndConditions", conditions.use_maxiterations_NAME, conditions.use_maxiterations, "\\ Stop execution after MaxIterations.");
	ini.SetLongValue("EndConditions", conditions.maxiterations_NAME, conditions.maxiterations, "\\ The maximum number of iterations to run.");
	ini.SetBoolValue("EndConditions", conditions.use_foundnegatives_NAME, conditions.use_foundnegatives, "\\ Stop execution after foundnegatives failing inputs have been found");
	ini.SetLongValue("EndConditions", conditions.foundnegatives_NAME, (long)conditions.foundnegatives, "\\ The number of failing inputs to generate.");

	ini.SaveFile(path.c_str());
}

size_t Settings::GetStaticSize(int32_t version)
{
	static size_t size0x1 = Form::GetDynamicSize()  // form base size
	                        + 4                     // version
	                        + 4                     // oracle
	                        + 1                     // General::usehardwarethreads
	                        + 4                     // General::numthreads
	                        + 4                     // General::numcomputingthreads
	                        + 4                     // General::concurrenttests
	                        + 1                     // General::enablesaves
	                        + 8                     // General::autosave_every_tests
	                        + 8                     // General::autosave_every_seconds
	                        + 1                     // Optimization::constructinputsiteratively
	                        + 1                     // Methods::deltadebugging
	                        + 4                     // Generation::generationsize
	                        + 4                     // Generation::generationtweakstart
	                        + 4                     // Generation::generationtweakmax
	                        + 1                     // EndConditions::use_maxiterations
	                        + 4                     // EndConditions::maxiterations
	                        + 1                     // EndConditions::use_foundnegatives
	                        + 8                     // EndConditions::foundnegatives
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

	switch (version) {
	case 0x1:
		return size0x1;
	default:
		return 0;
	}
}

size_t Settings::GetDynamicSize()
{
	return GetStaticSize()                                  // static size
	       + Buffer::CalcStringLength(oraclepath.string())  // oraclepath
	       + Buffer::CalcStringLength(general.savepath);    // General::savepath
}

bool Settings::WriteData(unsigned char* buffer, size_t& offset)
{
	Buffer::Write(classversion, buffer, offset);
	Form::WriteData(buffer, offset);
	Buffer::Write((int32_t)oracle, buffer, offset);
	// general
	Buffer::Write(oraclepath.string(), buffer, offset);
	Buffer::Write(general.usehardwarethreads, buffer, offset);
	Buffer::Write(general.numthreads, buffer, offset);
	Buffer::Write(general.numcomputingthreads, buffer, offset);
	Buffer::Write(general.concurrenttests, buffer, offset);
	Buffer::Write(general.enablesaves, buffer, offset);
	Buffer::Write(general.autosave_every_tests, buffer, offset);
	Buffer::Write(general.autosave_every_seconds, buffer, offset);
	Buffer::Write(general.savepath, buffer, offset);
	// optimization
	Buffer::Write(optimization.constructinputsiteratively, buffer, offset);
	// methods
	Buffer::Write(methods.deltadebugging, buffer, offset);
	// generation
	Buffer::Write(generation.generationsize, buffer, offset);
	Buffer::Write(generation.generationtweakstart, buffer, offset);
	Buffer::Write(generation.generationtweakmax, buffer, offset);
	// endconditions
	Buffer::Write(conditions.use_maxiterations, buffer, offset);
	Buffer::Write(conditions.maxiterations, buffer, offset);
	Buffer::Write(conditions.use_foundnegatives, buffer, offset);
	Buffer::Write(conditions.foundnegatives, buffer, offset);
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
	int32_t version = Buffer::ReadInt32(buffer, offset);
	switch (version) {
	case 0x1:
		{
			Form::ReadData(buffer, offset, length, resolver);
			oracle = (Oracle::PUTType)Buffer::ReadInt32(buffer, offset);
			oraclepath = std::filesystem::path(Buffer::ReadString(buffer, offset));
			// general
			general.usehardwarethreads = Buffer::ReadBool(buffer, offset);
			general.numthreads = Buffer::ReadInt32(buffer, offset);
			general.numcomputingthreads = Buffer::ReadInt32(buffer, offset);
			general.concurrenttests = Buffer::ReadInt32(buffer, offset);
			general.enablesaves = Buffer::ReadBool(buffer, offset);
			general.autosave_every_tests = Buffer::ReadInt64(buffer, offset);
			general.autosave_every_seconds = Buffer::ReadInt64(buffer, offset);
			general.savepath = Buffer::ReadString(buffer, offset);
			// optimization
			optimization.constructinputsiteratively = Buffer::ReadBool(buffer, offset);
			// methods
			methods.deltadebugging = Buffer::ReadBool(buffer, offset);
			// generation
			generation.generationsize = Buffer::ReadInt32(buffer, offset);
			generation.generationtweakstart = Buffer::ReadFloat(buffer, offset);
			generation.generationtweakmax = Buffer::ReadFloat(buffer, offset);
			// endconditions
			conditions.use_maxiterations = Buffer::ReadBool(buffer, offset);
			conditions.maxiterations = Buffer::ReadInt32(buffer, offset);
			conditions.use_foundnegatives = Buffer::ReadBool(buffer, offset);
			conditions.foundnegatives = Buffer::ReadInt64(buffer, offset);
			conditions.use_timeout = Buffer::ReadBool(buffer, offset);
			conditions.timeout = Buffer::ReadInt64(buffer, offset);
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

}
