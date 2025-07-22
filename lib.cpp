#include <cstdlib>
#include <libqalculate/Calculator.h>
#include <libqalculate/ExpressionItem.h>
#include <libqalculate/Function.h>
#include <libqalculate/MathStructure.h>
#include <libqalculate/Number.h>
#include <libqalculate/Unit.h>
#include <libqalculate/Variable.h>
#include <libqalculate/includes.h>
#include <limits>
#include <lua5.1/lua.hpp>

#include "function.hpp"
#include "opttbl.hpp"

auto constexpr infini = std::numeric_limits<double>::infinity();

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

struct LCalculator {
    Calculator* calc;
    int table_index;
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

static LCalculator* check_Calculator(lua_State* L, int index) {
    return (LCalculator*)luaL_checkudata(L, index, "QalcCalculator");
}

static LMathStructure* check_MathStructure(lua_State* L, int index) {
    return (LMathStructure*)luaL_checkudata(L, index, "QalcExpression");
}

const std::string type_names[] = {
    "multiplication", "inverse",  "division", "addition", "negation",   "power",     "number",  "unit",
    "symbolic",       "function", "variable", "vector",   "bitand",     "bitor",     "bitxor",  "bitnot",
    "logand",         "logor",    "logxor",   "lognot",   "comparison", "undefined", "aborted", "datetime",
};

static double num_value(Number const& num) {
    if (num.isPlusInfinity()) {
        return infini;
    } else if (num.isMinusInfinity()) {
        return -infini;
    } else {
        return num.floatValue();
    }
}

static int push_MathStructureValue(lua_State* L, MathStructure const& expr, Calculator const* calc,
                                   PrintOptions const& opts) {
    if (expr.isNumber()) {
        Number num = expr.number();
        if (num.isComplex()) {
            lua_createtable(L, 3, 0);

            push_cppstr(L, "complex");
            lua_rawseti(L, -2, 1);

            lua_pushnumber(L, num_value(num.realPart()));
            lua_rawseti(L, -2, 2);

            lua_pushnumber(L, num_value(num.imaginaryPart()));
            lua_rawseti(L, -2, 3);
        } else {
            lua_pushnumber(L, num_value(num));
        }
        return 1;
    }

    if (expr.isVector()) {
        lua_createtable(L, expr.countChildren(), 0);

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
#include <lua5.1/lua.h>
#include <lua5.1/lauxlib.h>

int l_calc_new(lua_State* L) {
    LCalculator* udata = (LCalculator*)lua_newuserdata(L, sizeof(LCalculator));
    Calculator* calc = new Calculator;
    udata->calc = calc;

    calc->loadExchangeRates();
    calc->loadGlobalDefinitions();
    calc->loadLocalDefinitions();

    lua_newtable(L);
    int table_index = luaL_ref(L, LUA_REGISTRYINDEX);
    udata->table_index = table_index;

    // MathFunction* func = calc->addFunction(new ReturnPlotFunction());
    // calc->f_plot = NULL;

    luaL_getmetatable(L, "QalcCalculator");
    lua_setmetatable(L, -2);
    return 1;
}

int l_calc_gc(lua_State* L) {
    LCalculator* self = check_Calculator(L, 1);
    // FIXME: find out why this leads to a double free
    // as far as I can see, nothing in my code should be the cause
    // delete self->calc;
    return 0;
}

int l_calc_eval(lua_State* L) {
    LCalculator* self = check_Calculator(L, 1);
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

    res->calc = self->calc;
    res->expr = new MathStructure;
    res->parsed_src = new MathStructure;
    *res->expr = self->calc->calculate(expr, eopts, res->parsed_src);

    return 1 + push_messages(L, self->calc);
}

int l_calc_get_plot_values(lua_State* L) {
    LCalculator* self = check_Calculator(L, 1);

    std::string code = check_cppstr(L, 2);

    MathStructure min = check_MathValue(self->calc, L, 3);
    MathStructure max = check_MathValue(self->calc, L, 4);
    MathStructure step = check_MathValue(self->calc, L, 5);
    ParseOptions opts = check_ParseOptions(L, 6);

    MathStructure expr = self->calc->parse(code, opts);

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
    MathStructure x_var = self->calc->v_x;
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
    LCalculator* self = check_Calculator(L, 1);
    std::string name = check_cppstr(L, 2);

    Variable* var = self->calc->getActiveVariable(name);
    if (!var) {
        lua_pushnil(L);
    } else {
        LMathStructure* res = (LMathStructure*)lua_newuserdata(L, sizeof(LMathStructure));
        luaL_getmetatable(L, "QalcExpression");
        lua_setmetatable(L, -2);

        res->calc = self->calc;
        MathStructure* expr = new MathStructure(var);
        expr->eval();
        res->expr = expr;
        res->parsed_src = NULL;
    }

    return 1;
}

int l_calc_setvar(lua_State* L) {
    LCalculator* self = check_Calculator(L, 1);
    std::string name = check_cppstr(L, 2);
    MathStructure val = check_MathValue(self->calc, L, 3);

    Variable* var = self->calc->getVariable(name);
    if (!var) {
        KnownVariable* v = new KnownVariable();
        v->setName(name);
        v->set(val);
        self->calc->addVariable(v);
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
    LCalculator* self = check_Calculator(L, 1);
    bool variables = lua_toboolean(L, 2);
    bool functions = lua_toboolean(L, 3);
    if (variables)
        self->calc->resetVariables();
    if (functions)
        self->calc->resetFunctions();

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

    int rows = self->expr->rows();
    int cols = self->expr->columns();
    lua_createtable(L, rows, 0);
    for (int i = 0; i < rows; i++) {
        lua_createtable(L, cols, 0);
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
