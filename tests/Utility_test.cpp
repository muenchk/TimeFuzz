#include "Logging.h"
#include "Utility.h"

bool TestSplitString(std::string input, std::vector<std::string> output, bool removeempty)
{
	std::string out;
	auto runtest = [&input, &output, &removeempty, &out]() {
		auto result = Utility::SplitString(input, '|', removeempty, true, '\"');
		out += "[";
		for (int i = 0; i < result.size(); i++) {
			if (i == result.size() - 1)
				out += "\"" + result[i] + "\"";
			else
				out += "\"" + result[i] + "\", ";
		}
		out += "]";
		if (result.size() == output.size()) {
			for (int i = 0; i < result.size(); i++) {
				if (result[i] != output[i])
					return false;
			}
		} else
			return false;

		return true;
	};
	bool result = runtest();
	loginfo("TestSplitString:\tInput:\t{}\tRemoveEmpty:\t{}\tOutput:\t{}\tResult:\t{}", input, removeempty, out, result);
	return result;
}

bool TestRemoveWhitespaces(std::string input, std::string output, bool removetab)
{
	std::string in = input;
	auto runtest = [&input, &removetab]() {
		Utility::RemoveWhiteSpaces(input, '\"', removetab);
	};
	runtest();
	loginfo("TestRemoveWhitespaces:\tInput:\t{}\tRemoveTab:\t{}\tOutput:\t{}\tResult:\t{}", in, removetab, input, output == input);
	return output == input;
}

bool TestRemoveSymbols(std::string input, std::string output, char symbol, bool enableescape = false, char escape = '\"')
{
	std::string in = input;
	auto runtest = [&input, &symbol, &enableescape, &escape]() {
		Utility::RemoveSymbols(input, symbol, enableescape, escape);
	};
	runtest();
	loginfo("TestRemoveWhitespaces:\tInput:\t{}\tSymbol:\t{}\tOutput:\t{}\tResult:\t{}", in, symbol, input, output == input);
	return output == input;
}

int main(int argc, char** argv)
{
	Logging::InitializeLog(".");
	loginfo("Starting Utility_test.exe");
	bool res = true;
	res &= TestSplitString("1|2", std::vector<std::string>{ "1", "2" }, true);
	res &= TestSplitString("1|2|", std::vector<std::string>{ "1", "2" }, true);
	res &= TestSplitString("1|2|", std::vector<std::string>{ "1", "2", "" }, false);
	res &= TestSplitString("1|2|\"3|4\"|5", std::vector<std::string>{ "1", "2", "\"3|4\"", "5" }, true);
	res &= TestRemoveWhitespaces("1  2", "12", true);
	res &= TestRemoveWhitespaces("1 \t2", "12", true);
	res &= TestRemoveWhitespaces("1 \t2", "1\t2", false);
	res &= TestRemoveWhitespaces("1 \" \"2", "1\" \"2", true);
	res &= TestRemoveSymbols("1 \t2", "1 2", '\t');
	res &= TestRemoveSymbols("1 \" \"2", "1  2", '\"');
	res &= TestRemoveSymbols("1|2", "12", '|');
	if (res == true)
		return 0;
	return 1;
}
