
#include "dxgi1_4.h"
#include <span>
#include <boost/stacktrace.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "CrashHandler.h"

using namespace std::literals;

namespace Crash
{
	namespace Modules
	{
		namespace detail
		{
			class Factory;
		}

		class Module
		{
		};
	}

	Callstack::Callstack(const ::EXCEPTION_RECORD& a_except)
	{
		const auto exceptionAddress = reinterpret_cast<std::uintptr_t>(a_except.ExceptionAddress);
		auto it = std::find_if(_stacktrace.cbegin(), _stacktrace.cend(), [&](auto&& a_elem) noexcept {
			return reinterpret_cast<std::uintptr_t>(a_elem.address()) == exceptionAddress;
		});

		if (it == _stacktrace.cend()) {
			it = _stacktrace.cbegin();
		}

		_frames = std::span(it, _stacktrace.cend());
	}

	void Callstack::print(spdlog::logger& a_log, std::span<const module_pointer> a_modules) const
	{
		print_probable_callstack(a_log, a_modules);
	}

	std::string Callstack::get_size_string(std::size_t a_size)
	{
		return fmt::to_string(fmt::to_string(a_size - 1).length());
	}

	std::string Callstack::get_format(std::size_t a_nameWidth) const
	{
		return "\t[{:>"s + get_size_string(_frames.size()) + "}] 0x{:012X} {:>"s + fmt::to_string(a_nameWidth) + "}{}"s;
	}

	void Callstack::print_probable_callstack(spdlog::logger& a_log, std::span<const module_pointer> a_modules) const
	{
		a_log.critical("PROBABLE CALL STACK:"sv);

		std::vector<const Modules::Module*> moduleStack;
		moduleStack.reserve(_frames.size());
		for (const auto& frame : _frames) {
			const auto mod = Introspection::get_module_for_pointer(frame.address(), a_modules);
			if (mod && mod->in_range(frame.address())) {
				moduleStack.push_back(mod);
			} else {
				moduleStack.push_back(nullptr);
			}
		}

		const auto format = get_format([&]() {
			std::size_t max = 0;
			std::for_each(moduleStack.begin(), moduleStack.end(),
				[&](auto&& a_elem) { max = a_elem ? std::max(max, a_elem->name().length()) : max; });
			return max;
		}());

		for (std::size_t i = 0; i < _frames.size(); ++i) {
			const auto mod = moduleStack[i];
			const auto& frame = _frames[i];
			a_log.critical(fmt::runtime(format), i, reinterpret_cast<std::uintptr_t>(frame.address()), (mod ? mod->name() : ""sv),
				(mod ? mod->frame_info(frame) : ""s));
		}
	}

	void Callstack::print_raw_callstack(spdlog::logger& a_log) const
	{
		a_log.critical("RAW CALL STACK:");

		const auto format = "\t[{:>"s + get_size_string(_stacktrace.size()) + "}] 0x{:X}"s;

		for (std::size_t i = 0; i < _stacktrace.size(); ++i) {
			a_log.critical(fmt::runtime(format), i, reinterpret_cast<std::uintptr_t>(_stacktrace[i].address()));
		}
	}

	namespace
	{
		[[nodiscard]] std::shared_ptr<spdlog::logger> get_log()
		{
			std::optional<std::filesystem::path> path = crashPath;
			const auto time = std::time(nullptr);
			std::tm localTime{};
			if (gmtime_s(&localTime, &time) != 0) {
				util::report_and_fail("failed to get current time"sv);
			}

			std::stringstream buf;
			buf << "crash-"sv << std::put_time(&localTime, "%Y-%m-%d-%H-%M-%S") << ".log"sv;
			*path /= buf.str();

			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(path->string(), true);
			auto log = std::make_shared<spdlog::logger>("crash log"s, std::move(sink));
			log->set_pattern("%v"s);
			log->set_level(spdlog::level::trace);
			log->flush_on(spdlog::level::off);

			return log;
		}

