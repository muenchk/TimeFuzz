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
#include <atomic>
#include <random>

#if defined(unix) || defined(__unix__) || defined(__unix)
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#	include <Windows.h>
#endif

typedef uint64_t EnumType;

class Input;

#include "Form.h"

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

	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	static std::string PrintForm(std::shared_ptr<T> form)
	{
		if (!form || form->GetFormID() == 0)
			return "None";
		return std::string("[") + typeid(T).name() + "<" + Utility::GetHex(form->GetFormID()) + ">";
	}
	template <class T, typename = std::enable_if<std::is_base_of<Form, T>::value>>
	static std::string PrintForm(Types::shared_ptr<T> form)
	{
		if (!form || form->GetFormID() == 0)
			return "None";
		return std::string("[") + typeid(T).name() + "<" + Utility::GetHex(form->GetFormID()) + ">";
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
	/// Returns a string showing [buffer] as Hexadecimal
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	/// <returns></returns>
	static std::string GetHex(unsigned char* buffer, size_t length)
	{
		std::stringstream ss;
		for (size_t i = 0; i < length; i++)
			ss << std::setw(2) << std::hex << std::setfill('0') << (unsigned short)buffer[i];
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

	static std::string AccString(std::vector<std::string>& vec, char sep = '|')
	{
		std::string res = "";
		if (vec.size() > 0)
		{
			res += vec[0];
			for (int64_t i = 0; i < (int64_t)vec.size(); i++)
				res += sep + vec[i];
		}
		return res;
	}
	static std::string AccString(std::vector<const char*>& vec, char sep = '|')
	{
		std::string res = "";
		if (vec.size() > 0) {
			res += vec[0];
			for (int64_t i = 0; i < (int64_t)vec.size(); i++) {
				if (vec[i] != NULL)
					res += sep + std::string(vec[i]);
				else
					res += sep;
			}
		}
		return res;
	}

	/// <summary>
	/// Evaluates whether the char at position [position] is escaped
	/// </summary>
	/// <param name="str">input to search</param>
	/// <param name="escapesymbol">symbol that marks escaped sequences, sequences need to be surrounded by this symbol</param>
	/// <param name="position">the position to check</param>
	/// <returns></returns>
	static bool IsEscaped(std::string str, int32_t position, char escapesymbol = '\"')
	{
		bool escaped = false;
		int32_t count = 0;
		for (char c : str) {
			//logdebug("Char: {}\t Window: {}\t Wsize: {}\t Wmaxsize: {}\t tmp: {}", c, slide.GetWindow(), slide.size, slide.maxsize, tmp);
			if (escaped == true) {
				// escaped sequence.
				if (c == escapesymbol) {
					escaped = !escaped;
				}
			} else {
				if (c == escapesymbol) {
					// we are at the beginning of an escaped sequence
					escaped = !escaped;
				}
			}
			if (count == position)
				return escaped;
			count++;
		}
		return false;
	}

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

	static std::vector<std::pair<char, int32_t>> GetSymbols(std::string str);

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

	class SpinLock
	{
	private:
		std::atomic_flag* _flag;
		// adapted from cppreference code on atomic_flag
	public:
		SpinLock(std::atomic_flag& flag) :
			_flag(std::addressof(flag))
		{
			while (_flag->test_and_set(std::memory_order_acquire))
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
				_flag->wait(true, std::memory_order_relaxed)
#endif
					;
		}
		~SpinLock() 
		{
			// exit flag
			_flag->clear(std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
			_flag->notify_one();
#endif
		}
	};


	template<class T>
	static uint64_t RandomInt(T min = 0, T max = UINT64_MAX)
	{
		std::mt19937 randan((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
		std::uniform_int_distribution<T> dist(min, max);
		return dist(randan);
	}
};

