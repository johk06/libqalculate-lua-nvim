#include "function.hpp"
#include "util.hpp"

#include <libqalculate/Calculator.h>
#include <libqalculate/ExpressionItem.h>
#include <libqalculate/Function.h>
#include <libqalculate/MathStructure.h>
#include <libqalculate/Number.h>
#include <libqalculate/includes.h>
#include <libqalculate/util.h>
#include <limits>
#include <lua.h>
#include <lua5.1/lua.hpp>
#include <optional>
#include <string_view>

using std::string;

extern int QALC_CURRENT_PLOT_HANDLER;
extern lua_State* QALC_CURRENT_LUA_STATE;

ReturnPlotFunction::ReturnPlotFunction() : MathFunction("plot", 4, -1) {
    NumberArgument* start = new NumberArgument();
    start->setComplexAllowed(false);
    start->setHandleVector(false);
    setArgumentDefinition(2, start);

    NumberArgument* stop = new NumberArgument();
    stop->setComplexAllowed(false);
    stop->setHandleVector(false);
    setArgumentDefinition(3, stop);

    NumberArgument* step = new NumberArgument();
    step->setComplexAllowed(false);
    step->setHandleVector(false);
    setArgumentDefinition(4, step);

    setArgumentDefinition(5, new TextArgument());

    setCondition("\\y < \\z");
    setCondition("\\a > 0");
}

int ReturnPlotFunction::id() const { return 2690; }

ReturnPlotFunction::ReturnPlotFunction(const ReturnPlotFunction* function) { set(function); };

ExpressionItem* ReturnPlotFunction::copy() const { return new ReturnPlotFunction(this); }

std::string_view trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
        sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))
        sv.remove_suffix(1);
    return sv;
}

std::pair<std::string_view, std::string_view> split_var(std::string_view str) {
    size_t eq = str.find("=");
    if (eq == std::string_view::npos) {
        return {trim(str), {}};
    }

    return {trim(str.substr(0, eq)), trim(str.substr(eq + 1))};
}

int ReturnPlotFunction::calculate(MathStructure& mstruct, const MathStructure& vargs, const EvaluationOptions& eo) {
    if (!QALC_CURRENT_PLOT_HANDLER) {
        return 0;
    }

    std::optional<double> step_size;
    std::optional<string> xfmt;
    std::optional<string> line_type;
    std::optional<std::pair<double, double>> y_range;
    std::vector<string> extra_directives;

    for (size_t i = 4; i < vargs.size(); i++) {
        string meta = vargs[i].symbol();
        auto [name, value] = split_var(meta);
        bool has_value = value.size() > 0;
        if (name == "step" && has_value) {
            MathStructure display_step = CALCULATOR->parse(string(value));
            display_step.eval(eo);
            if (display_step.isNumber()) {
                step_size = display_step.number().floatValue();
            } else {
                CALCULATOR->error(false, "step= value must be a number");
            }
        } else if (name == "fmt-x" && has_value) {
            xfmt = value;
        } else if (name == "type" && has_value) {
            line_type = value;
        } else if (name == "range" && has_value) {
            MathStructure range = CALCULATOR->parse(string(value));
            range.eval(eo);
            if (!range.isVector() || range.countChildren() != 2 || !range[0].isNumber() || !range[1].isNumber()) {
                CALCULATOR->error(false, "range should be a vector of two numbers");
            } else {
                y_range = {
                    range[0].number().floatValue(),
                    range[1].number().floatValue(),
                };
            }
        } else if (name == "add" && has_value) {
            extra_directives.push_back(string(value));
        }
    }

    MathStructure expr = vargs[0], xvar = CALCULATOR->getVariableById(VARIABLE_ID_X);
    MathStructure xvalue = vargs[1], max = vargs[2], step = vargs[3];

    mstruct.clearVector();
    MathStructure yvalue;

    std::vector<double> x_values, y_values;
    while (xvalue.number().isLessThanOrEqualTo(max.number())) {
        yvalue = expr;
        yvalue.replace(xvar, xvalue);
        yvalue.eval();

        x_values.push_back(xvalue.number().floatValue());

        if (yvalue.isUndefined() || yvalue.isInfinite()) {
            y_values.push_back(std::numeric_limits<double>::quiet_NaN());
        } else if (yvalue.isNumber()) {
            y_values.push_back(yvalue.number().floatValue());
        } else {
            y_values.push_back(std::numeric_limits<double>::quiet_NaN());
        }

        xvalue.calculateAdd(step, eo);
    }

    lua_State* L = QALC_CURRENT_LUA_STATE;
    lua_rawgeti(L, LUA_REGISTRYINDEX, QALC_CURRENT_PLOT_HANDLER);
    lua_newtable(L);
    for (size_t i = 0; i < x_values.size(); i++) {
        lua_pushnumber(L, x_values[i]);
        lua_rawseti(L, -2, i + 1);
    }

    lua_newtable(L);
    for (size_t i = 0; i < y_values.size(); i++) {
        lua_pushnumber(L, y_values[i]);
        lua_rawseti(L, -2, i + 1);
    }

    lua_newtable(L);
    if (step_size.has_value()) {
        lua_pushnumber(L, step_size.value());
        lua_setfield(L, -2, "step");
    }
    if (xfmt.has_value()) {
        push_cppstr(L, xfmt.value());
        lua_setfield(L, -2, "xfmt");
    }
    if (line_type.has_value()) {
        push_cppstr(L, line_type.value());
        lua_setfield(L, -2, "type");
    }
    if (y_range.has_value()) {
        lua_newtable(L);
        lua_pushnumber(L, y_range.value().first);
        lua_rawseti(L, -2, 1);
        lua_pushnumber(L, y_range.value().second);
        lua_rawseti(L, -2, 2);
        lua_setfield(L, -2, "range");
    }
    lua_newtable(L);
    for (size_t i = 0; i < extra_directives.size(); i++) {
        push_cppstr(L, extra_directives[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "extra");

    lua_call(L, 3, 0);

    mstruct.clear();
    return 1;
}
