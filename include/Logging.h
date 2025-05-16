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
#include <optional>
#include <unordered_map>
#include <mutex>

#include "Threading.h"
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
	/// whether to write logging messages to stdout
	/// </summary>
	static inline bool StdOutLogging = false;
	/// <summary>
	/// whether to write warnings to stdout
	/// </summary>
	static inline bool StdOutWarn = true;
	/// <summary>
	/// whether to write errors to stdout
	/// </summary>
	static inline bool StdOutError = true;
	/// <summary>
	/// whether to write debug messages to stdout
	/// </summary>
	static inline bool StdOutDebug = true;

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
	static std::string FormatTime(int64_t microseconds)
	{
		std::stringstream ss;
		int64_t tmp = 0;
		if (microseconds > 60000000) {
			tmp = (int64_t)trunc((long double)microseconds / 60000000);
			ss << std::setw(6) << tmp << "m ";
			microseconds -= tmp * 60000000;
		} else
			ss << "        ";
		if (microseconds > 1000000) {
			tmp = (int64_t)trunc((long double)microseconds / 1000000);
			ss << std::setw(2) << tmp << "s ";
			microseconds -= tmp * 1000000;
		} else
			ss << "    ";
		if (microseconds > 1000) {
			tmp = (int64_t)trunc((long double)microseconds / 1000);
			ss << std::setw(3) << tmp << "ms ";
			microseconds -= tmp * 1000;
		} else
			ss << "      ";
		ss << std::setw(3) << microseconds << "μs";
		return ss.str();
	}

	/// <summary>
	/// Formats microseconds into a proper time string
	/// </summary>
	/// <returns></returns>
	static std::string FormatTimeNS(int64_t nanoseconds)
	{
		std::stringstream ss;
		int64_t tmp = 0;
		if (nanoseconds > 60000000000) {
			tmp = (int64_t)trunc((long double)nanoseconds / 60000000000);
			ss << std::setw(6) << tmp << "m ";
			nanoseconds -= tmp * 60000000000;
		} else
			ss << "        ";
		if (nanoseconds > 1000000000) {
			tmp = (int64_t)trunc((long double)nanoseconds / 1000000000);
			ss << std::setw(2) << tmp << "s ";
			nanoseconds -= tmp * 1000000000;
		} else
			ss << "    ";
		if (nanoseconds > 1000000) {
			tmp = (int64_t)trunc((long double)nanoseconds / 1000000);
			ss << std::setw(3) << tmp << "ms ";
			nanoseconds -= tmp * 1000000;
		} else
			ss << "      ";
		if (nanoseconds > 1000) {
			tmp = (int64_t)trunc((long double)nanoseconds / 1000);
			ss << std::setw(3) << tmp << "μs ";
			nanoseconds -= tmp * 1000;
		} else
			ss << "      ";
		ss << std::setw(3) << nanoseconds << "ns";
		return ss.str();
	}

	static void InitializeLog(std::filesystem::path _path, bool append = false, bool timestamp = false);
};

/*#ifndef NDEBUG
#	define StartProfilingDebug \
	auto $$profiletimebegin$$ = std::chrono::steady_clock::now();
#	define TimeProfilingDebug \
	$$profiletimebegin$$
#	define profileDebug(...)                                    \
		static_cast<void>(prof(__func__, __VA_ARGS__));
#else
#	define StartProfilingDebug ;
#	define TimeProfilingDebug ;
#	define profileDebug(...) ;
#endif*/
#define StartProfilingDebug \
	auto $$profiletimebegin$$ = std::chrono::steady_clock::now();
#define ResetProfilingDebug \
	$$profiletimebegin$$ = std::chrono::steady_clock::now();
#define TimeProfilingDebug \
	$$profiletimebegin$$
#define profileDebug(...) \
	if (Logging::EnableDebug) {                               \
		static_cast<void>(prof(__func__, true, __VA_ARGS__)); \
	}
#define profileWDebug(...)                                     \
	if (Logging::EnableDebug) {                               \
		void();   \ /* static_cast<void>(prof(__func__, false, __VA_ARGS__));*/ \
	}

#define StartProfiling \
	auto $$profiletimebegin$$ = std::chrono::steady_clock::now();
#define ResetProfiling \
	$$profiletimebegin$$ = std::chrono::steady_clock::now();

