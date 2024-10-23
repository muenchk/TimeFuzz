#include "LuaEngine.h"
#include "Oracle.h"
#include "Session.h"

bool Lua::RegisterThread(std::shared_ptr<Session> session)
{
	unsigned int threadid = std::this_thread::get_id()._Get_underlying_id();

	std::unique_lock<std::shared_mutex> guard(_statesLock);
	lua_State* luas = nullptr;
	try
	{
		luas = _states.at(threadid);
	}
	catch (std::out_of_range&)
	{
		luas = nullptr;
	}
	if (luas == nullptr) {
		luas = luaL_newstate();
		luaL_openlibs(luas);
		// apply session specific functions / globals
		session->_oracle->ApplyLuaCommands(luas);
		// add state to map
		_states.insert({ threadid, luas });
		return true;
	} else
		return false;
}

void Lua::UnregisterThread()
{
	unsigned int threadid = std::this_thread::get_id()._Get_underlying_id();

	std::unique_lock<std::shared_mutex> guard(_statesLock);
	lua_State* luas = nullptr;
	try {
		luas = _states.at(threadid);
	} catch (std::out_of_range&) {
		luas = nullptr;
	}
	if (luas != nullptr) {
		lua_close(luas);
		_states.erase(threadid);
	}
}

EnumType Lua::EvaluateOracle(std::function<EnumType(lua_State*, std::shared_ptr<Test>)> func, std::shared_ptr<Test> test, bool& stateerror)
{
	unsigned int threadid = std::this_thread::get_id()._Get_underlying_id();
	lua_State* luas = nullptr;
	std::shared_lock<std::shared_mutex> guard(_statesLock);
	try {
		luas = _states.at(threadid);
	} catch (std::out_of_range&) {
		luas = nullptr;
	}
	if (luas == nullptr) {
		stateerror = true;
		return Oracle::OracleResult::None;
	}
	// call the enclosed function
	return func(luas, test);
}

std::string Lua::GetCmdArgs(std::function<std::string(lua_State*, Test*)> func, std::shared_ptr<Test> test, bool& stateerror)
{
	unsigned int threadid = std::this_thread::get_id()._Get_underlying_id();
	lua_State* luas = nullptr;
	std::shared_lock<std::shared_mutex> guard(_statesLock);
	try {
		luas = _states.at(threadid);
	} catch (std::out_of_range&) {
		luas = nullptr;
	}
	if (luas == nullptr) {
		stateerror = true;
		return "";
	}
	// call the enclosed function
	return func(luas, test.get());
}

std::string Lua::GetCmdArgs(std::function<std::string(lua_State*, Test*)> func, Test* test, bool& stateerror)
{
	unsigned int threadid = std::this_thread::get_id()._Get_underlying_id();
	lua_State* luas = nullptr;
	std::shared_lock<std::shared_mutex> guard(_statesLock);
	try {
		luas = _states.at(threadid);
	} catch (std::out_of_range&) {
		luas = nullptr;
	}
	if (luas == nullptr) {
		stateerror = true;
		return "";
	}
	// call the enclosed function
	return func(luas, test);
}
