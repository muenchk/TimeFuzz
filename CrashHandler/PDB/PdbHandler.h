#pragma once

#include <stdlib.h>
#include <string>
#include <filesystem>
#include <string_view>
#include <dia2.h>

namespace Crash
{
	namespace PDB
	{
		std::string processSymbol(IDiaSymbol* symbol, IDiaSession* pSession, const DWORD& rva, std::string_view& a_name, uintptr_t& a_offset, std::string& a_result);
		std::string pdb_details(std::string_view a_name, uintptr_t a_offset);
		void dump_symbols(bool exe = false);
		void dumpFileSymbols(const std::filesystem::path& path, int& retflag);

		const std::string_view sPluginPath = ".";
	}
}