#define TimeProfiling \
	$$profiletimebegin$$

#define loginfo(...)                          \
	if (Logging::EnableLog) { static_cast<void>(loginf(__func__, __VA_ARGS__)); }

#define logwarn(...)                          \
	if (Logging::EnableLog) { static_cast<void>(warn(__func__, __VA_ARGS__)); }

#define logcritical(...)                      \
	if (Logging::EnableLog) { static_cast<void>(crit(__func__, __VA_ARGS__)); }

#define logdebug(...)                                   \
	if (Logging::EnableDebug) {                          \
		static_cast<void>(debug(__func__, __VA_ARGS__)); \
	}
#define logdebug2(...)                                    \
	if (Logging::EnableDebug) {                          \
		void();  /* static_cast<void>(debug(__func__, __VA_ARGS__));*/ \
	}
#define logtest(...)                                      \
	if (Logging::EnableDebug) {                             \
		static_cast<void>(logTest(__func__, __VA_ARGS__)); \
	}

#define profile(...)                          \
	if (Logging::EnableProfile) \
		static_cast<void>(prof(__func__, true, __VA_ARGS__));  // we check for enabled in the function itself, so we can save the exectimes itself
#define profileW(...) \
	if (Logging::EnableProfile) \
		static_cast<void>(prof(__func__, false, __VA_ARGS__));  // we check for enabled in the function itself, so we can save the exectimes itself

#define logmessage(...) \
	static_cast<void>(logmes(__func__, __VA_ARGS__)); \


class Profile
{
	static inline std::ofstream* _stream = nullptr;
	static inline std::binary_semaphore lock{ 1 };
	static inline std::mutex _m_lock;
	static inline std::atomic_flag _exec_flag = ATOMIC_FLAG_INIT;
	static inline bool _writelog;

public:

	struct ExecTime {
		private:
			std::mutex _lock;
			public:
		std::string functionName;
		std::chrono::nanoseconds exectime;
		uint64_t executions = 0;
		std::chrono::nanoseconds average;
		std::chrono::steady_clock::time_point lastexec;
		std::string fileName;
		std::string usermessage;
		void Lock()
		{
			_lock.lock();
		}

		void Unlock()
		{
			_lock.unlock();
		}
	};

	/// <summary>
	/// contains the execution times of the last logged function executions
	/// exectime, timepoint, name, file, usermes
	/// </summary>
	static inline std::unordered_map<std::string, ExecTime*> exectimes;

	static void Dispose()
	{
		SpinlockA guard(_exec_flag);
		//lock.acquire();
		for (auto [func, exec] : exectimes)
			delete exec;
		exectimes.clear();
		//lock.release();
	}

	/// <summary>
	/// Inits profile log
	/// </summary>
	/// <param name="pluginname"></param>
	static void Init(std::string pluginname, bool append = false)
	{
		std::unique_lock<std::mutex> guard(_m_lock);
		//lock.acquire();
		//auto path = SKSE::log::log_directory();
		//if (path.has_value()) {
		//	_stream = new std::ofstream(path.value() / (pluginname + "_profile.log"), std::ios_base::out | std::ios_base::trunc);
		//}
		if (!append)
			_stream = new std::ofstream(Logging::log_directory / (pluginname + "_profile.log"), std::ios_base::out | std::ios_base::trunc);
		else
			_stream = new std::ofstream(Logging::log_directory / (pluginname + "_profile.log"), std::ios_base::out | std::ios_base::ate);
		if (_stream == nullptr || _stream->is_open() == false)
			std::cout << "Cannot create Profiling Log!\n";
		//lock.release();
	}

	static void SetWriteLog(bool writelogfile)
	{
		_writelog = writelogfile;
	}

