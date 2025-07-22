#include <lua5.1/lua.hpp>
#include <string>

static inline void push_cppstr(lua_State* L, const std::string& str) { lua_pushlstring(L, str.data(), str.size()); }

static inline std::string check_cppstr(lua_State* L, int index) {
    size_t len;
    const char* str = luaL_checklstring(L, index, &len);
    return std::string(str, len);
}

