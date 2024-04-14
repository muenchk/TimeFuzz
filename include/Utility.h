#pragma once

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <Windows.h>

typedef uint64_t EnumType;

class Utility
{
public:
	/// <summary>
	/// Converts all symbols in a string into lower case
	/// </summary>
	/// <param name="s"></param>
	/// <returns></returns>
	static std::string ToLower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); }
		);
		return s;
	}

	static std::string executable_name()
	{
#if defined(PLATFORM_POSIX) || defined(__linux__)  //check defines for your setup

		std::string sp;
		std::ifstream("/proc/self/comm") >> sp;
		return sp;

#elif defined(_WIN32)

		char buf[MAX_PATH];
		GetModuleFileNameA(nullptr, buf, MAX_PATH);
		return buf;

#else

		static_assert(false, "unrecognized platform");

#endif
	}

	/// <summary>
	/// Returns a string showing [val] as Hexadecimal number
	/// </summary>
	/// <param name="val"></param>
	/// <returns></returns>
	static std::string GetHex(long val)
	{
		std::stringstream ss;
		ss << std::hex << val;
		return ss.str();
	}
	/// <summary>
	/// Returns a string showing [val] as Hexadecimal number
	/// </summary>
	/// <param name="val"></param>
	/// <returns></returns>
	static std::string GetHex(uint64_t val)
	{
		std::stringstream ss;
		ss << std::hex << val;
		return ss.str();
	}
	/// <summary>
	/// Returns a string showing [val] as Hexadecimal number
	/// </summary>
	/// <param name="val"></param>
	/// <returns></returns>
	static std::string GetHex(uint32_t val)
	{
		std::stringstream ss;
		ss << std::hex << val;
		return ss.str();
	}
	/// <summary>
	/// Returns a string showing [val] as Hexadecimal number
	/// </summary>
	/// <param name="val"></param>
	/// <returns></returns>
	static std::string GetHex(int val)
	{
		std::stringstream ss;
		ss << std::hex << val;
		return ss.str();
	}
	/// <summary>
	/// Returns a string showing [val] as Hexadecimal number with padding
	/// </summary>
	/// <param name="val"></param>
	/// <returns></returns>
	static std::string GetHexFill(uint32_t val)
	{
		std::stringstream ss;
		ss << std::setw(16) << std::hex << std::setfill('0') << val;
		return ss.str();
	}
	/// <summary>
	/// Returns a string showing [val] as Hexadecimal number with padding
	/// </summary>
	/// <param name="val"></param>
	/// <returns></returns>
	static std::string GetHexFill(uint64_t val)
	{
		std::stringstream ss;
		ss << std::setw(16) << std::hex << std::setfill('0') << val;
		return ss.str();
	}

	/// <summary>
	/// Splits a string at a delimiter, optionally removes empty results, and optionally respects escaped sequences
	/// </summary>
	/// <param name="str">input to split</param>
	/// <param name="delimiter">delimiter at which to split</param>
	/// <param name="removeEmpty">whether to remove empty strings</param>
	/// <param name="escape">whether to respect escaped sequences</param>
	/// <param name="escapesymbol">symbol that marks escaped sequences, sequences need to be surrounded by this symbol</param>
	/// <returns></returns>
	static std::vector<std::string> SplitString(std::string str, char delimiter, bool removeEmpty, bool escape = false, char escapesymbol = '\"');

	/// <summary>
	/// Splits a string at a delimiter, optionally removes empty results, and optionally respects escaped sequences
	/// </summary>
	/// <param name="str">input to split</param>
	/// <param name="delimiter">delimiter at which to split</param>
	/// <param name="removeEmpty">whether to remove empty strings</param>
	/// <param name="escape">whether to respect escaped sequences</param>
	/// <param name="escapesymbol">symbol that marks escaped sequences, sequences need to be surrounded by this symbol</param>
	/// <returns></returns>
	static std::vector<std::string> SplitString(std::string str, const char* delimiter, bool removeEmpty, bool escape = false, char escapesymbol = '\"');
	/// <summary>
	/// Removes whitespace from an input in-place, optionally respects escaped sequences
	/// </summary>
	static std::string& RemoveWhiteSpaces(std::string& str, char escape, bool removetab = false);

	/// <summary>
	/// Removes Symbols from an input in-place, optionally respects escaped sequences
	/// </summary>
	static std::string& RemoveSymbols(std::string& str, char symbol, bool enableescape = false, char escape = '\"');

	/// <summary>
	/// Counts the occurences of the given symbol, respecting escaped sequences
	/// </summary>
	static int CountSymbols(std::string str, char symbol, char escaped1, char escaped2)
	{
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
	}
};