	/// <summary>
	/// Closes profile log
	/// </summary>
	static void Close()
	{
		std::unique_lock<std::mutex> guard(_m_lock);
		//lock.acquire();
		if (_stream != nullptr) {
			_stream->flush();
			_stream->close();
			delete _stream;
			_stream = nullptr;
		}
		//lock.release();
	}
	static void log(std::string message, std::string func, std::string file, std::chrono::nanoseconds ns, std::chrono::steady_clock::time_point time, std::string usermes, bool writetolog)
	{
		//lock.acquire();
		if (writetolog)
			write(message);

		std::unordered_map<std::string, ExecTime*>::iterator itr = exectimes.end();
		{
			SpinlockA guard(_exec_flag);
			itr = exectimes.find(file + "::" + func);
		}
		if (itr != exectimes.end()) {
			itr->second->Lock();
			itr->second->functionName = func;
			itr->second->average = std::chrono::nanoseconds((long long)(itr->second->exectime.count() * itr->second->executions + ns.count()) / (itr->second->executions + 1));
			itr->second->executions++;
			itr->second->exectime = ns;
			itr->second->lastexec = time;
			itr->second->fileName = file;
			itr->second->usermessage = usermes;
			itr->second->Unlock();
		} else {
			ExecTime* exec = new ExecTime();
			exec->Lock();
			{
				SpinlockA guard(_exec_flag);
				exectimes.insert_or_assign(file + "::" + func, exec);
			}
			exec->functionName = func;
			exec->average = ns;
			exec->exectime = ns;
			exec->executions = 1;
			exec->lastexec = time;
			exec->fileName = file;
			exec->usermessage = usermes;

			exec->Unlock();
		}
		//lock.release();
	}

	/// <summary>
	/// writes to the profile log
	/// </summary>
	/// <typeparam name="...Args"></typeparam>
	/// <param name="message"></param>
	template <class... Args>
	static void write(std::string message)
	{
		std::unique_lock<std::mutex> guard(_m_lock);
		if (_stream) {
			try {
				_stream->write(message.c_str(), message.size());
				_stream->flush();
			}
			catch (std::exception& e) {
				std::cout << "Error in profiling stream: " << e.what() << "\n";
			}
		}
	}
};
template <class... Args>
struct [[maybe_unused]] prof
{
	prof() = delete;

	explicit prof(
		std::string func,
		bool writetolog,
		std::chrono::time_point<std::chrono::steady_clock> begin,
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		auto now = std::chrono::steady_clock::now();
		std::string file = std::filesystem::path(a_loc.file_name()).filename().string();
		std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - begin);
		std::string usermes = fmt::format(a_fmt, std::forward<Args>(a_args)...);
		std::string mes = fmt::format("{:<30} {} {} {:<40} [ExecTime:{:>11}]\t{}", file + "(" + std::to_string(static_cast<int>(a_loc.line())) + "):", "[prof]", Logging::TimePassed(), "[" + func + "]", Logging::FormatTime(std::chrono::duration_cast<std::chrono::microseconds>(ns).count()), usermes+ "\n");

		Profile::log(mes, func, file, ns, now, usermes, writetolog);
	}
};

template <class... Args>
prof(std::string, bool, std::chrono::time_point<std::chrono::steady_clock>, fmt::format_string<Args...>, Args&&...) -> prof<Args...>;


class Log
{
	static inline std::ofstream* _stream = nullptr;
	static inline std::binary_semaphore lock{ 1 };
	static inline std::mutex _m_lock;

public:
	/// <summary>
	/// Inits profile log
	/// </summary>
	/// <param name="pluginname"></param>
	static void Init(std::string pluginname, bool append = false)
	{
		std::unique_lock<std::mutex> guard(_m_lock);
		//lock.acquire();
		//auto path = SKSE::log::log_directory();
		//if (path.has_value()) {
		//	_stream = new std::ofstream(path.value() / pluginname / (pluginname + "log.log"), std::ios_base::out | std::ios_base::trunc);
		//}
		if (!append)
			_stream = new std::ofstream(Logging::log_directory / (pluginname + ".log"), std::ios_base::out | std::ios_base::trunc);
		else
			_stream = new std::ofstream(Logging::log_directory / (pluginname + ".log"), std::ios_base::out | std::ios_base::ate);
		if (_stream == nullptr || _stream->is_open() == false)
			std::cout << "Cannot create Log!\n";
		//lock.release();
	}

	/// <summary>
	/// Closes log log
	/// </summary>
	static void Close()
	{
		std::unique_lock<std::mutex> guard(_m_lock);
		//lock.acquire();
		if (_stream != nullptr) {
			_stream->flush();
			_stream->close();
			delete _stream;
			_stream = nullptr;
		}
		//lock.release();
	}

