--- @meta
error("Cannot require a meta file")

---@alias QalcBase "roman"|"time"|integer

---@class QalcPrintOptions
---@field base QalcBase?
---@field min_decimals boolean?
---@field max_decimals boolean?
---@field abbreviate_names boolean?
---@field negative_exponents boolean?
---@field spacious boolean?
---@field excessive_parenthesis boolean?
---@field interval_display "adaptive"| "significant"| "interval"| "plusminus"| "midpoint"| "lower"| "upper"| "concise"| "relative"?
---@field unicode "on"|"off"|"no-unit"?

---@class QalcParseOptions
---@field base QalcBase?
---@field mode "default"|"rpn"

---@class Qalculate
---@field new fun(): QalcCalculator

---@alias QalcMessages {[1]: string, [2]: vim.log.levels}[]

---@class QalcCalculator
---@field eval fun(self: QalcCalculator, expr: string, parse_opts: QalcParseOptions?, allow_assingment: boolean?): QalcExpression, QalcMessages?
---@field plot fun(self: QalcCalculator, expr: string, min: QalcInput, max: QalcInput, step: QalcInput, parse_opts: QalcParseOptions?): number[]
---@field reset fun(self: QalcCalculator, variables: boolean, functions: boolean)
---@field get fun(self: QalcCalculator, name: string): QalcExpression
---@field set fun(self: QalcCalculator, name: string, value: QalcInput): boolean

---@class QalcExpression
---@field print fun(self: QalcExpression, opts: QalcPrintOptions?): string, QalcMessages?
---@field value fun(self: QalcExpression, opts: QalcPrintOptions?): QalcValue
---@field type fun(self: QalcExpression): QalcType
---@field source fun(self: QalcExpression, opts: QalcPrintOptions?): string?
---@field is_approximate fun(self: QalcExpression): boolean
---@field as_matrix fun(self: QalcExpression): QalcExpression[][]?

--- Regular Number | Vector | Matrix | Expression
---@alias QalcValue number|number[]|number[][]|{[1]: QalcType, [integer]: QalcValue}

---@alias QalcInput string|number|QalcExpression

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
---|"matrix"
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
