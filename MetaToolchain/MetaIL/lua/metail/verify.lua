-- Boundary-value and effect/trap equivalence checks for every declarative rule.
local ir=require('metail.ir')
local parser=require('metail.parser')
local transform=require('metail.transform')
local V={}
local function evaluate(n,trace)
  if n.tag=='int' then return n.value end
  if n.tag=='var' then return 17 end
  if n.tag=='call' then
    trace[#trace+1]=n.name
    return #trace*7
  end
  assert(n.tag=='op','unbound verifier node')
  local a,err=evaluate(n.args[1],trace)
  if a==nil then return nil,err end
  if n.op=='select' then return evaluate(n.args[a~=0 and 2 or 3],trace) end
  local b
  if n.args[2] then b,err=evaluate(n.args[2],trace);if b==nil then return nil,err end end
  return lkopt_native.eval(n.op,a,b)
end
V.evaluate=evaluate
local function instantiate(n,captures)
  if n.tag=='hole' then return ir.copy(assert(captures[n.name])) end
  local out=ir.copy(n)
  for i,c in ipairs(n.args or {}) do out.args[i]=instantiate(c,captures) end
  return out
end
function V.run(lkopt)
  local values={0,1,-1,2,-2,7,-7,63,64,65,255,256,1024,math.mininteger,math.maxinteger}
  for _,width in ipairs({8,16,24,32,40,48,62}) do
    local p=1<<width
    for _,x in ipairs({p-1,p,p+1,-p-1,-p,-p+1}) do values[#values+1]=x end
  end
  local seed=0x14887
  local function random()
    -- Lua integers wrap; these vectors exercise all 64 bits, not just small literals.
    seed=seed*6364136223846793005+1442695040888963407
    return seed
  end
  local verified,cases,effect_cases=0,0,0
  for _,name in ipairs(lkopt.order) do
    local k=lkopt.registered[name]
    if k.kind=='rewrite' then
      local holes,set={},{}
      ir.walk(k.pattern_ast,function(n)
        if n.tag=='hole' and not set[n.name] then set[n.name]=true;holes[#holes+1]=n.name end
      end)
      local fired=false
      local function check(captures,effects)
        local before=instantiate(k.pattern_ast,captures)
        local p={functions={{name='check',params={},statements={{kind='return',expr=before}}}}}
        local stat=transform.apply(p,k)
        local after=p.functions[1].statements[1].expr
        local at,bt={},{}
        local a,ae=evaluate(before,at)
        local b,be=evaluate(after,bt)
        assert(a==b and ae==be and table.concat(at,',')==table.concat(bt,','),
          k.name..': semantic/effect/trap mismatch '..ir.format(before)..' => '..ir.format(after))
        if stat.changes>0 then fired=true end
        cases=cases+1
        if effects then effect_cases=effect_cases+1 end
      end
      local function constants(value)
        local captures={}
        for _,hole in ipairs(holes) do captures[hole]={tag='int',value=value} end
        return captures
      end
      for _,x in ipairs(values) do
        check(constants(x))
        for _,hole in ipairs(holes) do
          local captures=constants(1);captures[hole]={tag='int',value=x};check(captures)
        end
      end
      if #holes==2 then
        for _,a in ipairs(values) do for _,b in ipairs(values) do
          check({[holes[1]]={tag='int',value=a},[holes[2]]={tag='int',value=b}})
        end end
      end
      for _=1,128 do
        local captures={}
        for _,hole in ipairs(holes) do captures[hole]={tag='int',value=random()} end
        check(captures)
      end
      for _,src in ipairs({'div(1,0)','div(-9223372036854775808,-1)',
          'mod(1,0)','shl(1,64)','shr(1,-1)','x'}) do
        for _,hole in ipairs(holes) do
          local captures=constants(1);captures[hole]=parser.expression(src);check(captures,true)
        end
      end
      for _,hole in ipairs(holes) do
        local captures=constants(1)
        captures[hole]={tag='call',name='tick_'..hole:sub(2),args={}}
        check(captures,true)
      end
      local captures={}
      for _,hole in ipairs(holes) do captures[hole]={tag='call',name='tick_'..hole:sub(2),args={}} end
      check(captures,true)
      assert(fired,k.name..': rule never fired')
      verified=verified+1
    end
  end
  return verified,cases,effect_cases
end
return V
