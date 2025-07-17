#include <libqalculate/Calculator.h>
#include <libqalculate/MathStructure.h>
#include <libqalculate/Unit.h>
#include <libqalculate/Variable.h>
#include <libqalculate/includes.h>
#include <lua5.1/lua.hpp>

static inline void push_cppstr(lua_State* L, const std::string& str) { lua_pushlstring(L, str.data(), str.size()); }

static std::string check_cppstr(lua_State* L, int index) {
    size_t len;
    const char* str = luaL_checklstring(L, index, &len);
    return std::string(str, len);
}

struct Options {
    bool do_assignment;
    struct PrintOptions print;
    struct ParseOptions parse;
    struct EvaluationOptions eval;
};

struct LCalculator {
    Calculator* calc;
    Options* opts;
};

static LCalculator* check_Calculator(lua_State* L, int index) {
    return (LCalculator*)luaL_checkudata(L, index, "QalculatorMT");
}

static inline bool table_toboolean(lua_State* L, int index, char const* field, bool def) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_remove(L, -1);
        return def;
    } else {
        return lua_toboolean(L, -1);
    }
}

static inline int table_tointeger(lua_State* L, int index, char const* field, int def) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_remove(L, -1);
        return def;
    } else {
        return lua_tointeger(L, -1);
    }
}

static inline std::string table_tostring(lua_State* L, int index, char const* field, std::string const& def) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) != LUA_TSTRING) {
        lua_remove(L, -1);
        return def;
    } else {
        size_t len;
        const char* buf = lua_tolstring(L, -1, &len);
        return std::string(buf, len);
    }
}

struct EnumPair {
    const char* key;
    int value;
};

static inline int table_toenum(lua_State* L, int index, char const* field, EnumPair keys[], int def) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) != LUA_TSTRING) {
        lua_remove(L, -1);
        return def;
    } else {
        const char* key = lua_tostring(L, -1);

        int i = 0;
        while (keys[i].key) {
            if (strcmp(keys[i].key, key) == 0) {
                return keys[i].value;
            }
            i++;
        }
    }

    return def;
}

EnumPair IntDisplaySwitch[] = {
    {"adaptive", -1},
    {"significant", INTERVAL_DISPLAY_SIGNIFICANT_DIGITS},
    {"interval", INTERVAL_DISPLAY_INTERVAL},
    {"plusminus", INTERVAL_DISPLAY_PLUSMINUS},
    {"midpoint", INTERVAL_DISPLAY_MIDPOINT},
    {"lower", INTERVAL_DISPLAY_LOWER},
    {"upper", INTERVAL_DISPLAY_UPPER},
    {"concise", INTERVAL_DISPLAY_CONCISE},
    {"relative", INTERVAL_DISPLAY_RELATIVE},
    {NULL, 0},
};

static Options* check_Options(lua_State* L, int index) {
    Options* opts = new Options();
    opts->print = default_print_options;
    opts->print.use_unicode_signs = true;

    opts->eval = default_evaluation_options;

    opts->parse = default_parse_options;

    if (lua_type(L, index) != LUA_TTABLE) {
        return opts;
    }

    opts->do_assignment = table_toboolean(L, index, "assing_variables", true);
    opts->print.base = table_tointeger(L, index, "base", 10);
    opts->print.use_unicode_signs = table_toboolean(L, index, "unicode", true);
    opts->print.negative_exponents = table_toboolean(L, index, "negative_exponents", false);
    opts->print.spacious = table_toboolean(L, index, "spacing", true);
    opts->print.excessive_parenthesis = table_toboolean(L, index, "extra_parens", false);
    opts->print.min_decimals = table_tointeger(L, index, "min_decimals", 0);
    opts->print.max_decimals = table_tointeger(L, index, "max_decimals", -1);
    opts->print.interval_display =
        (IntervalDisplay)table_toenum(L, index, "interval_display", IntDisplaySwitch, INTERVAL_DISPLAY_PLUSMINUS);

    return opts;
}

static int MessageToVimLogLevels[] = {
    2,
    3,
    4,
};

