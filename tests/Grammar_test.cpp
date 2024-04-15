#include "Logging.h"
#include "Grammar.h"
#include "CrashHandler/CrashHandler.h"

int main(int argc, char** argv)
{
	Logging::InitializeLog(".");
	Crash::Install(".");

	std::string compare = Utility::ReadFile("../../FormatExamples/grammar2.scala");
	bool result = true;

	std::shared_ptr<Grammar> grammar = std::make_shared<Grammar>();
	grammar->ParseScala("../../FormatExamples/grammar1.scala");
	std::string scala = grammar->Scala();
	logdebug("Full Grammar:\n{}", scala);
	result &= Utility::RemoveSymbols(scala, '\n').compare(compare) == 0;
	grammar.reset();

	grammar = std::make_shared<Grammar>();
	grammar->ParseScala("../../FormatExamples/grammar2.scala");
	scala = grammar->Scala();
	logdebug("Full Grammar:\n{}", scala);
	result &= Utility::RemoveSymbols(scala, '\n').compare(compare) == 0;
	grammar.reset();

	if (result)
		return 0;
	else
		return 1;
}
