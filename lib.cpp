#include <libqalculate/Calculator.h>
#include <libqalculate/ExpressionItem.h>
#include <libqalculate/MathStructure.h>
#include <libqalculate/Unit.h>
#include <libqalculate/Variable.h>
#include <libqalculate/includes.h>
#include <lua5.1/lua.hpp>

static inline void push_cppstr(lua_State* L, const std::string& str) { lua_pushlstring(L, str.data(), str.size()); }

static inline std::string check_cppstr(lua_State* L, int index) {
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

struct LMathStructure {
    MathStructure* expr;
    MathStructure* parsed_src; // nullable
    LCalculator* calc;
};

static LCalculator* check_Calculator(lua_State* L, int index) {
    return (LCalculator*)luaL_checkudata(L, index, "QalcCalculator");
}

static LMathStructure* check_MathStructure(lua_State* L, int index) {
    return (LMathStructure*)luaL_checkudata(L, index, "QalcExpression");
}

static inline void table_getboolean(lua_State* L, int index, bool* dest, char const* field) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_remove(L, -1);
    } else {
        *dest = lua_toboolean(L, -1);
    }
}

static inline void table_getinteger(lua_State* L, int index, int* dest, char const* field) {
    lua_getfield(L, index, field);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_remove(L, -1);
    } else {
        *dest = lua_tointeger(L, -1);
    }
}

