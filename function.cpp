#include "function.hpp"
#include <libqalculate/ExpressionItem.h>
#include <libqalculate/MathStructure.h>

ReturnPlotFunction::ReturnPlotFunction() : MathFunction("plot", 4) {
    NumberArgument* arg = new NumberArgument();
    arg->setHandleVector(true);
    setArgumentDefinition(1, arg);
}

int ReturnPlotFunction::id() const {
    return 2690;
}

ReturnPlotFunction::ReturnPlotFunction(const ReturnPlotFunction* function) { set(function); };

ExpressionItem* ReturnPlotFunction::copy() const { return new ReturnPlotFunction(this); }

int ReturnPlotFunction::calculate(MathStructure& mstruct, const MathStructure& vargs, const EvaluationOptions& eo) {
    return 0;
}
