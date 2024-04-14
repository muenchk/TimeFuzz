#pragma once

#include <chrono>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <semaphore>
#include <source_location>
#include <sstream>
#include <string>
#include <math.h>

#include "Utility.h"

class Logging
{
public:
	/// <summary>
	/// whether to enable logging output
	/// </summary>
	static inline bool EnableLog = true;
	/// <summary>
	/// whether to enable advanced debug logging
	/// </summary>
	static inline bool EnableDebug = true;
	/// <summary>
	/// whether to enable profiling output
	/// </summary>
	static inline bool EnableProfile = true;
	/// <summary>
	/// the path to store log files
	/// </summary>
	static inline std::filesystem::path log_directory;

	/// <summary>
	/// time the executable was started
	/// </summary>
	static inline std::chrono::time_point<std::chrono::system_clock> execstart = std::chrono::system_clock::now();

	/// <summary>
	/// calculates and returns the time passed since programstart
	/// </summary>
	/// <returns></returns>
	static std::string TimePassed()
	{
		std::stringstream ss;
		ss << std::setw(12) << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - execstart);
		return ss.str();
	}

	/// <summary>
	/// Formats microseconds into a proper time string
	/// </summary>
	/// <returns></returns>
	static std::string FormatTime(long long microseconds)
	{
		std::stringstream ss;
		long long tmp = 0;
		if (microseconds > 60000000) {
			tmp = (long long)trunc((long double)microseconds / 60000000);
			ss << std::setw(6) << tmp << "m ";
			microseconds -= tmp * 60000000;
		} else
			ss << "        ";
		if (microseconds > 1000000) {
			tmp = (long long)trunc((long double)microseconds / 1000000);
			ss << std::setw(2) << tmp << "s ";
			microseconds -= tmp * 1000000;
		} else
			ss << "    ";
		if (microseconds > 1000) {
			tmp = (long long)trunc((long double)microseconds / 1000);
			ss << std::setw(3) << tmp << "ms ";
			microseconds -= tmp * 1000;
		} else
			ss << "      ";
		ss << std::setw(3) << microseconds << "μs";
		return ss.str();
	}

	static void InitializeLog(std::filesystem::path _path);
};

#define StartProfiling \
	auto begin = std::chrono::steady_clock::now();

#define TimeProfiling \
	begin

#define loginfo(...)                          \
	if (Logging::EnableLog) {                 \
		static_cast<void>(loginf(__func__, __VA_ARGS__)); \
	}

#define logwarn(...)                          \
	if (Logging::EnableLog) {                 \
		static_cast<void>(warn(__func__, __VA_ARGS__)); \
	}

#define logcritical(...)                      \
	if (Logging::EnableLog) {                 \
		static_cast<void>(crit(__func__, __VA_ARGS__)); \
	}

#define logdebug(...)                                   \
	if (Logging::EnableDebug) {                           \
		static_cast<void>(debug(__func__, __VA_ARGS__)); \
	}

#define profile(...)                          \
	if (Logging::EnableProfile) {             \
		static_cast<void>(prof(__func__, __VA_ARGS__)); \
	}



class Profile
{
	static inline std::ofstream* _stream = nullptr;
	static inline std::binary_semaphore lock{ 1 };

public:
	/// <summary>
	/// Inits profile log
	/// </summary>
	/// <param name="pluginname"></param>
	static void Init(std::string pluginname)
	{
		lock.acquire();
		//auto path = SKSE::log::log_directory();
		//if (path.has_value()) {
		//	_stream = new std::ofstream(path.value() / pluginname / (pluginname + "_profile.log"), std::ios_base::out | std::ios_base::trunc);
		//}
		_stream = new std::ofstream(Logging::log_directory / pluginname / (pluginname + "_profile.log"), std::ios_base::out | std::ios_base::trunc);
		lock.release();
	}

	/// <summary>
	/// Closes profile log
	/// </summary>
	static void Close()
	{
		lock.acquire();
		if (_stream != nullptr) {
			_stream->flush();
			_stream->close();
			delete _stream;
			_stream = nullptr;
		}
		lock.release();
	}

	/// <summary>
	/// writes to the profile log
	/// </summary>
	/// <typeparam name="...Args"></typeparam>
	/// <param name="message"></param>
	template <class... Args>
	static void write(std::string message)
	{
		lock.acquire();
		if (_stream) {
			_stream->write(message.c_str(), message.size());
			_stream->flush();
		}
		lock.release();
	}
};
template <class... Args>
struct [[maybe_unused]] prof
{
	prof() = delete;

