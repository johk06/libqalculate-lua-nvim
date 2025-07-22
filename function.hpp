#include <libqalculate/ExpressionItem.h>
#include <libqalculate/Function.h>
#include <libqalculate/includes.h>

class ReturnPlotFunction : public MathFunction {
  public:
    ReturnPlotFunction();
    ReturnPlotFunction(const ReturnPlotFunction* function);
    ExpressionItem* copy() const;
    int id() const;

    int parse();

    int calculate(MathStructure& mstruct, const MathStructure& vargs, const EvaluationOptions& eo);
};