		void print_exception(spdlog::logger& a_log, const ::EXCEPTION_RECORD& a_exception,
			std::span<const module_pointer> a_modules)
		{
#define EXCEPTION_CASE(a_code) \
	case a_code:               \
		return " \"" #a_code "\""sv

			const auto eptr = a_exception.ExceptionAddress;
			const auto eaddr = reinterpret_cast<std::uintptr_t>(a_exception.ExceptionAddress);

			const auto post = [&]() {
				const auto mod = Introspection::get_module_for_pointer(eptr, a_modules);
				if (mod) {
					const auto pdbDetails = Crash::PDB::pdb_details(mod->path(), eaddr - mod->address());
					const auto assembly = mod->assembly((const void*)eaddr);
					if (!pdbDetails.empty())
						return fmt::format(
							" {}+{:07X}\t{} | {})"sv,
							mod->name(),
							eaddr - mod->address(),
							assembly,
							pdbDetails);
					return fmt::format(" {}+{:07X}\t{}"sv, mod->name(), eaddr - mod->address(), assembly);
				} else {
					return ""s;
				}
			}();

			const auto exception = [&]() {
				switch (a_exception.ExceptionCode) {
					EXCEPTION_CASE(EXCEPTION_ACCESS_VIOLATION);
					EXCEPTION_CASE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
					EXCEPTION_CASE(EXCEPTION_BREAKPOINT);
					EXCEPTION_CASE(EXCEPTION_DATATYPE_MISALIGNMENT);
					EXCEPTION_CASE(EXCEPTION_FLT_DENORMAL_OPERAND);
					EXCEPTION_CASE(EXCEPTION_FLT_DIVIDE_BY_ZERO);
					EXCEPTION_CASE(EXCEPTION_FLT_INEXACT_RESULT);
					EXCEPTION_CASE(EXCEPTION_FLT_INVALID_OPERATION);
					EXCEPTION_CASE(EXCEPTION_FLT_OVERFLOW);
					EXCEPTION_CASE(EXCEPTION_FLT_STACK_CHECK);
					EXCEPTION_CASE(EXCEPTION_FLT_UNDERFLOW);
					EXCEPTION_CASE(EXCEPTION_ILLEGAL_INSTRUCTION);
					EXCEPTION_CASE(EXCEPTION_IN_PAGE_ERROR);
					EXCEPTION_CASE(EXCEPTION_INT_DIVIDE_BY_ZERO);
					EXCEPTION_CASE(EXCEPTION_INT_OVERFLOW);
					EXCEPTION_CASE(EXCEPTION_INVALID_DISPOSITION);
					EXCEPTION_CASE(EXCEPTION_NONCONTINUABLE_EXCEPTION);
					EXCEPTION_CASE(EXCEPTION_PRIV_INSTRUCTION);
					EXCEPTION_CASE(EXCEPTION_SINGLE_STEP);
					EXCEPTION_CASE(EXCEPTION_STACK_OVERFLOW);
				default:
					return ""sv;
				}
			}();

			a_log.critical("Unhandled exception{} at 0x{:012X}{}"sv, exception, eaddr, post);

#undef EXCEPTION_CASE
		}

		void print_modules(spdlog::logger& a_log, std::span<const module_pointer> a_modules)
		{
			a_log.critical("MODULES:"sv);

			const auto format = [&]() {
				const auto width = [&]() {
					std::size_t max = 0;
					std::for_each(a_modules.begin(), a_modules.end(),
						[&](auto&& a_elem) { max = std::max(max, a_elem->name().length()); });
					return max;
				}();

				return "\t{:<"s + fmt::to_string(width) + "} 0x{:012X}"s;
			}();

			for (const auto& mod : a_modules) {
				a_log.critical(fmt::runtime(format), mod->name(), mod->address());
			}
		}

