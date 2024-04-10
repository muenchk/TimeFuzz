#pragma once

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
};
