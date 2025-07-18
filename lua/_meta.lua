--- @meta
error("Cannot require a meta file")

---@class Qalculate
---@field new fun(table): QalcCalculator

---@class QalcCalculator
---@field eval fun(self: QalcCalculator, expr: string): QalcExpression
---@field reset fun(self: QalcCalculator, variables: boolean, functions: boolean)
---@field set_options fun(self: QalcCalculator, table)

---@class QalcExpression
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
