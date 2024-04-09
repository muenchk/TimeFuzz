#pragma once

#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <semaphore>
#include <source_location>

class Logging
{
public:
	static inline bool EnableLog = true;
	static inline bool EnableProfile = true;
	static inline std::filesystem::path log_directory;
};

#define loginfo(...)									\
	if (Logging::EnableLog) {							\
		static_cast<void>(loginf(__VA_ARGS__));			\
	}

#define logwarn(...)                         \
	if (Logging::EnableLog) {                \
		static_cast<void>(warn(__VA_ARGS__)); \
	}

#define logcritical(...)                         \
	if (Logging::EnableLog) {                \
		static_cast<void>(crit(__VA_ARGS__)); \
	}

#define profile(...)									\
	if (Logging::EnableProfile) {						\
		static_cast<void>(prof(__VA_ARGS__));			\
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
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = std::string(std::filesystem::path(a_loc.file_name()).filename().string()) + "(" + std::to_string(static_cast<int>(a_loc.line())) + "): [profiling] " + fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n";
		Profile::write(mes);
	}
};

template <class... Args>
prof(fmt::format_string<Args...>, Args&&...) -> prof<Args...>;


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
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = std::string(std::filesystem::path(a_loc.file_name()).filename().string()) + "(" + std::to_string(static_cast<int>(a_loc.line())) + "): [info] " + fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n";
		Log::write(mes);
	}
};

template <class... Args>
loginf(fmt::format_string<Args...>, Args&&...) -> loginf<Args...>;

template <class... Args>
struct [[maybe_unused]] warn
{
	warn() = delete;

	explicit warn(
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = std::string(std::filesystem::path(a_loc.file_name()).filename().string()) + "(" + std::to_string(static_cast<int>(a_loc.line())) + "): [warning] " + fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n";
		Log::write(mes);
	}
};

template <class... Args>
warn(fmt::format_string<Args...>, Args&&...) -> warn<Args...>;

template <class... Args>
struct [[maybe_unused]] crit
{
	crit() = delete;

	explicit crit(
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = std::string(std::filesystem::path(a_loc.file_name()).filename().string()) + "(" + std::to_string(static_cast<int>(a_loc.line())) + "): [critical] " + fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n";
		Log::write(mes);
	}
};

template <class... Args>
crit(fmt::format_string<Args...>, Args&&...) -> crit<Args...>;
