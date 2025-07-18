--- @meta
error("Cannot require a meta file")

---@class QalcOptions
---@field assing_variables boolean?
---@field base integer?
---@field negative_exponents boolean?
---@field spacing boolean?
---@field extra_parens boolean?
---@field min_decimals boolean?
---@field max_decimals boolean?
---@field interval_display "adaptive"| "significant"| "interval"| "plusminus"| "midpoint"| "lower"| "upper"| "concise"| "relative"?
---@field unicode "on"|"off"|"no-unit"?

---@class Qalculate
---@field new fun(opts: QalcOptions): QalcCalculator

---@class QalcCalculator
---@field eval fun(self: QalcCalculator, expr: string): QalcExpression, {[1]: string, [2]: vim.log.levels}[]
---@field reset fun(self: QalcCalculator, variables: boolean, functions: boolean)
---@field set_options fun(self: QalcCalculator, opts: QalcOptions)

---@class QalcExpression
---@field print fun(self: QalcExpression): string
---@field value fun(self: QalcExpression): QalcValue
---@field type fun(self: QalcExpression): QalcType
---@field source fun(self: QalcExpression): string?
---@field is_approximate fun(self: QalcExpression): boolean

--- Regular Number | Vector | Matrix | Expression
---@alias QalcValue number|number[]|number[][]|{[1]: QalcType, [integer]: QalcValue}

---@alias QalcType 
---|"multiplication"
---|"inverse"
---|"division"
---|"addition"
---|"negation"
---|"power"
---|"number"
---|"unit"
---|"symbolic"
---|"function"
---|"variable"
---|"vector"
---|"bitand"
---|"bitor"
---|"bitxor"
---|"bitnot"
---|"logand"
---|"logor"
---|"logxor"
---|"lognot"
---|"comparison"
---|"undefined"
---|"aborted"
---|"datetime"
