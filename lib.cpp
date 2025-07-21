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

struct LMathStructure {
    MathStructure* expr;
    MathStructure* parsed_src; // nullable
    Calculator* calc;
};

static MathStructure check_MathValue(Calculator* calc, lua_State* L, int index) {
    int type = lua_type(L, index);
    switch (type) {
    case LUA_TNUMBER:
        return Number(luaL_checknumber(L, index));
    case LUA_TSTRING:
        return calc->parse(check_cppstr(L, index));
    case LUA_TUSERDATA:
        return *((LMathStructure*)luaL_checkudata(L, index, "QalcExpression"))->expr;
        break;
    default:
        luaL_argerror(L, index, "Must be a number or string");
        return MathStructure();
    }
}

static Calculator* check_Calculator(lua_State* L, int index) {
    return (Calculator*)luaL_checkudata(L, index, "QalcCalculator");
}

static LMathStructure* check_MathStructure(lua_State* L, int index) {
    return (LMathStructure*)luaL_checkudata(L, index, "QalcExpression");
}

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
static PrintOptions check_PrintOptions(lua_State* L, int index) {
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

static ParseOptions check_ParseOptions(lua_State* L, int index) {
    ParseOptions ret = default_parse_options;
    if (lua_type(L, index) != LUA_TTABLE) {
        return ret;
    }

    opt_getbase(L, index, &ret.base);

    opt_getenum(L, index, (int*)&ret.parsing_mode, "mode", parsing_mode_options);

    return ret;
}

const std::string type_names[] = {
    "multiplication", "inverse",  "division", "addition", "negation",   "power",     "number",  "unit",
    "symbolic",       "function", "variable", "vector",   "bitand",     "bitor",     "bitxor",  "bitnot",
    "logand",         "logor",    "logxor",   "lognot",   "comparison", "undefined", "aborted", "datetime",
};

static int push_MathStructureValue(lua_State* L, MathStructure const& expr, Calculator const* calc,
                                   PrintOptions const& opts) {
    if (expr.isNumber()) {
        lua_pushnumber(L, expr.number().floatValue());
        return 1;
    }

    if (expr.isVector()) {
        lua_newtable(L);

        for (int i = 0; i < expr.countChildren(); i++) {
            push_MathStructureValue(L, expr[i], calc, opts);
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
        push_cppstr(L, expr.print(opts));
        lua_rawseti(L, -2, i++ + 1);
    } else {
        for (; (i - 1) < expr.countChildren(); i++) {
            push_MathStructureValue(L, expr[i - 1], calc, opts);
            lua_rawseti(L, -2, i + 1);
        }
    }

    return 1;
}

#define MESSAGE_TO_VIM_LOG_LEVELS 2
static int push_messages(lua_State* L, Calculator* calc) {
    if (!calc->message()) {
        return 0;
    }

    lua_newtable(L);
    int i = 1;
    while (auto msg = calc->message()) {
        lua_newtable(L);

        push_cppstr(L, msg->message());
        lua_rawseti(L, -2, 1);

        lua_pushinteger(L, msg->type() + MESSAGE_TO_VIM_LOG_LEVELS);
        lua_rawseti(L, -2, 2);

        calc->nextMessage();

        lua_rawseti(L, -2, i++);
    }

    return 1;
}

extern "C" {
#include <lua5.1/lauxlib.h>
#include <lua5.1/lua.h>

int l_calc_new(lua_State* L) {
    void* mem = lua_newuserdata(L, sizeof(Calculator));
    Calculator* calc = new (mem) Calculator;

    calc->loadExchangeRates();
    calc->loadGlobalDefinitions();
    calc->loadLocalDefinitions();

    luaL_getmetatable(L, "QalcCalculator");
    lua_setmetatable(L, -2);
    return 1;
}

int l_calc_gc(lua_State* L) {
    Calculator* calc = check_Calculator(L, 1);
    calc->~Calculator();
    return 0;
}

int l_calc_eval(lua_State* L) {
    Calculator* calc = check_Calculator(L, 1);
    auto expr = check_cppstr(L, 2);
    ParseOptions opts = check_ParseOptions(L, 3);
    EvaluationOptions eopts = default_evaluation_options;
    eopts.parse_options = opts;

    bool do_assignment = lua_toboolean(L, 4);
    if (do_assignment) {
        transform_expression_for_equals_save(expr, opts);
    }

    LMathStructure* res = (LMathStructure*)lua_newuserdata(L, sizeof(LMathStructure));
    luaL_getmetatable(L, "QalcExpression");
    lua_setmetatable(L, -2);

    res->calc = calc;
    res->expr = new MathStructure;
    res->parsed_src = new MathStructure;
    *res->expr = calc->calculate(expr, eopts, res->parsed_src);

    return 1 + push_messages(L, calc);
}

int l_calc_get_plot_values(lua_State* L) {
    Calculator* calc = check_Calculator(L, 1);

    std::string code = check_cppstr(L, 2);

    MathStructure min = check_MathValue(calc, L, 3);
    MathStructure max = check_MathValue(calc, L, 4);
    MathStructure step = check_MathValue(calc, L, 5);
    ParseOptions opts = check_ParseOptions(L, 6);

    MathStructure expr = calc->parse(code, opts);

    min.eval();
    max.eval();
    step.eval();
    if (!step.isNumber() || step.number().isNegative()) {
        luaL_error(L, "Step must be a number > 0");
    }

    if (!min.isNumber() || !max.isNumber() || !min.number().isLessThan(max.number())) {
        luaL_error(L, "Min must be a number < Max");
    }

    MathStructure x_value(min);
    MathStructure x_var = calc->v_x;
    lua_newtable(L);

    MathStructure y_value;

    double maxvalue = max.number().floatValue();
    size_t i = 1;

    lua_newtable(L);
    while (x_value.number().isLessThanOrEqualTo(maxvalue)) {
        y_value = expr;
        y_value.replace(x_var, x_value);
        y_value.eval();

        if (!y_value.isNumber()) {
            luaL_error(L, "Expression did not evaluate to number for x=%f", x_value.number().floatValue());
        }

        lua_pushnumber(L, y_value.number().floatValue());
        lua_rawseti(L, -2, i++);

        x_value.calculateAdd(step, default_evaluation_options);
    }

    return 1;
}

int l_calc_getvar(lua_State* L) {
    Calculator* self = check_Calculator(L, 1);
    std::string name = check_cppstr(L, 2);

    Variable* var = self->getActiveVariable(name);
    if (!var) {
        lua_pushnil(L);
    } else {
        LMathStructure* res = (LMathStructure*)lua_newuserdata(L, sizeof(LMathStructure));
        luaL_getmetatable(L, "QalcExpression");
        lua_setmetatable(L, -2);

        res->calc = self;
        MathStructure* expr = new MathStructure(var);
        expr->eval();
        res->expr = expr;
        res->parsed_src = NULL;
    }

    return 1;
}

int l_calc_setvar(lua_State* L) {
    Calculator* self = check_Calculator(L, 1);
    std::string name = check_cppstr(L, 2);
    MathStructure val = check_MathValue(self, L, 3);

    Variable* var = self->getVariable(name);
    if (!var) {
        KnownVariable* v = new KnownVariable();
        v->setName(name);
        v->set(val);
        self->addVariable(v);
        lua_pushboolean(L, true);
    } else if (var->isKnown()) {
        KnownVariable* v = (KnownVariable*)var;
        v->set(val);
        lua_pushboolean(L, true);
    }

    lua_pushboolean(L, false);
    return 1;
}

int l_calc_reset(lua_State* L) {
    Calculator* self = check_Calculator(L, 1);
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

    if (expr->expr != NULL) {
        delete expr->expr;
        expr->expr = NULL;
    }

    if (expr->parsed_src != NULL) {
        delete expr->parsed_src;
        expr->expr = NULL;
    }

    return 0;
}

int l_expr_tostring(lua_State* L) {
    LMathStructure* self = check_MathStructure(L, 1);
    PrintOptions opts = check_PrintOptions(L, 2);

    std::string s = self->expr->print(opts);
    push_cppstr(L, s);
    return 1 + push_messages(L, self->calc);
}

int l_expr_tolua(lua_State* L) {
    LMathStructure* self = check_MathStructure(L, 1);
    PrintOptions opts = check_PrintOptions(L, 2);
    MathStructure* res = self->expr;

    return push_MathStructureValue(L, *res, self->calc, opts);
}

int l_expr_source(lua_State* L) {
    LMathStructure* self = check_MathStructure(L, 1);
    PrintOptions opts = check_PrintOptions(L, 2);

    if (self->parsed_src) {
        push_cppstr(L, self->parsed_src->print(opts));
    } else {
        lua_pushnil(L);
    }

    return 1;
}

int l_expr_type(lua_State* L) {
    LMathStructure* self = check_MathStructure(L, 1);

    push_cppstr(L, self->expr->isMatrix() ? "matrix" : type_names[self->expr->type()]);
    return 1;
}

int l_expr_is_approximate(lua_State* L) {
    LMathStructure* self = check_MathStructure(L, 1);

    lua_pushboolean(L, self->expr->isApproximate());

    return 1;
}

int l_expr_as_matrix(lua_State* L) {
    LMathStructure* self = check_MathStructure(L, 1);
    if (!self->expr->isMatrix()) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    int rows = self->expr->rows();
    int cols = self->expr->columns();
    for (int i = 0; i < rows; i++) {
        lua_newtable(L);
        for (int j = 0; j < cols; j++) {
            auto em = self->expr->getElement(i + 1, j + 1);
            if (em) {
                LMathStructure* res = (LMathStructure*)lua_newuserdata(L, sizeof(LMathStructure));
                luaL_getmetatable(L, "QalcExpression");
                lua_setmetatable(L, -2);

                res->calc = self->calc;
                res->expr = new MathStructure(*em);
                lua_rawseti(L, -2, j + 1);
            }
        }
        lua_rawseti(L, -2, i + 1);
    }

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

    lua_pushcfunction(L, l_calc_get_plot_values);
    lua_setfield(L, -2, "plot");

    lua_pushcfunction(L, l_calc_getvar);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, l_calc_setvar);
    lua_setfield(L, -2, "set");

    lua_pushcfunction(L, l_calc_reset);
    lua_setfield(L, -2, "reset");

    luaL_newmetatable(L, "QalcExpression");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_expr_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, l_expr_tostring);
    lua_setfield(L, -2, "__tostring");

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

    lua_pushcfunction(L, l_expr_as_matrix);
    lua_setfield(L, -2, "as_matrix");

    static luaL_Reg const library[] = {
        {"new", l_calc_new},
        {NULL, NULL},
    };
    luaL_register(L, "qalc", library);

    return 1;
}
}