		void print_registers(spdlog::logger& a_log, const ::CONTEXT& a_context,
			std::span<const module_pointer> a_modules)
		{
			a_log.critical("REGISTERS:"sv);

			const std::array regs{
				std::make_pair("RAX"sv, a_context.Rax),
				std::make_pair("RCX"sv, a_context.Rcx),
				std::make_pair("RDX"sv, a_context.Rdx),
				std::make_pair("RBX"sv, a_context.Rbx),
				std::make_pair("RSP"sv, a_context.Rsp),
				std::make_pair("RBP"sv, a_context.Rbp),
				std::make_pair("RSI"sv, a_context.Rsi),
				std::make_pair("RDI"sv, a_context.Rdi),
				std::make_pair("R8"sv, a_context.R8),
				std::make_pair("R9"sv, a_context.R9),
				std::make_pair("R10"sv, a_context.R10),
				std::make_pair("R11"sv, a_context.R11),
				std::make_pair("R12"sv, a_context.R12),
				std::make_pair("R13"sv, a_context.R13),
				std::make_pair("R14"sv, a_context.R14),
				std::make_pair("R15"sv, a_context.R15),
			};

			std::array<std::size_t, regs.size()> todo{};
			for (std::size_t i = 0; i < regs.size(); ++i) {
				todo[i] = regs[i].second;
			}
			//const auto analysis = Introspection::analyze_data(todo, a_modules);
			//for (std::size_t i = 0; i < regs.size(); ++i) {
			//	const auto& [name, reg] = regs[i];
			//	a_log.critical("\t{:<3} 0x{:<16X} {}"sv, name, reg, analysis[i]);
			//}
		}

		void print_stack(spdlog::logger& a_log, const ::CONTEXT& a_context, std::span<const module_pointer> a_modules)
		{
			a_log.critical("STACK:"sv);

			const auto tib = reinterpret_cast<const ::NT_TIB*>(::NtCurrentTeb());
			const auto base = tib ? static_cast<const std::size_t*>(tib->StackBase) : nullptr;
			if (!base) {
				a_log.critical("\tFAILED TO READ TIB"sv);
			} else {
				const auto rsp = reinterpret_cast<const std::size_t*>(a_context.Rsp);
				std::span stack{ rsp, base };

				const auto format = [&]() {
					return "\t[RSP+{:<"s +
					       fmt::to_string(fmt::format("{:X}"sv, (stack.size() - 1) * sizeof(std::size_t)).length()) +
					       "X}] 0x{:<16X} {}"s;
				}();

				//constexpr std::size_t blockSize = 1000;
				//std::size_t idx = 0;
				//for (std::size_t off = 0; off < stack.size(); off += blockSize) {
					//const auto analysis = Introspection::analyze_data(
					//	stack.subspan(off, std::min<std::size_t>(stack.size() - off, blockSize)), a_modules);
					//for (const auto& data : analysis) {
					//	a_log.critical(fmt::runtime(format), idx * sizeof(std::size_t), stack[idx], data);
					//	++idx;
					//}
				//}
			}
		}

		void print_sysinfo(spdlog::logger& a_log)
		{
			a_log.critical("SYSTEM SPECS:"sv);

			const auto os = iware::system::OS_info();
			a_log.critical("\tOS: {} v{}.{}.{}"sv, os.full_name, os.major, os.minor, os.patch);

			a_log.critical("\tCPU: {} {}"sv, iware::cpu::vendor(), iware::cpu::model_name());

			const auto vendor = [](iware::gpu::vendor_t a_vendor) {
				using vendor_t = iware::gpu::vendor_t;
				switch (a_vendor) {
				case vendor_t::intel:
					return "Intel"sv;
				case vendor_t::amd:
					return "AMD"sv;
				case vendor_t::nvidia:
					return "Nvidia"sv;
				case vendor_t::microsoft:
					return "Microsoft"sv;
				case vendor_t::qualcomm:
					return "Qualcomm"sv;
				case vendor_t::unknown:
				default:
					return "Unknown"sv;
				}
			};

			const auto gpus = iware::gpu::device_properties();
			for (std::size_t i = 0; i < gpus.size(); ++i) {
				const auto& gpu = gpus[i];
				a_log.critical("\tGPU #{}: {} {}"sv, i + 1, vendor(gpu.vendor), gpu.name);
			}

			const auto gibibyte = [](std::uint64_t a_bytes) {
				constexpr double factor = 1024 * 1024 * 1024;
				return static_cast<double>(a_bytes) / factor;
			};

			const auto mem = iware::system::memory();
			a_log.critical("\tPHYSICAL MEMORY: {:.02f} GB/{:.02f} GB"sv,
				gibibyte(mem.physical_total - mem.physical_available), gibibyte(mem.physical_total));

			//https://forums.unrealengine.com/t/how-to-get-vram-usage-via-c/218627/2
			IDXGIFactory4* pFactory{};
			CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&pFactory);
			IDXGIAdapter3* adapter{};
			pFactory->EnumAdapters(0, reinterpret_cast<IDXGIAdapter**>(&adapter));
			DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
			adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);
			a_log.critical("\tGPU MEMORY: {:.02f}/{:.02f} GB"sv, gibibyte(videoMemoryInfo.CurrentUsage),
				gibibyte(videoMemoryInfo.Budget));

