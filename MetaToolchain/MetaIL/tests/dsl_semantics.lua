local parser=require('metail.parser')
local ir=require('metail.ir')
local evaluate=require('metail.verify').evaluate
local file=assert(io.open(assert(arg[3]),'rb'))
local source=assert(file:read('*a'));file:close()
local program=parser.program(source,'DSLtk rewrite corpus')
local values={math.mininteger,math.mininteger+1,-65,-64,-1,0,1,63,64,65,
              math.maxinteger-1,math.maxinteger}
local seed=0x1D51
for _=1,128 do
  seed=seed*6364136223846793005+1442695040888963407
  values[#values+1]=seed
end
local function instantiate(expr,value)
  local copy=ir.copy(expr)
  ir.walk(copy,function(n)
    if n.tag=='var' then n.tag='int';n.value=value;n.name=nil end
  end)
  return copy
end
assert(#program.functions==24,'missing DSLtk semantic cases')
local checked=0
for i=1,#program.functions,2 do
  local before=program.functions[i].statements[1].expr
  local after=program.functions[i+1].statements[1].expr
  for _,value in ipairs(values) do
    local at,bt={},{}
    local a,ae=evaluate(instantiate(before,value),at)
    local b,be=evaluate(instantiate(after,value),bt)
    assert(a==b and ae==be and table.concat(at,',')==table.concat(bt,','),
      program.functions[i].name..': value, trap, or effect order changed')
    checked=checked+1
  end
end
print('OK: '..checked..' DSLtk rewrite value/trap/effect comparisons')
