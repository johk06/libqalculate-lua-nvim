#include "function.hpp"
#include <libqalculate/Calculator.h>
#include <libqalculate/ExpressionItem.h>
#include <libqalculate/Function.h>
#include <libqalculate/MathStructure.h>
#include <libqalculate/Number.h>
#include <libqalculate/includes.h>
#include <lua5.1/lua.hpp>

extern int QALC_CURRENT_PLOT_HANDLER;
extern lua_State* QALC_CURRENT_LUA_STATE;

ReturnPlotFunction::ReturnPlotFunction() : MathFunction("plot", 4) {
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

    setCondition("\\y < \\z");
    setCondition("\\a > 0");
}

int ReturnPlotFunction::id() const { return 2690; }

ReturnPlotFunction::ReturnPlotFunction(const ReturnPlotFunction* function) { set(function); };

ExpressionItem* ReturnPlotFunction::copy() const { return new ReturnPlotFunction(this); }

int ReturnPlotFunction::calculate(MathStructure& mstruct, const MathStructure& vargs, const EvaluationOptions& eo) {
    if (!QALC_CURRENT_PLOT_HANDLER) {
        return 0;
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

        if (!yvalue.isNumber()) {
            CALCULATOR->error(true, "Expression did not evaluate to number for x=%s", xvalue.print().c_str());
            return 0;
        }

        x_values.push_back(xvalue.number().floatValue());
        y_values.push_back(yvalue.number().floatValue());

        xvalue.calculateAdd(step, eo);
    }

    lua_State* L = QALC_CURRENT_LUA_STATE;
    lua_rawgeti(L, LUA_REGISTRYINDEX, QALC_CURRENT_PLOT_HANDLER);
    lua_newtable(L);
    for (size_t i = 0; i < x_values.size(); i++) {
        lua_pushnumber(L, x_values[i]);
        lua_rawseti(L, -2, i+1);
    }
    lua_newtable(L);
    for (size_t i = 0; i < y_values.size(); i++) {
        lua_pushnumber(L, y_values[i]);
        lua_rawseti(L, -3, i+1);
    }

    lua_call(L, 2, 0);

    mstruct.clear();
    return 1;
}