	explicit prof(
		std::string func,
		std::chrono::time_point<std::chrono::steady_clock> begin,
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = fmt::format("{:<25} {} {} {:<30} [ExecTime:{:>11}]\t{}", std::filesystem::path(a_loc.file_name()).filename().string() + "(" + std::to_string(static_cast<int>(a_loc.line())) + "):", "[prof]", Logging::TimePassed(), "[" + func + "]", Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - begin).count()), fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n");
		Profile::write(mes);
	}
};

template <class... Args>
prof(std::string, std::chrono::time_point<std::chrono::steady_clock>, fmt::format_string<Args...>, Args&&...) -> prof<Args...>;


class Log
{
	static inline std::ofstream* _stream = nullptr;
	static inline std::binary_semaphore lock{ 1 };

public:
	/// <summary>
	/// Inits profile log
	/// </summary>
	/// <param name="pluginname"></param>
	static void Init(std::string pluginname)
	{
		lock.acquire();
		//auto path = SKSE::log::log_directory();
		//if (path.has_value()) {
		//	_stream = new std::ofstream(path.value() / pluginname / (pluginname + "log.log"), std::ios_base::out | std::ios_base::trunc);
		//}
		_stream = new std::ofstream(Logging::log_directory / pluginname / (pluginname + ".log"), std::ios_base::out | std::ios_base::trunc);
		lock.release();
	}

	/// <summary>
	/// Closes log log
	/// </summary>
	static void Close()
	{
		lock.acquire();
		if (_stream != nullptr) {
			_stream->flush();
			_stream->close();
			delete _stream;
			_stream = nullptr;
		}
		lock.release();
	}

	/// <summary>
	/// writes to the log log
	/// </summary>
	/// <typeparam name="...Args"></typeparam>
	/// <param name="message"></param>
	template <class... Args>
	static void write(std::string message)
	{
		lock.acquire();
		if (_stream) {
			_stream->write(message.c_str(), message.size());
			_stream->flush();
		}
		lock.release();
	}
};
template <class... Args>
struct [[maybe_unused]] loginf
{
	loginf() = delete;

	explicit loginf(
		std::string func,
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = fmt::format("{:<25} {} {} {:<30}\t{}", std::filesystem::path(a_loc.file_name()).filename().string() + "(" + std::to_string(static_cast<int>(a_loc.line())) + "):", "[info]    ", Logging::TimePassed(), "[" + func + "]", fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n");
		Log::write(mes);
	}
};

template <class... Args>
loginf(std::string, fmt::format_string<Args...>, Args&&...) -> loginf<Args...>;

template <class... Args>
struct [[maybe_unused]] warn
{
	warn() = delete;

	explicit warn(
		std::string func,
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = fmt::format("{:<25} {} {} {:<30}\t{}", std::filesystem::path(a_loc.file_name()).filename().string() + "(" + std::to_string(static_cast<int>(a_loc.line())) + "):", "[warning] ", Logging::TimePassed(), "[" + func + "]", fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n");
		Log::write(mes);
	}
};

template <class... Args>
warn(std::string, fmt::format_string<Args...>, Args&&...) -> warn<Args...>;

template <class... Args>
struct [[maybe_unused]] crit
{
	crit() = delete;

	explicit crit(
		std::string func,
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = fmt::format("{:<25} {} {} {:<30}\t{}", std::filesystem::path(a_loc.file_name()).filename().string() + "(" + std::to_string(static_cast<int>(a_loc.line())) + "):", "[critical]", Logging::TimePassed(), "[" + func + "]", fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n");
		Log::write(mes);
	}
};

template <class... Args>
crit(std::string, fmt::format_string<Args...>, Args&&...) -> crit<Args...>;

template <class... Args>
struct [[maybe_unused]] debug
{
	debug() = delete;

	explicit debug(
		std::string func,
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = fmt::format("{:<25} {} {} {:<30}\t{}", std::filesystem::path(a_loc.file_name()).filename().string() + "(" + std::to_string(static_cast<int>(a_loc.line())) + "):", "[debug]   ", Logging::TimePassed(), "[" + func + "]", fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n");
		Log::write(mes);
	}
};

template <class... Args>
debug(std::string, fmt::format_string<Args...>, Args&&...) -> debug<Args...>;
