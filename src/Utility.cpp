
#include <vector>
#include <string>

#include "Utility.h"

std::vector<std::string> Utility::SplitString(std::string str, char delimiter, bool removeEmpty, bool escape, char escapesymbol)
{
	std::vector<std::string> splits;
	if (escape) {
		std::string tmp;
		bool escaped = false;
		char last = 0;
		for (char c : str) {
			last = c;
			if (escaped == false) {
				if (c == delimiter) {
					splits.push_back(tmp);
					tmp = "";
				} else if (c == escapesymbol) {
					escaped = !escaped;
					tmp += c;
				} else
					tmp += c;
			} else {
				if (c == escapesymbol)
				{
					escaped = !escaped;
					tmp += c;
				} else
					tmp += c;
			}
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
		if (str.length() != 0)
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

std::vector<std::string> Utility::SplitString(std::string str, const char* delimiter, bool removeEmpty, bool escape, char escapesymbol)
{
	std::vector<std::string> splits;
	if (escape) {
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
	} else {
		size_t pos = str.find(delimiter);
		while (pos != std::string::npos) {
			splits.push_back(str.substr(0, pos));
			str.erase(0, pos + strlen(delimiter));
			pos = str.find(delimiter);
		}
		if (str.length() != 0)
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



std::string& Utility::RemoveWhiteSpaces(std::string& str, char escape, bool removetab)
{
	bool escaped = false;
	auto itr = str.begin();
	while (itr != str.end()) {
		if (*itr == escape)
			escaped = !escaped;
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
