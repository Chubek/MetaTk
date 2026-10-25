local root=assert(_METAIL_ROOT,'MetaIL home not provided')
package.path=root..'/lua/?.lua;'..root..'/lua/?/init.lua;'..package.path
local parser=require('metail.parser')
local ir=require('metail.ir')
local json=require('metail.json')
local analysis=require('metail.analysis')
local transform=require('metail.transform')
local lkopt=require('lkopt')
local function help()
  io.write([[
MetaIL + Optikern (Lua 5.4)
Usage:
  metail check <program.mi>
  metail emit <program.mi> [--out FILE]
  metail kernels [--json]
  metail optimize <program.mi> [--passes all|Name1,Name2] [--repeat N] [--out FILE] [--stats FILE]
  metail analyze <program.mi> [--passes all|Name1,Name2] [--out FILE]
  metail selftest
  metail verify                 # check rewrite semantics against 64-bit evaluator
  metail lua <script.lua>       # run Lua with lkopt_native and MetaNum available

For a portable runtime installation set METAIL_HOME=<project directory>.
Optikern kernels are individually loadable Lua DSL files (.optk).
]])
end
local function read(path)
  local f,err=io.open(path,'rb');if not f then error(err) end
  local text=f:read('*a');f:close();return text
end
local function write(path,data)
  if not path or path=='-' then io.write(data);return end
  local f,err=io.open(path,'wb');if not f then error(err) end
  assert(f:write(data));assert(f:close())
end
local function options(from)
  local o={};local i=from
  while arg[i] do
    local key=arg[i]
    if key=='--json' then o.json=true;i=i+1
    elseif key=='--out' or key=='--passes' or key=='--repeat' or key=='--stats' then
      assert(arg[i+1],key..' requires an argument')
      o[key:sub(3)]=arg[i+1];i=i+2
    else error('unknown option '..key) end
  end
  return o
end
local function validate(program)
  local seen={}
  for _,f in ipairs(program.functions) do
    assert(not seen[f.name],'duplicate fn '..f.name);seen[f.name]=true
    local params={}
    for _,p in ipairs(f.params) do assert(not params[p],'duplicate parameter '..p);params[p]=true end
    analysis.cfg(f)
  end
end
local function passes(filter,required_kind)
  local result={}
  if not filter or filter=='all' then
    for _,name in ipairs(lkopt.order) do
      local k=lkopt.registered[name]
      if required_kind=='analysis' and k.kind=='analysis' or
        required_kind=='transform' and k.kind~='analysis' then result[#result+1]=k end
    end
  else
    for name in filter:gmatch('[^,]+') do
      name=name:match('^%s*(.-)%s*$')
      local k=assert(lkopt.registered[name],'unknown kernel '..name)
      assert(required_kind=='analysis' and k.kind=='analysis' or
        required_kind=='transform' and k.kind~='analysis','incorrect kernel kind for '..name)
      result[#result+1]=k
    end
  end
  return result
end
local function execute()
  local cmd=arg[1]
  if cmd==nil or cmd=='help' or cmd=='--help' or cmd=='-h' then help();return 0 end
  if cmd=='lua' then
    assert(arg[2],'usage: metail lua SCRIPT')
    assert(loadfile(arg[2],'t'))()
    return 0
  end
  if cmd=='check' then
    assert(arg[2] and not arg[3],'usage: metail check FILE')
    local p=parser.program(read(arg[2]),arg[2]);validate(p)
    io.write('valid MetaIL: '..#p.functions..' function(s)\n')
    return 0
  end
  if cmd=='emit' then
    assert(arg[2],'usage: metail emit FILE [--out FILE]')
    local opts=options(3);local p=parser.program(read(arg[2]),arg[2]);validate(p)
    write(opts.out,ir.emit(p));return 0
  end
  lkopt.load_directory(root..'/optimize-kernels')
  if cmd=='kernels' then
    local opts=options(2)
    local items={}
    for _,name in ipairs(lkopt.order) do
      local k=lkopt.registered[name]
      items[#items+1]={name=name,kind=k.kind,implementation=k.kind=='builtin' and k.builtin_kind or
        k.kind=='analysis' and k.analysis_cfg.kind or k.pattern}
    end
    if opts.json then write('-',json.encode({count=#items,kernels=items})..'\n')
    else
      io.write(#items..' Optikern kernels\n')
      for _,k in ipairs(items) do io.write(string.format('%-39s %-9s %s\n',k.name,k.kind,k.implementation)) end
    end
    return 0
  end
  if cmd=='verify' then
    local verified,cases,effects=require('metail.verify').run(lkopt)
    io.write('verified '..verified..' rewrites across '..cases..' signed-64 cases ('..effects..' effect/trap cases)\n')
    return 0
  end
  if cmd=='selftest' then
    local src='module demo; fn main(a) { entry: let x = add(a,0); let y = mul(x,2); return y; }'
    local p=parser.program(src,'<selftest>');validate(p)
    assert(parser.program(ir.emit(p),'<emit>').functions[1].name=='main')
    assert(#lkopt.order>200,'expected at least 201 kernels')
    io.write('selftest ok ('..#lkopt.order..' kernels)\n');return 0
  end
  assert(cmd=='optimize' or cmd=='analyze','unknown command '..cmd)
  assert(arg[2], 'usage: metail '..cmd..' FILE')
  local opts=options(3)
  local p=parser.program(read(arg[2]),arg[2]);validate(p)
  if cmd=='analyze' then
    local results={}
    for _,k in ipairs(passes(opts.passes,'analysis')) do
      results[k.name]=analysis.run(p,k.analysis_cfg)
    end
    write(opts.out,json.encode({module=p.module,analyses=results})..'\n')
  else
    local choices=passes(opts.passes,'transform')
    local rounds=tonumber(opts['repeat'] or '2')
    assert(rounds and math.type(rounds)=='integer' and rounds>=1 and rounds<=12,'--repeat must be 1..12')
    local stats={}
    -- Rewrites are deterministic, with a final fold and dead-code cleanup each iteration.
    for _=1,rounds do
      local changes=0
      for _,k in ipairs(choices) do
        local st=transform.apply(p,k);changes=changes+st.changes
        stats[k.name]=(stats[k.name] or 0)+st.changes
      end
      if not opts.passes or opts.passes=='all' then
        local clean={ {name='FinalConstantFolding',kind='builtin',builtin_kind='constant-folding'},
          {name='FinalBranchSimplification',kind='builtin',builtin_kind='simplify-branches'},
          {name='FinalDeadAssignmentElimination',kind='builtin',builtin_kind='dead-assignments'},
          {name='FinalUnreachableCodeElimination',kind='builtin',builtin_kind='unreachable-code'}}
        for _,k in ipairs(clean) do local st=transform.apply(p,k);changes=changes+st.changes end
      end
      if changes==0 then break end
    end
    validate(p)
    write(opts.out,ir.emit(p))
    if opts.stats then write(opts.stats,json.encode({kernel_changes=stats})..'\n') end
  end
  return 0
end
local ok,code=pcall(execute)
if not ok then io.stderr:write('metail: '..tostring(code)..'\n');return 1 end
return code
