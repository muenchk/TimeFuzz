
#include <vector>
#include <string>

#include "Logging.h"
#include "Utility.h"

std::vector<std::string> Utility::SplitString(std::string str, char delimiter, bool removeEmpty, bool escape, char escapesymbol, bool allowdisableescape, char disablechar)
{
	std::vector<std::string> splits;
	if (escape) {
		std::string tmp;
		bool escaped = false;
		char last = 0;
		for (char c : str) {
			if (escaped == false) {
				if (c == delimiter) {
					splits.push_back(tmp);
					tmp = "";
				} else if (c == escapesymbol && (allowdisableescape == false || allowdisableescape && last != disablechar)) {
					escaped = !escaped;
					tmp += c;
				} else
					tmp += c;
			} else {
				if (c == escapesymbol && (allowdisableescape == false || allowdisableescape && last != disablechar))
				{
					escaped = !escaped;
					tmp += c;
				} else
					tmp += c;
			}
			last = c;
		}
		if (tmp.empty() == false || last == delimiter && escaped == false)
			splits.push_back(tmp);
	} else {
		size_t pos = str.find(delimiter);
		while (pos != std::string::npos) {
			splits.push_back(str.substr(0, pos));
			str.erase(0, pos + 1);
			pos = str.find(delimiter);
		}
		splits.push_back(str);
	}
	if (removeEmpty) {
		auto itr = splits.begin();
		while (itr != splits.end()) {
			if (*itr == "") {
				itr = splits.erase(itr);
				continue;
			}
			itr++;
		}
	}
	return splits;
}

struct window
{
	std::string sliding;

	int size = 0;
	int maxsize = 0;

	char AddChar(char c)
	{
		char ret = 0;
		if (size == maxsize)
		{
			ret = sliding[0];
			sliding = sliding.substr(1, sliding.length() - 1);
			sliding += c;
		}
		else
		{
			sliding += c;
			size++;
		}
		return ret;
	}
	void Reset()
	{
		sliding = "";
		size = 0;
	}
	std::string GetWindow()
	{
		return sliding;
	}
	int Compare(std::string &other)
	{
		return sliding.compare(other);
	}
};

std::vector<std::string> Utility::SplitString(std::string str, std::string delimiter, bool removeEmpty, bool escape, char escapesymbol)
{
	std::vector<std::string> splits;
	if (escape) {
		bool escaped = false;
		std::string tmp;
		window slide;
		slide.maxsize = (int)delimiter.size();
		for (char c : str) {
			//logdebug("Char: {}\t Window: {}\t Wsize: {}\t Wmaxsize: {}\t tmp: {}", c, slide.GetWindow(), slide.size, slide.maxsize, tmp);
			if (escape == true && escaped == true) {
				// escaped sequence.
				// just add char to string since we don't use window during escaped sequences
				tmp += c;
				if (c == escapesymbol) {
					escaped = !escaped;
				}
			} else {
				char x = slide.AddChar(c);
				if (x != 0)
					tmp += x;
				if (escape == true && c == escapesymbol) {
					// we are at the beginning of an escaped sequence
					escaped = !escaped;
					// add window to string since we don't use window during escaped sequence
					tmp += slide.GetWindow();
					slide.Reset();
				} else {
					if (slide.Compare(delimiter) == 0) {
						// delimiter matches sliding window
						splits.push_back(tmp);
						tmp = "";
						slide.Reset();
					}
				}
			}
		}
		// assemble last complete string part
		tmp += slide.GetWindow();
		// add as last split
		splits.push_back(tmp);
	}

	/* if (escape) {
		std::string tmp;
		bool escaped = false;
		char llast = 0;
		char last = 0;
		for (char c : str) {
			llast = last;
			last = c;
			if (escaped == false) {
				if ((std::string("") + last) + c == delimiter) {
					splits.push_back(tmp.substr(0, tmp.size() - 2));
					tmp = "";
					last = 0;
				} else if (c == escapesymbol) {
					escaped = !escaped;
					tmp += c;
				} else
					tmp += c;
			} else {
				if (c == escapesymbol) {
					escaped = !escaped;
					tmp += c;
				} else
					tmp += c;
			}
		}
		if (tmp.empty() == false || (std::string("") + llast) + last == delimiter && escaped == false)
			splits.push_back(tmp);
	} */
	else {
		size_t pos = str.find(delimiter);
		while (pos != std::string::npos) {
			splits.push_back(str.substr(0, pos));
			str.erase(0, pos + delimiter.length());
			pos = str.find(delimiter);
		}
		splits.push_back(str);
	}
	if (removeEmpty) {
		auto itr = splits.begin();
		while (itr != splits.end()) {
			if (*itr == "") {
				itr = splits.erase(itr);
				continue;
			}
			itr++;
		}
	}
	return splits;
}



std::string& Utility::RemoveWhiteSpaces(std::string& str, char escape, bool removetab, bool allowdisableescape, char disablechar)
{
	bool escaped = false;
	auto itr = str.begin();
	char last = 0;
	while (itr != str.end()) {
		if (*itr == escape && (allowdisableescape == false || allowdisableescape && last != disablechar))
			escaped = !escaped;
		last = *itr;
		if (!escaped) {
			char c = *itr;
			if (*itr == ' ' || (removetab && *itr == '\t')) {
				itr = str.erase(itr);
				continue;
			}
		}
		itr++;
	}
	return str;
}

std::string& Utility::RemoveSymbols(std::string& str, char symbol, bool enableescape, char escape)
{
	bool escaped = false;
	auto itr = str.begin();
	while (itr != str.end()) {
		if (enableescape && *itr == escape)
			escaped = !escaped;
		if (!escaped) {
			char c = *itr;
			if (*itr == symbol) {
				itr = str.erase(itr);
				continue;
			}
		}
		itr++;
	}
	return str;
}

std::string& Utility::RemoveSymbols(std::string& str, char symbol, char disablechar)
{
	auto itr = str.begin();
	char last = 0;
	while (itr != str.end()) {
		if (*itr == symbol && last != disablechar) {
			itr = str.erase(itr);
			continue;
		}
		last = *itr;
		itr++;
	}
	return str;
}

std::string Utility::ReadFile(std::filesystem::path path)
{
	if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
		logwarn("Cannot read the file: {}. Either the path does not exist or refers to a directory.", path.string());
		return "";
	}
	std::ifstream _stream;
	try {
		_stream = std::ifstream(path, std::ios_base::in);
	} catch (std::exception& e) {
		logwarn("Cannot read the file: {}. The file cannot be accessed with error message: {}", path.string(), e.what());
		return "";
	}
	logdebug("filestream");
	if (_stream.is_open()) {
		std::string all;
		std::string line;
		while (std::getline(_stream, line))
			all += line;
		return all;
	} else {
		logwarn("Cannot read the file: {}. The filestream does not open.", path.string());
	}
	return "";
}
