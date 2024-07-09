#include <memory>

#include "Logging.h"
#include "Settings.h"
#include "SimpleIni.h"
#include "Utility.h"

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
	ini.SetLongValue("EndConditions", conditions.foundnegatives_NAME, conditions.foundnegatives, "\\ The number of failing inputs to generate.");

	ini.SaveFile(path.c_str());
}