static void push_MathStructure(lua_State* L, MathStructure const& st, struct Options const* opts) {
    lua_newtable(L);

    if (st.isNumber()) {
        auto as_number = st.number().floatValue();
        lua_pushnumber(L, as_number);
        lua_setfield(L, -2, "number");
    }

    if (st.isMatrix()) {
        lua_newtable(L);
        for (int i = 0; i < st.countChildren(); i++) {
            push_MathStructure(L, st[i], opts);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, "matrix");
    } else if (st.isVector()) {
        lua_newtable(L);
        for (int i = 0; i < st.countChildren(); i++) {
            push_MathStructure(L, st[i], opts);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, "vector");
    }

    push_cppstr(L, st.print(opts->print));
    lua_setfield(L, -2, "string");
}

extern "C" {
#include <lua5.1/lauxlib.h>
#include <lua5.1/lua.h>

int l_calc_new(lua_State* L) {
    Calculator* calc = new Calculator();
    Options* opts = check_Options(L, 1);
    LCalculator* udata = (LCalculator*)lua_newuserdata(L, sizeof(LCalculator));
    udata->calc = calc;
    udata->opts = opts;

    calc->loadExchangeRates();
    calc->loadGlobalDefinitions();
    calc->loadLocalDefinitions();

    luaL_getmetatable(L, "QalculatorMT");
    lua_setmetatable(L, -2);
    return 1;
}

int l_calc_set_opts(lua_State* L) {
    LCalculator* calc = check_Calculator(L, 1);
    Options* opts = check_Options(L, 2);
    delete calc->opts;
    calc->opts = opts;

    return 0;
}

// TODO/FIXME: fix garbage collection double frees/crashes
int l_calc_gc(lua_State* L) {
    LCalculator* self = check_Calculator(L, 1);
    if (self->calc) {
        delete self->calc;
        // self->calc = nullptr;
    }
    if (self->opts) {
        delete self->opts;
        // self->opts = nullptr;
    }
    return 0;
}

int l_calc_eval(lua_State* L) {
    LCalculator* self = check_Calculator(L, 1);
    auto expr = check_cppstr(L, 2);

    if (self->opts->do_assignment) {
        transform_expression_for_equals_save(expr, self->opts->parse);
    }

    MathStructure parsed;
    MathStructure res = self->calc->calculate(expr, self->opts->eval, &parsed);
    push_MathStructure(L, res, self->opts);

    push_cppstr(L, parsed.print(self->opts->print));
    lua_setfield(L, -2, "parsed");

    lua_newtable(L);
    int i = 1;
    while (auto msg = self->calc->message()) {
        std::string text = msg->message();
        MessageType type = msg->type();
        lua_newtable(L);

        push_cppstr(L, text);
        lua_rawseti(L, -2, 1);
        lua_pushinteger(L, MessageToVimLogLevels[type]);
        lua_rawseti(L, -2, 2);

        lua_rawseti(L, -2, i++);

        self->calc->nextMessage();
    }
    lua_setfield(L, -2, "messages");
    return 1;
}

int l_calc_reset(lua_State* L) {
    Calculator* self = check_Calculator(L, 1)->calc;
    bool variables = lua_toboolean(L, 2);
    bool functions = lua_toboolean(L, 3);
    if (variables)
        self->resetVariables();
    if (functions)
        self->resetFunctions();

    return 0;
}

int luaopen_qalc(lua_State* L) {
    luaL_newmetatable(L, "QalculatorMT");

    lua_pushcfunction(L, l_calc_gc);
    lua_setfield(L, -2, "__gc");

    // Index with self
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_calc_eval);
    lua_setfield(L, -2, "eval");

    lua_pushcfunction(L, l_calc_reset);
    lua_setfield(L, -2, "reset");

    lua_pushcfunction(L, l_calc_set_opts);
    lua_setfield(L, -2, "set_options");

    static luaL_Reg const library[] = {
        {"new", l_calc_new},
        {NULL, NULL},
    };
    luaL_register(L, "qalc", library);

    return 1;
}
}
