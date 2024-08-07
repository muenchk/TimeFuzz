#pragma once

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <optional>

#if defined(unix) || defined(__unix__) || defined(__unix)
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include <Windows.h>
#endif

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
		std::ifstream stream(std::filesystem::path("/proc/self/comm"), std::ios_base::in);
		stream >> sp;
		return sp;

		//char result[ 100 ];
  		//ssize_t count = readlink( "/proc/self/comm", result, 100 );
  		//return std::string( result, (count > 0) ? count : 0 );

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
	static std::string GetHex(int64_t val)
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
	static std::string GetHex(int32_t val)
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
	static std::vector<std::string> SplitString(std::string str, char delimiter, bool removeEmpty, bool escape = false, char escapesymbol = '\"', bool allowdisableescape = false, char disblechar = '\\');

	/// <summary>
	/// Splits a string at a delimiter, optionally removes empty results, and optionally respects escaped sequences
	/// </summary>
	/// <param name="str">input to split</param>
	/// <param name="delimiter">delimiter at which to split</param>
	/// <param name="removeEmpty">whether to remove empty strings</param>
	/// <param name="escape">whether to respect escaped sequences</param>
	/// <param name="escapesymbol">symbol that marks escaped sequences, sequences need to be surrounded by this symbol</param>
	/// <returns></returns>
	static std::vector<std::string> SplitString(std::string str, std::string delimiter, bool removeEmpty, bool escape = false, char escapesymbol = '\"');
	/// <summary>
	/// Removes whitespace from an input in-place, optionally respects escaped sequences
	/// </summary>
	static std::string& RemoveWhiteSpaces(std::string& str, char escape, bool removetab = false, bool allowdisableescape = false, char disablechar = '\\');

	/// <summary>
	/// Removes Symbols from an input in-place, optionally respects escaped sequences
	/// </summary>
	static std::string& RemoveSymbols(std::string& str, char symbol, bool enableescape = false, char escape = '\"');

	/// <summary>
	/// Removes Symbols from an input in-place, optionally doesn't remove escaped characters
	/// </summary>
	static std::string& RemoveSymbols(std::string& str, char symbol, char disablechar);

	/// <summary>
	/// Counts the occurences of the given symbol, respecting escaped sequences
	/// </summary>
	static int32_t CountSymbols(std::string str, char symbol, char escaped1, char escaped2)
	{
		int32_t count = 0;
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

	// From Crashandler

	[[nodiscard]] static auto ConvertToWideString(std::string_view a_in) noexcept
		-> std::optional<std::wstring>
	{
		const auto len = mbstowcs(nullptr, a_in.data(), a_in.length());
		if (len == 0) {
			return std::nullopt;
		}

		std::wstring out(len, '\0');
		mbstowcs(out.data(), a_in.data(), a_in.length());

		return out;
	}

	[[nodiscard]] static auto ConvertToString(std::wstring_view a_in) noexcept
		-> std::optional<std::string>
	{
		const wchar_t* input = a_in.data();

		// Count required buffer size (plus one for null-terminator).
		size_t size = (wcslen(input) + 1) * sizeof(wchar_t);
		std::string out(size, '\0');

		size_t convertedSize;
		//wcstombs_s(&convertedSize, out.data(), size, input, size);
		convertedSize = wcstombs(out.data(), input, size);

		if (strcmp(out.c_str(), ""))
			return std::nullopt;

		return out;
	}

	static std::string ReadFile(std::filesystem::path path);
};
