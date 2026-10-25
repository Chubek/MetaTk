local M = {}
local operators = {
  add=2,sub=2,mul=2,div=2,mod=2,band=2,bor=2,bxor=2,shl=2,shr=2,
  eq=2,ne=2,lt=2,le=2,gt=2,ge=2,land=2,lor=2,
  neg=1,bnot=1,lnot=1,select=3,
}
M.operators=operators
function M.copy(n)
  if type(n)~='table' then return n end
  local t={}; for k,v in pairs(n) do t[k]=M.copy(v) end; return t
end
function M.same(a,b)
  if type(a)~=type(b) then return false end
  if type(a)~='table' then return a==b end
  if a.tag~=b.tag then return false end
  if a.tag=='int' then return a.value==b.value end
  if a.tag=='var' or a.tag=='hole' then return a.name==b.name end
  if a.op~=b.op or a.name~=b.name or #(a.args or {})~=#(b.args or {}) then return false end
  for i,x in ipairs(a.args or {}) do if not M.same(x,b.args[i]) then return false end end
  return true
end
function M.pure(n)
  if n.tag=='call' then return false end
  if n.tag=='int' or n.tag=='var' or n.tag=='hole' then return true end
  -- "pure" means safe to discard/duplicate/reorder: traps are observable too.
  if n.tag~='op' or not operators[n.op] then return false end
  if n.op=='div' or n.op=='mod' then
    local x,y=n.args[1],n.args[2]
    if y.tag~='int' or y.value==0 then return false end
    if y.value==-1 and (x.tag~='int' or x.value==math.mininteger) then return false end
  elseif n.op=='shl' or n.op=='shr' then
    local count=n.args[2]
    if count.tag~='int' or count.value<0 or count.value>63 then return false end
  elseif n.op=='select' and n.args[1].tag=='int' then
    return M.pure(n.args[n.args[1].value~=0 and 2 or 3])
  end
  for _,v in ipairs(n.args or {}) do if not M.pure(v) then return false end end
  return true
end
function M.walk(n,fn)
  fn(n)
  for _,x in ipairs(n.args or {}) do M.walk(x,fn) end
end
function M.node_count(n)
  local k=0; M.walk(n,function() k=k+1 end); return k
end
function M.depth(n)
  local d=0
  for _,a in ipairs(n.args or {}) do d=math.max(d,M.depth(a)) end
  return d+1
end
function M.vars(n,into)
  into=into or {}
  M.walk(n,function(x) if x.tag=='var' then into[x.name]=true end end)
  return into
end
local symbols={
  add='+',sub='-',mul='*',div='/',mod='%',band='&',bor='|',bxor='^',shl='<<',shr='>>',
  eq='==',ne='!=',lt='<',le='<=',gt='>',ge='>=',land='&&',lor='||'
}
local unary={neg='-',bnot='~',lnot='!'}
function M.format(n)
  if n.tag=='int' then return tostring(n.value) end
  if n.tag=='var' or n.tag=='hole' then return n.name end
  if n.tag=='call' then
    local vals={}; for _,a in ipairs(n.args) do vals[#vals+1]=M.format(a) end
    return n.name..'('..table.concat(vals,', ')..')'
  end
  if unary[n.op] then return '('..unary[n.op]..M.format(n.args[1])..')' end
  if symbols[n.op] then
    return '('..M.format(n.args[1])..' '..symbols[n.op]..' '..M.format(n.args[2])..')'
  end
  local vals={}; for _,a in ipairs(n.args) do vals[#vals+1]=M.format(a) end
  return n.op..'('..table.concat(vals,', ')..')'
end
function M.serialize(n)
  if n.tag=='int' then return tostring(n.value) end
  if n.tag=='var' or n.tag=='hole' then return n.name end
  local xs={}; for _,a in ipairs(n.args or {}) do xs[#xs+1]=M.serialize(a) end
  return (n.op or n.name)..'('..table.concat(xs,',')..')'
end
function M.exprs(stmt)
  if stmt.kind=='let' or stmt.kind=='return' or stmt.kind=='eval' or stmt.kind=='br' or stmt.kind=='assume' then
    return {stmt.expr}
  end
  return {}
end
function M.emit(program)
  local out={}
  if program.module then out[#out+1]='module '..program.module..';' end
  for _,f in ipairs(program.functions) do
    out[#out+1]='fn '..f.name..'('..table.concat(f.params,', ')..') {'
    for _,s in ipairs(f.statements) do
      local line
      if s.kind=='label' then line=s.name..':'
      elseif s.kind=='let' then line='  let '..s.name..' = '..M.format(s.expr)..';'
      elseif s.kind=='return' then line='  return '..M.format(s.expr)..';'
      elseif s.kind=='eval' then line='  eval '..M.format(s.expr)..';'
      elseif s.kind=='assume' then line='  assume '..M.format(s.expr)..';'
      elseif s.kind=='goto' then line='  goto '..s.target..';'
      elseif s.kind=='br' then line='  br '..M.format(s.expr)..', '..s.yes..', '..s.no..';'
      else error('cannot print statement: '..tostring(s.kind)) end
      out[#out+1]=line
    end
    out[#out+1]='}'
  end
  return table.concat(out,'\n')..'\n'
end
return M