			// Detect VM
			if (VM::detect(VM::DEFAULT & ~(static_cast<std::bitset<89>>(VM::GAMARUE)))) {
				a_log.critical("\tDetected Virtual Machine: {} ({}%)"sv, VM::brand(VM::MULTIPLE), VM::percentage());
			}
		}

#undef SETTING_CASE

		std::int32_t __stdcall UnhandledExceptions(::EXCEPTION_POINTERS* a_exception) noexcept
		{
			try {
				const auto& debugConfig = Settings::GetSingleton()->GetDebug();
				if (debugConfig.waitForDebugger) {
					while (!IsDebuggerPresent()) {
					}
					if (IsDebuggerPresent())
						DebugBreak();
				}
				static std::mutex sync;
				const std::lock_guard l{ sync };

				const auto modules = Modules::get_loaded_modules();
				const std::span cmodules{ modules.begin(), modules.end() };
				const auto log = get_log();

				const auto print = [&](auto&& a_functor, std::string a_name = "") {
					log->critical(""sv);
					try {
						a_functor();
					} catch (const std::exception& e) {
						log->critical("\t{}:\t{}"sv, a_name, e.what());
					} catch (...) {
						log->critical("\t{}:\tERROR"sv, a_name);
					}
					log->flush();
				};

				const auto runtimeVer = REL::Module::get().version();
				log->critical("Skyrim {} v{}.{}.{}"sv, REL::Module::IsVR() ? "VR" : "SSE", runtimeVer[0], runtimeVer[1],
					runtimeVer[2]);
				log->critical("CrashLoggerSSE v{} {} {}"sv, SKSE::PluginDeclaration::GetSingleton()->GetVersion().string(), __DATE__, __TIME__);
				log->flush();

				print([&]() { print_exception(*log, *a_exception->ExceptionRecord, cmodules); }, "print_exception");
				print([&]() { print_sysinfo(*log); }, "print_sysinfo");
				if (REL::Module::IsVR())
					print([&]() { print_vrinfo(*log); }, "print_vrinfo");

				print(
					[&]() {
						const Callstack callstack{ *a_exception->ExceptionRecord };
						callstack.print(*log, cmodules);
					},
					"probable_callstack");

				print([&]() { print_registers(*log, *a_exception->ContextRecord, cmodules); }, "print_registers");
				print([&]() { print_stack(*log, *a_exception->ContextRecord, cmodules); }, "print_raw_stack");
				print([&]() { print_modules(*log, cmodules); }, "print_modules");
				//print([&]() { print_xse_plugins(*log, cmodules); }, "print_xse_plugins");
				//print([&]() { print_plugins(*log); }, "print_plugins");
			} catch (...) {}

			TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
		}

		std::int32_t _stdcall VectoredExceptions(::EXCEPTION_POINTERS*) noexcept
		{
			::SetUnhandledExceptionFilter(reinterpret_cast<::LPTOP_LEVEL_EXCEPTION_FILTER>(&UnhandledExceptions));
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}  // namespace

	void Install(std::string a_crashPath)
	{
		const auto success =
			::AddVectoredExceptionHandler(1, reinterpret_cast<::PVECTORED_EXCEPTION_HANDLER>(&VectoredExceptions));
		if (success == nullptr) {
			util::report_and_fail("failed to install vectored exception handler"sv);
		}
		logger::info("installed crash handlers"sv);
		if (!a_crashPath.empty()) {
			crashPath = a_crashPath;
			logger::info("Crash Logs will be written to {}", crashPath);
		}
	}
}  // namespace Crash
