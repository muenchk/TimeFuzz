#include "Input.h"
#include "Logging.h"
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include "CrashHandler.h"
#endif

int main(int argc, char** argv)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif
	loginfo("Starting Inputs_test.exe");
	auto inputs = Input::ParseInputs("../../FormatExamples/Inputs.py");
	loginfo("Found {} inputs", inputs.size());
	for (auto& input : inputs) {
		loginfo("Input:\t{}", input->ToString());
	}
	if (inputs.size() != 3)
		return 1;
	else
		return 0;
}
