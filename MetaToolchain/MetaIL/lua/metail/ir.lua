local M = {}
local operators = {
  add=2,sub=2,mul=2,div=2,mod=2,band=2,bor=2,bxor=2,shl=2,shr=2,
  eq=2,ne=2,lt=2,le=2,gt=2,ge=2,land=2,lor=2,
  neg=1,bnot=1,lnot=1,select=3,
}
M.operators=operators
M.abi_types={i8=true,u8=true,i16=true,u16=true,i32=true,u32=true,i64=true,u64=true,ptr=true}
local function library_name(name)
  assert(type(name)=='string' and #name>0 and name:sub(1,1)~='-' and name:sub(1,1)~='@' and
    not name:find('[%z\1-\31\127-\255]'),'invalid library name or path')
  assert(name:find('/',1,true) or name:match('^[A-Za-z0-9_+%.%-]+$'),'invalid linker library name')
end
local function symbol_name(name)
  assert(type(name)=='string' and name:match('^[A-Za-z_%.][A-Za-z0-9_%.$]*$'),'invalid external symbol')
end
-- Shared structural checks for parser-built and native-built programs.
function M.validate(program)
  local count=0
  local imports={}
  local function item()
    count=count+1;assert(count<=200000,'IR item limit exceeded')
  end
  local function identifier(name)
    assert(type(name)=='string' and name:match('^[A-Za-z_][A-Za-z0-9_]*$'),'invalid IR identifier: '..tostring(name))
  end
  local function expr(n,depth,discard)
    item();assert(depth<=256,'IR expression depth limit exceeded')
    assert(type(n)=='table','expression must be a table')
    if n.tag=='int' then assert(math.type(n.value)=='integer','literal must be signed-64 integer')
    elseif n.tag=='var' then
      identifier(n.name);assert(n.name~='true' and n.name~='false','boolean literal names cannot be variables')
    elseif n.tag=='op' or n.tag=='call' then
      assert(type(n.args)=='table','expression arguments required')
      if n.tag=='op' then assert(operators[n.op] and #n.args==operators[n.op],'invalid operator or arity')
      else
        identifier(n.name)
        assert(not operators[n.name] and n.name~='true' and n.name~='false','reserved call name: '..n.name)
        local f=imports[n.name]
        if f then
          assert(#n.args>=#f.params and (f.variadic or #n.args==#f.params),'external call arity mismatch: '..n.name)
          assert(f.result~='void' or discard,'void external call is only valid as an eval statement: '..n.name)
        end
      end
      for _,arg in ipairs(n.args) do expr(arg,depth+1) end
    else error('invalid expression tag: '..tostring(n.tag)) end
  end
  assert(type(program)=='table' and type(program.functions)=='table' and #program.functions>0,'module requires at least one function')
  if program.module then identifier(program.module) end
  local symbols={}
  for _,f in ipairs(program.externals or {}) do
    item();identifier(f.name);symbol_name(f.symbol);library_name(f.library)
    assert(not operators[f.name] and f.name~='true' and f.name~='false','reserved external name: '..f.name)
    assert(not imports[f.name],'duplicate external function: '..f.name)
    assert(not symbols[f.symbol],'duplicate external symbol: '..f.symbol)
    imports[f.name]=f;symbols[f.symbol]=true
    assert(M.abi_types[f.result] or f.result=='void','invalid external result type: '..tostring(f.result))
    assert(type(f.params)=='table','external parameter types required')
    assert(type(f.variadic)=='boolean','external variadic flag must be boolean')
    assert(not f.variadic or #f.params>0,'variadic external requires a fixed parameter')
    for _,p in ipairs(f.params) do item();assert(M.abi_types[p],'invalid external parameter type: '..tostring(p)) end
  end
  for _,f in ipairs(program.functions) do
    item();identifier(f.name)
    assert(not imports[f.name] and not symbols[f.name],'external function conflicts with local definition: '..f.name)
    for _,p in ipairs(f.params) do item();identifier(p) end
    for _,s in ipairs(f.statements) do
      item()
      if s.kind=='let' then identifier(s.name);expr(s.expr,1)
      elseif s.kind=='return' or s.kind=='eval' or s.kind=='assume' then expr(s.expr,1,s.kind=='eval')
      elseif s.kind=='label' then
        identifier(s.name)
        assert(not ({let=true,['return']=true,eval=true,assume=true,['goto']=true,br=true})[s.name],
          'reserved label name: '..s.name)
      elseif s.kind=='goto' then identifier(s.target)
      elseif s.kind=='br' then expr(s.expr,1);identifier(s.yes);identifier(s.no)
      else error('invalid statement kind: '..tostring(s.kind)) end
    end
  end
  return program
end
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
  for _,f in ipairs(program.externals or {}) do
    local params=M.copy(f.params)
    if f.variadic then params[#params+1]='...' end
    out[#out+1]='extern fn '..f.name..'('..table.concat(params,', ')..') -> '..f.result..
      ' from '..string.format('%q',f.library)..' symbol '..string.format('%q',f.symbol)..';'
  end
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
-- Stable, declaration-ordered driver metadata. Never interpreted as shell text.
function M.linkage(program)
  local libraries,arguments,imports,seen={},{},{},{}
  for _,f in ipairs(program.externals or {}) do
    if not seen[f.library] then
      seen[f.library]=true;libraries[#libraries+1]=f.library
      arguments[#arguments+1]=f.library:find('/',1,true) and f.library or ('-l'..f.library)
    end
    imports[#imports+1]={name=f.name,symbol=f.symbol,library=f.library,calling_convention='c',
      result=f.result,parameters=M.copy(f.params),variadic=f.variadic}
  end
  return {schema=1,libraries=libraries,arguments=arguments,imports=imports}
end
return M
