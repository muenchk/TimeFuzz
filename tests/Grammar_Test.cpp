#include "Logging.h"
#include "Grammar.h"
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#include "CrashHandler.h"
#endif

int main(/*int argc, char** argv*/)
{
	Logging::InitializeLog(".");
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	Crash::Install(".");
#endif

	std::string compare = Utility::ReadFile("../../FormatExamples/grammar2.scala");
	bool result = true;

	std::shared_ptr<Grammar> grammar = std::make_shared<Grammar>();
	grammar->ParseScala("../../FormatExamples/grammar1.scala");
	std::string scala = grammar->Scala();
	logdebug("Full Grammar:\n{}", scala);
	result &= Utility::RemoveSymbols(Utility::RemoveSymbols(scala, '\n', true, '\"'), '\r', true, '\"').compare(Utility::RemoveSymbols(Utility::RemoveSymbols(compare, '\n', true, '\"'), '\r', true, '\"')) == 0;
	grammar.reset();

	grammar = std::make_shared<Grammar>();
	grammar->ParseScala("../../FormatExamples/grammar2.scala");
	scala = grammar->Scala();
	logdebug("Full Grammar:\n{}", scala);
	result &= Utility::RemoveSymbols(Utility::RemoveSymbols(scala, '\n', true, '\"'), '\r', true, '\"').compare(Utility::RemoveSymbols(Utility::RemoveSymbols(compare, '\n', true, '\"'), '\r', true, '\"')) == 0;
	grammar.reset();

	if (result)
		return 0;
	else
		return 1;
}
