
#include <lua.hpp>
#include <unordered_map>
#include <thread>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <memory>

#include "Utility.h"

class Test;
class Session;

class Lua
{
public:
	static bool RegisterThread(std::shared_ptr<Session> session);
	static void UnregisterThread();

	static EnumType EvaluateOracle(std::function<EnumType(lua_State*, std::shared_ptr<Test>)> func, std::shared_ptr<Test> test, bool& stateerror);
	static std::string GetCmdArgs(std::function<std::string(lua_State*, Test*)> func, std::shared_ptr<Test> test, bool& stateerror);
	static std::string GetCmdArgs(std::function<std::string(lua_State*, Test*)> func, Test* test, bool& stateerror);

	static void DestroyAll();

private:
	inline static std::unordered_map<std::thread::id, lua_State*> _states;
	inline static std::shared_mutex _statesLock;
};
