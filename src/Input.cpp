
#include <filesystem>
#include <boost/container_hash/hash.hpp>

#include "Input.h"
#include "Logging.h"

Input::Input()
{

}

size_t Input::Length()
{
	return sequence.size();
}

std::string& Input::operator[](size_t index)
{
	auto itr = begin();
	size_t pos = 0;
	while (itr != end())
	{
		if (pos == index)
			return *itr;
		pos++;
		itr++;
	}
	throw std::out_of_range("");
}

void Input::AddEntry(std::string entry)
{
	sequence.push_back(entry);
}

std::string Input::ToString()
{
	std::string str = "[ ";
	int count = 0;
	for (auto& s : sequence)
	{
		count++;
		if (count == sequence.size())
			str += "\"" + s + "\"";
		else
			str += "\"" + s + "\", ";
	}
	str += " ]";
	return str;
}

std::list<std::string>::iterator Input::begin()
{
	return sequence.begin();
}

std::list<std::string>::iterator Input::end()
{
	return sequence.end();
}

std::size_t Input::Hash()
{
	return boost::hash_range(sequence.begin(), sequence.end());
}

std::vector<std::shared_ptr<Input>> Input::ParseInputs(std::filesystem::path path)
{
	// check whether the path exists and whether its actually a file
	if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
		logwarn("Cannot read inputs from file: {}. Either the path does not exist or refers to a directory.", path.string());
		return {};
	}
	// open filestream
	std::ifstream _stream;
	try {
		_stream = std::ifstream(path, std::ios_base::in);
	} catch (std::exception& e) {
		logwarn("Cannot read inputs from file: {}. The file cannot be accessed with error message: {}", path.string(), e.what());
		return {};
	}
	if (_stream.is_open()) {
		/* auto CountSymbols = [](std::string str, char symbol, char escaped1, char escaped2) {
			int count = 0;
			bool escape = false;
			for (char& c : str) {
				// escaped strings are marked by, e.g., " or ' and everything between two of them should not be counted
				// we need to check for both since one string could be marked with ", while another one could use '
				// this means that something thats invalid in python "usdzfgf' is counted as valid.
				if (c == escaped1 || c == escaped2)
					escape = !escape;
				if (escape == false)
					if (c == symbol)
						count++;
			}
			return count;
		};*/

		// parse file for a variable named inputs
		std::string contents;
		int open = 0;
		int closed = 0;
		std::string line;
		while (std::getline(_stream, line)) {
			// we read another line
			// check if its empty or with a comment
			if (line.empty())
				continue;
			// remove leading spaces and tabs
			while (line.length() > 0 && (line[0] == ' ' || line[0] == '\t')) {
				line = line.substr(1, line.length() - 1);
			}
			// check again
			if (line.length() == 0)
				continue;
			// now check whether the beginning of the line matches "inputs = ["
			if (auto pos = line.find("inputs = ["); pos != std::string::npos && pos == 0) {
				contents += line.substr(pos + 8, line.size() - (pos + 8));
				// count the number of opening and closing brackets, and add lines until it matches
				open += Utility::CountSymbols(line, '[', '\'', '\"');
				closed += Utility::CountSymbols(line, ']', '\'', '\"');
				while (open != closed && std::getline(_stream, line)) {
					contents += line;
					open += Utility::CountSymbols(line, '[', '\'', '\"');
					closed += Utility::CountSymbols(line, ']', '\'', '\"');
				}
				if (open != closed) {
					logwarn("Cannot read inputs from file: {}. Syntax error.", path.string());
					return {};
				} else {
					// open == closed
					// we have found our complete inuput, now onto parse it and extract the input sequences
					std::vector<std::shared_ptr<Input>> inputs;
					std::shared_ptr<Input> input(nullptr);

					struct record
					{
						int open = 0;
						bool escaped = false;
						std::string current;
						bool brk = false;
					};
					record rec;
					std::string worked = "";

					for (char c : contents) {
						worked += c;
						if (rec.open == 0) {
							switch (c) {
							case '[':
								rec.open++;
								break;
							}
						} else if (rec.open == 1) {
							switch (c) {
							case '[':
								// we are entering an input definition
								rec.open++;
								input = std::make_shared<Input>();
								break;
							case ']':
								// we are done with all inputs
								rec.open--;
								rec.brk = true;
								break;
							}
						} else if (rec.open == 2) {
							// we are currently parsing an input entry
							if (rec.escaped == true) {
								switch (c) {
								case '\'':  // fallthrough intended
								case '\"':
									// escaped sequence has ended. Add current as entry to the input and reset current
									rec.escaped = false;
									input->AddEntry(rec.current);
									rec.current = "";
									break;
								default:
									// add c to current
									rec.current += c;
									break;
								}
							} else {
								switch (c) {
								case '[':
									logwarn("Cannot read inputs from file: {}. Syntax Error: 3-dimensional arrays are not supported.", path.string());
									return {};
								case ']':
									// closing one input
									inputs.push_back(input);
									input.reset();
									rec.open--;
									break;
								case '\'':  // fallthrough intended
								case '\"':
									// escaped sequence has begun
									rec.escaped = true;
									break;
								}
							}
						}
						if (rec.brk == true)
							break;
					}
					if (rec.open != 0 || rec.escaped == true) {
						logwarn("Cannot read inputs from file: {}. Encountered Syntax error while parsing inputs.", path.string());
						return {};
					}
					// successfully parsed the file
					// return all found inputs
					return inputs;
				}
			}
		}
		logwarn("Cannot read inputs from file: {}. Could not find the definition of inputs.", path.string());
		return {};

	} else {
		logwarn("Cannot read inputs from file: {}. Could not open file.", path.string());
		return {};
	}
}