	/// <summary>
	/// writes to the log log
	/// </summary>
	/// <typeparam name="...Args"></typeparam>
	/// <param name="message"></param>
	template <class... Args>
	static void write(std::string message)
	{
		std::unique_lock<std::mutex> guard(_m_lock);
		//lock.acquire();
		if (_stream) {
			try {
				_stream->write(message.c_str(), message.size());
				_stream->flush();
			} catch (std::exception& e) {
				std::cout << "Error in logging stream: " << e.what() << "\n";
			}
		}
		//lock.release();
	}
};

template <class... Args>
struct [[maybe_unused]] logmes
{
	logmes() = delete;

	explicit logmes(
		std::string func,
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = fmt::format("{:<25} {} {} {:<30}\t{}", std::filesystem::path(a_loc.file_name()).filename().string() + "(" + std::to_string(static_cast<int>(a_loc.line())) + "):", "[info]    ", Logging::TimePassed(), "[" + func + "]", fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n");
		Log::write(mes);
		std::cout << mes;
	}
};

template <class... Args>
logmes(std::string, fmt::format_string<Args...>, Args&&...) -> logmes<Args...>;

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
		if (Logging::StdOutLogging)
			std::cout << mes;
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
		if (Logging::StdOutWarn)
			std::cout << mes;
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
		if (Logging::StdOutError)
			std::cout << mes;
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
		if (Logging::StdOutDebug)
			std::cout << mes;
	}
};

template <class... Args>
debug(std::string, fmt::format_string<Args...>, Args&&...) -> debug<Args...>;



class TestLog
{
	static inline std::ofstream* _stream = nullptr;
	static inline std::binary_semaphore lock{ 1 };
	static inline std::mutex _m_lock;

public:
	/// <summary>
	/// Inits profile log
	/// </summary>
	/// <param name="pluginname"></param>
	static void Init(std::string pluginname, bool append = false)
	{
		std::unique_lock<std::mutex> guard(_m_lock);
		//lock.acquire();
		//auto path = SKSE::log::log_directory();
		//if (path.has_value()) {
		//	_stream = new std::ofstream(path.value() / pluginname / (pluginname + "log.log"), std::ios_base::out | std::ios_base::trunc);
		//}
		if (!append)
			_stream = new std::ofstream(Logging::log_directory / (pluginname + "_test.log"), std::ios_base::out | std::ios_base::trunc);
		else
			_stream = new std::ofstream(Logging::log_directory / (pluginname + "_test.log"), std::ios_base::out | std::ios_base::ate);
		if (_stream == nullptr || _stream->is_open() == false)
			std::cout << "Cannot create Log!\n";
		//lock.release();
	}

	/// <summary>
	/// Closes log log
	/// </summary>
	static void Close()
	{
		std::unique_lock<std::mutex> guard(_m_lock);
		//lock.acquire();
		if (_stream != nullptr) {
			_stream->flush();
			_stream->close();
			delete _stream;
			_stream = nullptr;
		}
		//lock.release();
	}

	/// <summary>
	/// writes to the log log
	/// </summary>
	/// <typeparam name="...Args"></typeparam>
	/// <param name="message"></param>
	template <class... Args>
	static void write(std::string message)
	{
		std::unique_lock<std::mutex> guard(_m_lock);
		//lock.acquire();
		if (_stream) {
			try {
				_stream->write(message.c_str(), message.size());
				_stream->flush();
			} catch (std::exception& e) {
				std::cout << "Error in logging stream: " << e.what() << "\n";
			}
		}
		//lock.release();
	}
};

template <class... Args>
struct [[maybe_unused]] logTest
{
	logTest() = delete;

	explicit logTest(
		std::string func,
		fmt::format_string<Args...> a_fmt,
		Args&&... a_args,
		std::source_location a_loc = std::source_location::current())
	{
		std::string mes = fmt::format("{:<25} {} {} {:<30}\t{}", std::filesystem::path(a_loc.file_name()).filename().string() + "(" + std::to_string(static_cast<int>(a_loc.line())) + "):", "[test]   ", Logging::TimePassed(), "[" + func + "]", fmt::format(a_fmt, std::forward<Args>(a_args)...) + "\n");
		TestLog::write(mes);
	}
};

template <class... Args>
logTest(std::string, fmt::format_string<Args...>, Args&&...) -> logTest<Args...>;
