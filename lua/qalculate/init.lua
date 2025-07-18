local soname = vim.fs.dirname(debug.getinfo(1).source:sub(2)) .. "/?.so"

package.cpath = package.cpath .. ";" .. soname

---@type Qalculate
local qalc = require("qalculate.qalc")

return {
    new = qalc.new,
    default = qalc.new {},
}