static inline void table_getstring(lua_State* L, int index, std::string* dest, char const* field) {
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

static inline void table_getenum(lua_State* L, int index, int* dest, char const* field, EnumPair keys[]) {
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

EnumPair interval_display_options[] = {
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

EnumPair unicode_sign_options[] = {
    {"on", true},
    {"off", false},
    {"no-exponent", UNICODE_SIGNS_WITHOUT_EXPONENTS},
    {NULL},
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

    table_getboolean(L, index, &opts->do_assignment, "assign_variables");
    table_getinteger(L, index, &opts->print.base, "base");
    table_getboolean(L, index, &opts->print.negative_exponents, "negative_exponents");
    table_getboolean(L, index, &opts->print.spacious, "spacing");
    table_getboolean(L, index, &opts->print.excessive_parenthesis, "extra_parens");
    table_getinteger(L, index, &opts->print.min_decimals, "min_decimals");
    table_getinteger(L, index, &opts->print.max_decimals, "max_decimals");
    table_getenum(L, index, (int*)&opts->print.interval_display, "interval_display", interval_display_options);
    table_getenum(L, index, &opts->print.use_unicode_signs, "unicode", unicode_sign_options);

    return opts;
}

std::string type_names[] = {
    "multiplication", "inverse",  "division", "addition", "negation",   "power",     "number",  "unit",
    "symbolic",       "function", "variable", "vector",   "bitand",     "bitor",     "bitxor",  "bitnot",
    "logand",         "logor",    "logxor",   "lognot",   "comparison", "undefined", "aborted", "datetime",
};

static int push_MathStructureValue(lua_State* L, MathStructure const& expr, LCalculator const* calc) {
    if (expr.isNumber()) {
        lua_pushnumber(L, expr.number().floatValue());
        return 1;
    }

    if (expr.isVector()) {
        lua_newtable(L);

        for (int i = 0; i < expr.countChildren(); i++) {
            push_MathStructureValue(L, expr[i], calc);
            lua_rawseti(L, -2, i + 1);
        }

        return 1;
    }

    lua_newtable(L);
    int i = 0;
    push_cppstr(L, type_names[expr.type()]);
    lua_rawseti(L, -2, i++ + 1);

    if (expr.isUnit()) {
        push_cppstr(L, expr.unit()->singular());
        lua_rawseti(L, -2, i++ + 1);

        push_cppstr(L, expr.unit()->abbreviation());
        lua_rawseti(L, -2, i++ + 1);
    } else if (expr.isVariable() || expr.isSymbolic()) {
        push_cppstr(L, expr.print(calc->opts->print));
        lua_rawseti(L, -2, i++ + 1);
    } else {
        for (; (i - 1) < expr.countChildren(); i++) {
            push_MathStructureValue(L, expr[i - 1], calc);
            lua_rawseti(L, -2, i + 1);
        }
    }

    return 1;
}

static int message_to_vim_log_levels = 2;

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

    luaL_getmetatable(L, "QalcCalculator");
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

int l_calc_gc(lua_State* L) {
    LCalculator* self = check_Calculator(L, 1);
    if (self->calc) {
        delete self->calc;
        self->calc = NULL;
    }
    if (self->opts) {
        delete self->opts;
        self->opts = NULL;
    }
    return 0;
}

int l_calc_eval(lua_State* L) {
    LCalculator* self = check_Calculator(L, 1);
    auto expr = check_cppstr(L, 2);

    if (self->opts->do_assignment) {
        transform_expression_for_equals_save(expr, self->opts->parse);
    }

    LMathStructure* res = (LMathStructure*)lua_newuserdata(L, sizeof(LMathStructure));
    luaL_getmetatable(L, "QalcExpression");
    lua_setmetatable(L, -2);

    res->calc = self;
    res->expr = new MathStructure;
    res->parsed_src = new MathStructure;
    *res->expr = self->calc->calculate(expr, self->opts->eval, res->parsed_src);
    
    lua_newtable(L);
    int i = 1;
    while (auto msg = self->calc->message()) {
        lua_newtable(L);

        push_cppstr(L, msg->message());
        lua_rawseti(L, -2, 1);

        lua_pushinteger(L, msg->type() + message_to_vim_log_levels);
        lua_rawseti(L, -2, 2);

        self->calc->nextMessage();

        lua_rawseti(L, -2, i++);
    }

    return 2;
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

int l_expr_gc(lua_State* L) {
    LMathStructure* expr = check_MathStructure(L, 1);

    delete expr->expr;

    return 0;
}

int l_expr_tostring(lua_State* L) {
    LMathStructure* expr = check_MathStructure(L, 1);

    std::string s = expr->expr->print(expr->calc->opts->print);
    push_cppstr(L, s);

    return 1;
}

int l_expr_tolua(lua_State* L) {
    LMathStructure* expr = check_MathStructure(L, 1);
    MathStructure* res = expr->expr;

    return push_MathStructureValue(L, *res, expr->calc);
}

int l_expr_source(lua_State* L) {
    LMathStructure* expr = check_MathStructure(L, 1);
    if (expr->parsed_src) {
        push_cppstr(L, expr->parsed_src->print(expr->calc->opts->print));
    } else {
        lua_pushnil(L);
    }

    return 1;
}

int l_expr_type(lua_State* L) {
    LMathStructure* expr = check_MathStructure(L, 1);

    push_cppstr(L, expr->expr->isMatrix() ? "matrix" : type_names[expr->expr->type()]);
    return 1;
}

int l_expr_is_approximate(lua_State* L) {
    LMathStructure* expr = check_MathStructure(L, 1);

    lua_pushboolean(L, expr->expr->isApproximate());

    return 1;
}

int l_expr_index(lua_State* L) {
    LMathStructure* expr = check_MathStructure(L, 1);
    int type = lua_type(L, 2);
    if (type == LUA_TSTRING) {
        luaL_getmetatable(L, "QalcExpression");
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

int l_expr_length(lua_State* L) {
    LMathStructure* expr = check_MathStructure(L, 1);

    lua_pushinteger(L, expr->expr->countChildren());
    return 1;
}

int luaopen_qalculate_qalc(lua_State* L) {
    luaL_newmetatable(L, "QalcCalculator");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_calc_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, l_calc_eval);
    lua_setfield(L, -2, "eval");

    lua_pushcfunction(L, l_calc_reset);
    lua_setfield(L, -2, "reset");

    lua_pushcfunction(L, l_calc_set_opts);
    lua_setfield(L, -2, "set_options");

    luaL_newmetatable(L, "QalcExpression");
    lua_pushcfunction(L, l_expr_index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_expr_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, l_expr_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, l_expr_length);
    lua_setfield(L, -2, "__len");

    lua_pushcfunction(L, l_expr_tostring);
    lua_setfield(L, -2, "print");

    lua_pushcfunction(L, l_expr_tolua);
    lua_setfield(L, -2, "value");

    lua_pushcfunction(L, l_expr_type);
    lua_setfield(L, -2, "type");

    lua_pushcfunction(L, l_expr_source);
    lua_setfield(L, -2, "source");

    lua_pushcfunction(L, l_expr_is_approximate);
    lua_setfield(L, -2, "is_approximate");

    static luaL_Reg const library[] = {
        {"new", l_calc_new},
        {NULL, NULL},
    };
    luaL_register(L, "qalc", library);

    return 1;
}
}
