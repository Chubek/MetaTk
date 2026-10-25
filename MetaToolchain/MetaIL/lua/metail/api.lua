-- Shared operations for the native API and CLI. The canonical IR stays in ir.lua.
local parser=require('metail.parser')
local ir=require('metail.ir')
local json=require('metail.json')
local analysis=require('metail.analysis')
local transform=require('metail.transform')
local lkopt=require('lkopt')
local M={}
local loaded=false
function M.ensure_kernels()
  if not loaded then
    lkopt.load_directory(_METAIL_ROOT..'/optimize-kernels')
    loaded=true
  end
end
function M.validate(program)
  local seen={}
  for _,f in ipairs(program.functions) do
    assert(not seen[f.name],'duplicate fn '..f.name);seen[f.name]=true
    local params={}
    for _,p in ipairs(f.params) do assert(not params[p],'duplicate parameter '..p);params[p]=true end
    analysis.cfg(f)
  end
  return program
end
function M.parse(source,name)
  return M.validate(parser.program(source,name or '<memory>'))
end
function M.passes(filter,kind)
  M.ensure_kernels()
  local result={}
  local function accepts(k) return (k.kind=='analysis')==(kind=='analysis') end
  if not filter or filter=='all' then
    for _,name in ipairs(lkopt.order) do
      local k=lkopt.registered[name]
      if accepts(k) then result[#result+1]=k end
    end
  else
    assert(filter:match('%S') and not filter:match('^%s*,') and
      not filter:match(',%s*$') and not filter:match(',%s*,'),'empty pass name')
    for name in filter:gmatch('[^,]+') do
      name=name:match('^%s*(.-)%s*$')
      local k=assert(lkopt.registered[name],'unknown kernel '..name)
      assert(accepts(k),'incorrect kernel kind for '..name)
      result[#result+1]=k
    end
  end
  return result
end
function M.optimize_program(p,filter,rounds)
  assert(math.type(rounds)=='integer' and rounds>=1 and rounds<=12,'--repeat must be 1..12')
  local choices=M.passes(filter,'transform')
  local stats={}
  for _=1,rounds do
    local changes=0
    for _,k in ipairs(choices) do
      local st=transform.apply(p,k);changes=changes+st.changes
      stats[k.name]=(stats[k.name] or 0)+st.changes
    end
    if not filter or filter=='all' then
      for _,kind in ipairs({'constant-folding','simplify-branches','dead-assignments','unreachable-code'}) do
        local st=transform.apply(p,{name='FinalCleanup',kind='builtin',builtin_kind=kind})
        changes=changes+st.changes
      end
    end
    if changes==0 then break end
  end
  M.validate(p)
  return ir.emit(p),json.encode({kernel_changes=stats})..'\n'
end
function M.analyze_program(p,filter)
  local results={}
  for _,k in ipairs(M.passes(filter,'analysis')) do results[k.name]=analysis.run(p,k.analysis_cfg) end
  return json.encode({module=p.module,analyses=results})..'\n'
end
function M.check(source,name) M.parse(source,name) end
function M.emit(source,name) return ir.emit(M.parse(source,name)) end
function M.optimize(source,name,filter,rounds) return M.optimize_program(M.parse(source,name),filter,rounds) end
function M.analyze(source,name,filter) return M.analyze_program(M.parse(source,name),filter) end
function M.load_kernels(directory) M.ensure_kernels();lkopt.load_directory(directory) end
function M.kernel_count() M.ensure_kernels();return #lkopt.order end
function M.kernel_info(index)
  M.ensure_kernels()
  local name=assert(lkopt.order[index],'kernel index out of range')
  local k=require('metail.catalog').describe(lkopt.registered[name])
  return k.name,k.path,k.kind,k.implementation,k.description,
    table.concat(k.keywords,'\n'),table.concat(k.tags,'\n'),table.concat(k.uses,'\n')
end
function M.kernels_json()
  M.ensure_kernels()
  local items=require('metail.catalog').search(lkopt,{})
  return json.encode({count=#items,kernels=items})..'\n'
end
return M
