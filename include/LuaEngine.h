
#include <lua.hpp>
#include <unordered_map>
#include <thread>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <memory>

#include "Utility.h"
#include "Types.h"

class Test;
class SessionData;

class Lua
{
public:
	static bool RegisterThread(Types::shared_ptr<SessionData> session);
	static void UnregisterThread();

	static EnumType EvaluateOracle(std::function<EnumType(lua_State*, Types::shared_ptr<Test>)> func, Types::shared_ptr<Test> test, bool& stateerror);
	static std::string GetCmdArgs(std::function<std::string(lua_State*, Test*, bool)> func, Types::shared_ptr<Test> test, bool& stateerror, bool replay);
	static std::string GetCmdArgs(std::function<std::string(lua_State*, Test*, bool)> func, Test* test, bool& stateerror, bool replay);
	static std::string GetScriptArgs(std::function<std::string(lua_State*, Test*)> func, Types::shared_ptr<Test> test, bool& stateerror);
	static std::string GetScriptArgs(std::function<std::string(lua_State*, Test*)> func, Test* test, bool& stateerror);

	static void DestroyAll();

private:
	inline static std::unordered_map<std::thread::id, lua_State*> _states;
	inline static std::shared_mutex _statesLock;
};
