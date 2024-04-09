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

void Settings::Load()
{
	constexpr auto path = L"config.ini";

	CSimpleIniA ini;

	ini.SetUnicode();
	ini.LoadFile(path);

	oracle = Oracle::ParseType(ini.GetValue("Oracle", "OracleType", "PythonScript"));
	loginfo("{} {}", "Oracle:           OracleType", Oracle::TypeString(oracle));

	oraclepath = std::filesystem::path(ini.GetValue("Oracle", "Path", ""));
	loginfo("{} {}", "Oracle:           Path", Oracle::TypeString(oraclepath));
}

void Settings::Save()
{
	constexpr auto path = L"config.ini";

	CSimpleIniA ini;

	ini.SetUnicode();

	// oracle
	ini.SetValue("Oracle", "OracleType", Oracle::TypeString(oracle).c_str(),
		"\\ Type of oracle used to execute the Program Under Test. \n"
		"\\	\tPythonScript\t-\tExecutes a pythonscript that itself executes the PUT\n"
		"\\ \tcmdResponsive\t-\tExecutes a responsive cmd program. One input is given at a time, and a response is awaited.\n"
		"\\ \tcmdDump\t-\tExecutes an unresponsive cmd program. All Inputs are dumped into the standardinput immediately.");
	ini.SetValue("Oracle", "Path", oraclepath.string().c_str(), "\\ The path to the oracle.");

	// general
	ini.SetBoolValue("General", "UseHardwareThreads", general.usehardwarethreads,
		"\\ Use the number of available hardware threads for the task scheduler.");
	ini.SetLongValue("General", "NumThreads", general.numthreads,
		"\\ The number of threads to use for the task schedular\n"
		"\\ if UseHardwareThreads is set to false.");

	// optimization
	ini.SetBoolValue("Optimization", "ConstructInputsIteratively", optimization.constructinputsitertively,
		"\\ Input sequences or partial input sequences, that do not result in either a bug or a successful run,\n"
		"\\ may be used as a starting point for generation and may be expanded until a result is produced.\n"
		"\\ [This should not be used with a PUT that produces undefined oracle results.]");

	// methods

	// generation

	// endconditions


	ini.SaveFile(path);
}
