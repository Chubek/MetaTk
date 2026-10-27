local ir=require('metail.ir')
local guards=require('metail.guards')
local analysis=require('metail.analysis')
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
local function integer(v) return {tag='int',value=v} end
local function operation(op,... ) return {tag='op',op=op,args={...}} end
local function literal(n) return n and n.tag=='int' and n.value or nil end
local function isop(n,op) return n and n.tag=='op' and n.op==op end
local function nonzero(f) return f.lo>0 or f.hi<0 or f.one~=0 end
local function power_shift(v)
  if v==0 or (v & (v-1))~=0 then return nil end
  for n=0,63 do if v==(1<<n) then return n end end
end
local inverse_comparison={eq='ne',ne='eq',lt='ge',le='gt',gt='le',ge='lt'}
local associative={add=true,mul=true,band=true,bor=true,bxor=true}
local function weighstone_expr(n,kind,env,cache)
  local function fact(x) return analysis.integer_facts(x,env,cache) end
  local f=fact(n)
  if (kind=='known-bits' or kind=='ranges') and f.lo==f.hi and f.pure then return integer(f.lo) end
  if n.tag~='op' then return n end
  local op=n.op
  local a,b=n.args[1],n.args[2]
  local fa,fb=fact(a),b and fact(b)
  local av,bv=literal(a),literal(b)
  if kind=='known-bits' then
    if op=='band' then
      if fb.pure and ((~fa.zero) & (~fb.one))==0 then return a end
      if fa.pure and ((~fb.zero) & (~fa.one))==0 then return b end
    elseif op=='bor' then
      if fb.pure and ((~fb.zero) & (~fa.one))==0 then return a end
      if fa.pure and ((~fa.zero) & (~fb.one))==0 then return b end
    elseif op=='bxor' then
      if fb.pure and fb.lo==0 and fb.hi==0 then return a end
      if fa.pure and fa.lo==0 and fa.hi==0 then return b end
    end
  elseif kind=='ranges' then
    if op=='select' and fa.pure then
      if nonzero(fa) then return b end
      if fa.lo==0 and fa.hi==0 then return n.args[3] end
    elseif op=='div' and bv==-1 and fa.lo>math.mininteger then return operation('neg',a)
    elseif op=='mod' and bv and bv~=0 then
      if bv==math.mininteger then
        if fa.lo>math.mininteger then return a end
      else
        local d=math.abs(bv)
        if fa.lo> -d and fa.hi<d then return a end
      end
    elseif op=='land' or op=='lor' then
      -- Logical operands are eager in MetaIL. Only discard a total operand.
      if fb.pure and ((op=='land' and nonzero(fb)) or (op=='lor' and fb.lo==0 and fb.hi==0)) then
        return operation('ne',a,integer(0))
      end
      if fa.pure and ((op=='land' and nonzero(fa)) or (op=='lor' and fa.lo==0 and fa.hi==0)) then
        return operation('ne',b,integer(0))
      end
    elseif op=='eq' or op=='ne' then
      if bv and fa.lo>=0 and fa.hi<=1 and (bv==0 or bv==1) then
        if (op=='eq')==(bv==1) then return a end
        return operation('lnot',a)
      end
    end
  elseif kind=='strength' then
    if op=='mul' then
      local x,v=a,bv
      if not v then x,v=b,av end
      if v then
        local shift=power_shift(v)
        if shift then return shift==0 and x or operation('shl',x,integer(shift)) end
        if v<0 then
          shift=power_shift(-v)
          if shift then return operation('neg',shift==0 and x or operation('shl',x,integer(shift))) end
        end
      end
    elseif (op=='div' or op=='mod') and bv and bv~=0 and bv~=math.mininteger then
      local d=math.abs(bv)
      local shift=power_shift(d)
      -- Arithmetic shift is truncating division only for nonnegative or exactly
      -- divisible values. MIN/-1 remains a trap even when all low bits are zero.
      if shift and not (bv==-1 and fa.lo==math.mininteger) then
        local mask=d-1
        local aligned=(fa.zero & mask)==mask
        if fa.lo>=0 or aligned then
          if op=='div' then
            local q=shift==0 and a or operation('shr',a,integer(shift))
            return bv<0 and operation('neg',q) or q
          elseif aligned and fa.pure then return integer(0)
          elseif fa.lo>=0 then return operation('band',a,integer(mask)) end
        end
      end
    end
  elseif kind=='shifts' then
    if op=='band' and bv and isop(a,'shr') and isop(a.args[1],'shl') then
      local shift=literal(a.args[2])
      if shift and shift>0 and shift<64 and literal(a.args[1].args[2])==shift then
        local lowmask=(-1)>>shift
        if (bv & ~lowmask)==0 then return operation('band',a.args[1].args[1],b) end
      end
    end
    if (op=='shl' or op=='shr') and bv and bv>=0 and bv<64 then
      if bv==0 then return a end
      if isop(a,op) then
        local count=literal(a.args[2])
        if count and count>=0 and count<64 then
          if op=='shr' then return operation('shr',a.args[1],integer(math.min(63,count+bv))) end
          if count+bv<64 then return operation('shl',a.args[1],integer(count+bv)) end
          if fact(a.args[1]).pure then return integer(0) end
        end
      end
      if op=='shl' and isop(a,'shr') and literal(a.args[2])==bv then
        return operation('band',a.args[1],integer(-1<<bv))
      elseif op=='shr' and isop(a,'shl') and literal(a.args[2])==bv then
        local original=fact(a.args[1])
        local bound=1<<(63-bv)
        if original.lo>= -bound and original.hi<bound then return a.args[1] end
      end
    end
  elseif kind=='algebra' then
    -- Combine actual constants with the signed-64 evaluator, never Lua's / or
    -- floating point. These rules retain the variable subtree exactly once.
    if associative[op] then
      local x,c=a,bv
      if not c then x,c=b,av end
      if c and isop(x,op) then
        local inner,v=x.args[1],literal(x.args[2])
        if not v then inner,v=x.args[2],literal(x.args[1]) end
        if v then return operation(op,inner,integer(assert(lkopt_native.eval(op,v,c)))) end
      end
    end
    if (op=='add' or op=='sub') and bv and (isop(a,'add') or isop(a,'sub')) then
      local c=literal(a.args[2])
      if c then
        if a.op=='sub' then c= -c end
        local v=assert(lkopt_native.eval(op,c,bv))
        return operation('add',a.args[1],integer(v))
      end
    end
    if op=='lnot' and a.tag=='op' and inverse_comparison[a.op] then
      return operation(inverse_comparison[a.op],a.args[1],a.args[2])
    end
    if (op=='eq' or op=='ne') and bv and a.tag=='op' then
      local c=literal(a.args[2])
      local inverse=({add='sub',sub='add',bxor='bxor'})[a.op]
      if inverse and c then return operation(op,a.args[1],integer(assert(lkopt_native.eval(inverse,bv,c)))) end
      if a.op=='neg' or a.op=='bnot' then
        return operation(op,a.args[1],integer(assert(lkopt_native.eval(a.op,bv))))
      end
      if a.op=='mul' then
        local x=a.args[1]
        if not c then x,c=a.args[2],literal(a.args[1]) end
        if c then
          local inv=require('weighstone_native').inverse(c)
          if inv then return operation(op,x,integer(assert(lkopt_native.eval('mul',bv,inv)))) end
        end
      end
      local left=literal(a.args[1])
      if left and (a.op=='add' or a.op=='bxor' or a.op=='sub') then
        local value
        if a.op=='sub' then value=lkopt_native.eval('sub',left,bv)
        else value=lkopt_native.eval(inverse,bv,left) end
        return operation(op,a.args[2],integer(assert(value)))
      end
    end
    if (op=='div' or op=='mod') and bv and bv>0 and isop(a,op) then
      local c=literal(a.args[2])
      if c and c>0 then
        if op=='div' and c<=math.maxinteger//bv then return operation('div',a.args[1],integer(c*bv)) end
        if op=='mod' and c%bv==0 then return operation('mod',a.args[1],b) end
      end
    end
    if op=='add' or op=='sub' then
      local function term(x)
        if isop(x,'mul') then
          if literal(x.args[2]) then return x.args[1],x.args[2].value end
          if literal(x.args[1]) then return x.args[2],x.args[1].value end
        elseif isop(x,'shl') then
          local s=literal(x.args[2])
          if s and s>=0 and s<64 then return x.args[1],1<<s end
        end
        return x,1
      end
      local x,c=term(a);local y,d=term(b)
      if ir.same(x,y) and fact(x).pure then
        return operation('mul',x,integer(assert(lkopt_native.eval(op,c,d))))
      end
    end
  end
  return n
end
local function weighstone(f,kind)
  local changes=0
  analysis.integer_scan(f,function(s,_,_,env)
    local cache={}
    s.expr=bottom_up(s.expr,function(n)
      -- Bound local cascading independently of user-selected optimization rounds.
      for _=1,4 do
        local next=weighstone_expr(n,kind,env,cache)
        if ir.same(n,next) then break end
        changes=changes+1;n=next
      end
      return n
    end)
  end)
  return changes
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
      elseif k.builtin_kind:match('^weighstone%-') then
        stats.changes=stats.changes+weighstone(f,k.builtin_kind:sub(12))
      else error(k.name..': unimplemented builtin '..k.builtin_kind) end
    end
  else error('cannot transform with '..k.name) end
  return stats
end
return T
