-- Optikern declarative DSL: Lua 5.4. Each kernel is either executable analysis,
-- a first-class IR rewrite rule, or a named implementation of a built-in pass.
local parser=require('metail.parser')
local guards=require('metail.guards')
local ir=require('metail.ir')
local analysis=require('metail.analysis')
local M={optimizer={},registered={},order={},numeric=require('metanum')}
local builtins={['constant-folding']=true,['dead-assignments']=true,
  ['constant-propagation']=true,['copy-propagation']=true,['simplify-branches']=true,
  ['unreachable-code']=true,['common-subexpressions']=true}
local function check(id)
  assert(type(id)=='string' and id:match('^[%a_][%w_]*$'),'invalid kernel identifier: '..tostring(id))
  assert(not M.registered[id],'duplicate kernel: '..id)
end
function M.optimizer.new(name)
  check(name)
  local k={name=name,kind=nil}
  function k:rewrite(cfg)
    assert(not self.kind,'kernel kind already declared')
    assert(type(cfg)=='table' and type(cfg.pattern)=='string' and type(cfg.replace)=='string', 'rewrite requires pattern and replace')
    self.kind='rewrite';self.pattern=cfg.pattern;self.replace=cfg.replace
    self.pattern_ast=parser.expression(cfg.pattern,self.name..':pattern')
    self.replace_ast=parser.expression(cfg.replace,self.name..':replacement')
    self.guard=cfg.guard or ''
    assert(type(self.guard)=='string', 'guard must be a guard expression string')
    local holes={}
    local function walk(n,fn) fn(n);for _,c in ipairs(n.args or {}) do walk(c,fn) end end
    walk(self.pattern_ast,function(n) if n.tag=='hole' then holes[n.name]=true end end)
    walk(self.replace_ast,function(n) if n.tag=='hole' then assert(holes[n.name],self.name..': unbound '..n.name) end end)
    self.guard_clauses=guards.parse(self.guard,holes)
    assert(not ir.same(self.pattern_ast,self.replace_ast),self.name..': no-op rewrite')
    return self
  end
  function k:analysis(cfg)
    assert(not self.kind,'kernel kind already declared')
    assert(type(cfg)=='table' and type(cfg.kind)=='string','analysis requires kind')
    analysis.validate(cfg)
    self.kind='analysis';self.analysis_cfg=ir.copy(cfg);return self
  end
  function k:builtin(kind)
    assert(not self.kind,'kernel kind already declared')
    assert(builtins[kind],'unknown builtin: '..tostring(kind))
    self.kind='builtin';self.builtin_kind=kind;return self
  end
  M.registered[name]=k;M.order[#M.order+1]=name
  return k
end
function M.load(path)
  local before=#M.order
  -- Give each file private library tables and only the registration/numeric API.
  local env={lkopt={optimizer={new=M.optimizer.new},numeric=ir.copy(M.numeric)},
    pairs=pairs,ipairs=ipairs,tonumber=tonumber,tostring=tostring,
    math=ir.copy(math),string=ir.copy(string),table=ir.copy(table),assert=assert}
  local chunk,err=loadfile(path,'t',env)
  if not chunk then error(path..': '..err) end
  local ok,result=pcall(function()
    chunk()
    assert(#M.order==before+1,'must register exactly one kernel')
    local k=M.registered[M.order[#M.order]]
    assert(k.kind~=nil,'must declare :rewrite, :analysis, or :builtin')
    return k
  end)
  if not ok then
    while #M.order>before do M.registered[table.remove(M.order)]=nil end
    error(path..': '..tostring(result),0)
  end
  return result
end
function M.load_directory(dir)
  local before=#M.order
  local ok,err=pcall(function()
    for _,path in ipairs(lkopt_native.scan(dir)) do M.load(path) end
  end)
  if not ok then
    while #M.order>before do M.registered[table.remove(M.order)]=nil end
    error(err,0)
  end
end
return M
