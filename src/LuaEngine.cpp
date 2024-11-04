#include "LuaEngine.h"
#include "Oracle.h"
#include "SessionData.h"
#include "Logging.h"
#include "Input.h"

bool Lua::RegisterThread(std::shared_ptr<SessionData> session)
{
	logdebug("Register");
	auto threadid = std::this_thread::get_id();

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
		// register lua functions
		Input::RegisterLuaFunctions(luas);
		// apply session specific functions / globals
		session->_oracle->ApplyLuaCommands(luas);
		// add state to map
		_states.insert({ threadid, luas });
		logdebug("Register success {}", uint64_t(luas));
		return true;
	} else {
		logdebug("already registered");
		return false;
	}
}

void Lua::UnregisterThread()
{
	logdebug("Unregister");
	auto threadid = std::this_thread::get_id();
	logdebug("Unregister 1");

	std::unique_lock<std::shared_mutex> guard(_statesLock);
	logdebug("Unregister 2");
	lua_State* luas = nullptr;
	try {
		logdebug("Unregister 3");
		luas = _states.at(threadid);
	} catch (std::out_of_range&) {
		logdebug("Unregister 4");
		luas = nullptr;
	}
	logdebug("Unregister 5");
	if (luas != nullptr) {
		logdebug("Unregister 6 {}", uint64_t(luas));
		lua_close(luas);
		logdebug("Unregister 7");
		_states.erase(threadid);
	}
	logdebug("Unregistered");
}

EnumType Lua::EvaluateOracle(std::function<EnumType(lua_State*, std::shared_ptr<Test>)> func, std::shared_ptr<Test> test, bool& stateerror)
{
	auto threadid = std::this_thread::get_id();
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

std::string Lua::GetCmdArgs(std::function<std::string(lua_State*, Test*, bool)> func, std::shared_ptr<Test> test, bool& stateerror, bool replay)
{
	auto threadid = std::this_thread::get_id();
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
	return func(luas, test.get(), replay);
}

std::string Lua::GetCmdArgs(std::function<std::string(lua_State*, Test*, bool)> func, Test* test, bool& stateerror, bool replay)
{
	auto threadid = std::this_thread::get_id();
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
	return func(luas, test, replay);
}

void Lua::DestroyAll()
{
	std::unique_lock<std::shared_mutex> guard(_statesLock);
	for (auto [id, luas] : _states)
	{
		lua_close(luas);
	}
	_states.clear();
}
