local ir=require('metail.ir')
local guards=require('metail.guards')
local T={}
local function match(pattern,actual,env)
  if pattern.tag=='hole' then
    if env[pattern.name] then return ir.same(env[pattern.name],actual) end
    env[pattern.name]=actual;return true
  end
  if pattern.tag~=actual.tag then return false end
  if pattern.tag=='int' then return pattern.value==actual.value end
  if pattern.tag=='var' then return pattern.name==actual.name end
  if pattern.name~=actual.name or pattern.op~=actual.op then return false end
  if #(pattern.args or {})~=#(actual.args or {}) then return false end
  for i,p in ipairs(pattern.args or {}) do if not match(p,actual.args[i],env) then return false end end
  return true
end
local function instantiate(n,env)
  if n.tag=='hole' then return ir.copy(assert(env[n.name], 'undefined capture '..n.name)) end
  local x={}
  for k,v in pairs(n) do if k~='args' then x[k]=v end end
  if n.args then
    x.args={}; for i,c in ipairs(n.args) do x.args[i]=instantiate(c,env) end
  end
  return x
end
local function guard(rule,env)
  return guards.test(rule.guard_clauses or guards.parse(rule.guard,env),env)
end
local function bottom_up(n,apply)
  if n.args then
    local changed={}
    for i,v in ipairs(n.args) do changed[i]=bottom_up(v,apply) end
    n=ir.copy(n);n.args=changed
  end
  return apply(n)
end
function T.fold_expr(n)
  return bottom_up(n,function(x)
    if x.tag~='op' then return x end
    if x.op=='select' and x.args[1].tag=='int' then
      return ir.copy(x.args[x.args[1].value~=0 and 2 or 3])
    end
    if x.op=='select' then return x end
    for _,a in ipairs(x.args) do if a.tag~='int' then return x end end
    local v=lkopt_native.eval(x.op,x.args[1].value,x.args[2] and x.args[2].value)
    if v~=nil then return {tag='int',value=v} end
    return x
  end)
end
local function rewrite_expr(n,k,stat)
  return bottom_up(n,function(x)
    local cap={}
    if match(k.pattern_ast,x,cap) and guard(k,cap) then
      local replacement=instantiate(k.replace_ast,cap)
      if not ir.same(x,replacement) then stat.changes=stat.changes+1;return replacement end
    end
    return x
  end)
end
local function each_expr(fn,callback)
  for _,s in ipairs(fn.statements) do if s.expr then s.expr=callback(s.expr) end end
