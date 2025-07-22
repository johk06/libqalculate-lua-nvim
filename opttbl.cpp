#include <lua5.1/lua.hpp>
#include <libqalculate/includes.h>

static void opt_getbase(lua_State* L, int index, int* dest) {
    lua_getfield(L, index, "base");
    int type = lua_type(L, -1);
    if (type == LUA_TNUMBER) {
        *dest = lua_tointeger(L, -1);
    } else if (type == LUA_TSTRING) {
        size_t len;
        const char* buf = lua_tolstring(L, -1, &len);
        std::string str(buf, len);

        if (str == "roman") {
            *dest = BASE_ROMAN_NUMERALS;
        } else if (str == "time") {
            *dest = BASE_TIME;
        }
    }
}

static inline void opt_getboolean(lua_State* L, int index, bool* dest, char const* field) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_remove(L, -1);
    } else {
        *dest = lua_toboolean(L, -1);
    }
}

static inline void opt_getinteger(lua_State* L, int index, int* dest, char const* field) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_remove(L, -1);
    } else {
        *dest = lua_tointeger(L, -1);
    }
}

static inline void opt_getstring(lua_State* L, int index, std::string* dest, char const* field) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) != LUA_TSTRING) {
        lua_remove(L, -1);
    } else {
        size_t len;
        const char* buf = lua_tolstring(L, -1, &len);
        *dest = std::string(buf, len);
    }
}

struct EnumPair {
    const char* key;
    int value;
};

static inline void opt_getenum(lua_State* L, int index, int* dest, char const* field, EnumPair const keys[]) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) != LUA_TSTRING) {
        lua_remove(L, -1);
    } else {
        const char* key = lua_tostring(L, -1);

        int i = 0;
        while (keys[i].key) {
            if (strcmp(keys[i].key, key) == 0) {
                *dest = keys[i].value;
            }
            i++;
        }
    }
}

const EnumPair interval_display_options[] = {
    {"adaptive", -1},
    {"significant", INTERVAL_DISPLAY_SIGNIFICANT_DIGITS},
    {"interval", INTERVAL_DISPLAY_INTERVAL},
    {"plusminus", INTERVAL_DISPLAY_PLUSMINUS},
    {"midpoint", INTERVAL_DISPLAY_MIDPOINT},
    {"lower", INTERVAL_DISPLAY_LOWER},
    {"upper", INTERVAL_DISPLAY_UPPER},
    {"concise", INTERVAL_DISPLAY_CONCISE},
    {"relative", INTERVAL_DISPLAY_RELATIVE},
    {NULL},
};
const EnumPair unicode_sign_options[] = {
    {"on", true},
    {"off", false},
    {"no-exponent", UNICODE_SIGNS_WITHOUT_EXPONENTS},
    {NULL},
};

PrintOptions check_PrintOptions(lua_State* L, int index) {
    PrintOptions ret = default_print_options;
    if (lua_type(L, index) != LUA_TTABLE) {
        return ret;
    }

    opt_getbase(L, index, &ret.base);
    opt_getinteger(L, index, &ret.min_decimals, "min_decimals");
    opt_getinteger(L, index, &ret.max_decimals, "max_decimals");

    opt_getboolean(L, index, &ret.abbreviate_names, "abbreviate_names");
    opt_getboolean(L, index, &ret.negative_exponents, "negative_exponents");
    opt_getboolean(L, index, &ret.spacious, "spacious");
    opt_getboolean(L, index, &ret.excessive_parenthesis, "excessive_parenthesis");

    opt_getenum(L, index, &ret.use_unicode_signs, "unicode", unicode_sign_options);
    opt_getenum(L, index, (int*)&ret.interval_display, "interval_display", interval_display_options);

    return ret;
};

EnumPair parsing_mode_options[] = {
    {"default", PARSING_MODE_ADAPTIVE},
    {"rpn", PARSING_MODE_RPN},
    {NULL},
};

ParseOptions check_ParseOptions(lua_State* L, int index) {
    ParseOptions ret = default_parse_options;
    if (lua_type(L, index) != LUA_TTABLE) {
        return ret;
    }

    opt_getbase(L, index, &ret.base);

    opt_getenum(L, index, (int*)&ret.parsing_mode, "mode", parsing_mode_options);

    return ret;
}
