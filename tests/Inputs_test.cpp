#include "Input.h"
#include "Logging.h"
#include "CrashHandler.h"

int main(int argc, char** argv)
{
	Logging::InitializeLog(".");
	Crash::Install(".");
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