end
local function assignments(f)
  -- Stable conservative fixed point. A RHS call is a side effect: preserve it as eval.
  local any=false
  for _=1,64 do
    local used={}
    for _,s in ipairs(f.statements) do if s.expr then ir.vars(s.expr,used) end end
    local kept,changed={},false
    for _,s in ipairs(f.statements) do
      if s.kind=='let' and not used[s.name] then
        if not ir.pure(s.expr) then kept[#kept+1]={kind='eval',expr=s.expr} end
        changed=true
      else kept[#kept+1]=s end
    end
    f.statements=kept
    if not changed then break end
    any=true
  end
  return any
end
local function local_propagate(f,kind)
  local env={};local changes=0
  for _,s in ipairs(f.statements) do
    if s.kind=='label' or s.kind=='goto' or s.kind=='br' then env={} end
    if s.expr then
      local function replace(n)
        if n.tag=='var' and env[n.name] then changes=changes+1;return ir.copy(env[n.name]) end
        return n
      end
      s.expr=bottom_up(s.expr,replace)
    end
    if s.kind=='let' then
      -- A later variable redefinition may invalidate copies; avoid crossing it.
      env[s.name]=nil
      for name,n in pairs(env) do
        if ir.vars(n)[s.name] then env[name]=nil end
      end
      if kind=='constant-propagation' and s.expr.tag=='int' then env[s.name]=ir.copy(s.expr) end
      if kind=='copy-propagation' and s.expr.tag=='var' and s.expr.name~=s.name then env[s.name]=ir.copy(s.expr) end
    end
  end
  return changes
end
local function simplify_branches(f)
  local count=0
  for _,s in ipairs(f.statements) do
    if s.kind=='br' and s.expr.tag=='int' then
      local dest=s.expr.value~=0 and s.yes or s.no
      s.kind='goto';s.target=dest;s.expr=nil;s.yes=nil;s.no=nil;count=count+1
    elseif s.kind=='br' and s.yes==s.no and ir.pure(s.expr) then
      s.kind='goto';s.target=s.yes;s.expr=nil;s.yes=nil;s.no=nil;count=count+1
    end
  end
  return count
end
local function unreachable(f)
  local ss=f.statements
  if #ss==0 then return 0 end
  local boundaries={1}
  for i,s in ipairs(ss) do
    if s.kind=='label' and i~=1 then boundaries[#boundaries+1]=i end
    if (s.kind=='return' or s.kind=='goto' or s.kind=='br') and i<#ss then boundaries[#boundaries+1]=i+1 end
  end
  table.sort(boundaries)
  local uniq={};for _,x in ipairs(boundaries) do if uniq[#uniq]~=x then uniq[#uniq+1]=x end end
  local blocks={};local label_to_block={}
  for i,begin in ipairs(uniq) do
    local last=(uniq[i+1] or (#ss+1))-1
    local b={begin=begin,last=last}
    blocks[i]=b
    for j=begin,last do if ss[j].kind=='label' then label_to_block[ss[j].name]=i end end
  end
  for i,b in ipairs(blocks) do
    local last=ss[b.last];b.succ={}
    if last and last.kind=='goto' then b.succ={label_to_block[last.target]}
    elseif last and last.kind=='br' then b.succ={label_to_block[last.yes],label_to_block[last.no]}
    elseif last and last.kind=='return' then b.succ={}
    elseif blocks[i+1] then b.succ={i+1} end
  end
  local seen={};local function dfs(i)
    if not i or seen[i] then return end
    seen[i]=true; for _,v in ipairs(blocks[i].succ) do dfs(v) end
  end
  dfs(1)
  local kept={};local dropped=0
  for i,b in ipairs(blocks) do
    for j=b.begin,b.last do
      if seen[i] then kept[#kept+1]=ss[j] else dropped=dropped+1 end
    end
  end
  f.statements=kept
  return dropped
end
local function cse(f)
  local seen,changed={},0
  for _,s in ipairs(f.statements) do
    if s.kind=='label' or s.kind=='goto' or s.kind=='br' then seen={} end
    if s.kind=='let' then
      for key,entry in pairs(seen) do
        if entry.name==s.name or ir.vars(entry.expr)[s.name] then seen[key]=nil end
      end
      local key=ir.serialize(s.expr)
      if ir.pure(s.expr) and seen[key] then
        s.expr={tag='var',name=seen[key].name};changed=changed+1
      elseif ir.pure(s.expr) and not ir.vars(s.expr)[s.name] then
        seen[key]={name=s.name,expr=ir.copy(s.expr)}
      end
    end
  end
  return changed
end
function T.apply(program,k)
  local stats={name=k.name,kind=k.kind,changes=0}
  if k.kind=='rewrite' then
    for _,f in ipairs(program.functions) do
      each_expr(f,function(n) return rewrite_expr(n,k,stats) end)
    end
  elseif k.kind=='builtin' then
    for _,f in ipairs(program.functions) do
      if k.builtin_kind=='constant-folding' then
        each_expr(f,function(n)
          local newer=T.fold_expr(n)
          if not ir.same(n,newer) then stats.changes=stats.changes+1 end
          return newer
        end)
      elseif k.builtin_kind=='dead-assignments' then
        if assignments(f) then stats.changes=stats.changes+1 end
      elseif k.builtin_kind=='constant-propagation' or k.builtin_kind=='copy-propagation' then
        stats.changes=stats.changes+local_propagate(f,k.builtin_kind)
      elseif k.builtin_kind=='simplify-branches' then
        stats.changes=stats.changes+simplify_branches(f)
      elseif k.builtin_kind=='unreachable-code' then
        stats.changes=stats.changes+unreachable(f)
      elseif k.builtin_kind=='common-subexpressions' then
        stats.changes=stats.changes+cse(f)
      else error(k.name..': unimplemented builtin '..k.builtin_kind) end
    end
  else error('cannot transform with '..k.name) end
  return stats
end
return T
