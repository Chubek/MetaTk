local root=assert(_METAIL_ROOT,'MetaIL home not provided')
package.path=root..'/lua/?.lua;'..root..'/lua/?/init.lua;'..package.path
local parser=require('metail.parser')
local ir=require('metail.ir')
local json=require('metail.json')
local lkopt=require('lkopt')
local api=require('metail.api')
local catalog=require('metail.catalog')
local function help()
  io.write([[
MetaIL + Optikern (Lua 5.4)
Usage:
  metail check <program.mi>
  metail emit <program.mi> [--out FILE]
  metail linkage <program.mi> [--out FILE]  # linker libraries, arguments, imports
  metail kernels [--search WORDS] [--tag TAG ...] [--use WORDS] [--kind KIND]
                 [--json | --paths | --source] [--kernel-dir DIR]
  metail optimize <program.mi> [--passes all|Name1,Name2] [--repeat N] [--out FILE] [--stats FILE]
  metail analyze <program.mi> [--passes all|Name1,Name2] [--out FILE]
  metail selftest
  metail verify                 # check rewrite semantics against 64-bit evaluator
  metail lua <script.lua>       # run Lua with lkopt_native and MetaNum available

For a portable runtime installation set METAIL_HOME=<project directory>.
Optikern kernels are individually loadable Lua DSL files (.optk).
Use --kernel-dir DIR with kernels, optimize or analyze to load additional kernels.
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
local function options(from,allowed)
  local o={};local i=from
  while arg[i] do
    local key=arg[i]
    assert(allowed[key],'unknown option '..key)
    if key=='--json' or key=='--paths' or key=='--source' then o[key:sub(3)]=true;i=i+1
    elseif key=='--tag' then
      assert(arg[i+1],key..' requires an argument')
      o.tags=o.tags or {};o.tags[#o.tags+1]=arg[i+1];i=i+2
    else
      assert(arg[i+1],key..' requires an argument')
      o[key:sub(3)]=arg[i+1];i=i+2
    end
  end
  return o
end
local validate=api.validate
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
  if cmd=='emit' or cmd=='linkage' then
    assert(arg[2],'usage: metail '..cmd..' FILE [--out FILE]')
    local opts=options(3,{['--out']=true});local p=parser.program(read(arg[2]),arg[2]);validate(p)
    write(opts.out,cmd=='emit' and ir.emit(p) or (json.encode(ir.linkage(p))..'\n'));return 0
  end
  api.ensure_kernels()
  if cmd=='kernels' then
    local opts=options(2,{['--json']=true,['--paths']=true,['--source']=true,['--search']=true,
      ['--tag']=true,['--use']=true,['--kind']=true,['--kernel-dir']=true})
    assert((opts.json and 1 or 0)+(opts.paths and 1 or 0)+(opts.source and 1 or 0)<=1,
      'choose one of --json, --paths, --source')
    if opts.kind then
      opts.kind=opts.kind:lower()
      assert(opts.kind=='analysis' or opts.kind=='rewrite' or opts.kind=='builtin','invalid kernel kind')
    end
    if opts['kernel-dir'] then api.load_kernels(opts['kernel-dir']) end
    local items=catalog.search(lkopt,{keywords=opts.search,tags=opts.tags,use=opts.use,kind=opts.kind})
    if opts.json then write('-',json.encode({count=#items,kernels=items})..'\n')
    elseif opts.paths then for _,k in ipairs(items) do io.write(k.path..'\n') end
    elseif opts.source then for _,k in ipairs(items) do io.write('-- '..k.path..'\n'..read(k.path)..'\n') end
    else
      io.write(#items..' Optikern kernels\n')
      for _,k in ipairs(items) do io.write(string.format('%-39s %-9s %s\n',k.name,k.kind,k.implementation)) end
    end
    return 0
  end
  if cmd=='verify' then
    assert(not arg[2],'usage: metail verify')
    local verified,cases,effects=require('metail.verify').run(lkopt)
    io.write('verified '..verified..' rewrites across '..cases..' signed-64 cases ('..effects..' effect/trap cases)\n')
    return 0
  end
  if cmd=='selftest' then
    assert(not arg[2],'usage: metail selftest')
    local src='module demo; fn main(a) { entry: let x = add(a,0); let y = mul(x,2); return y; }'
    local p=parser.program(src,'<selftest>');validate(p)
    assert(parser.program(ir.emit(p),'<emit>').functions[1].name=='main')
    assert(#lkopt.order>200,'expected at least 201 kernels')
    io.write('selftest ok ('..#lkopt.order..' kernels)\n');return 0
  end
  assert(cmd=='optimize' or cmd=='analyze','unknown command '..cmd)
  assert(arg[2], 'usage: metail '..cmd..' FILE')
  local allowed={['--out']=true,['--passes']=true,['--kernel-dir']=true}
  if cmd=='optimize' then allowed['--repeat']=true;allowed['--stats']=true end
  local opts=options(3,allowed)
  if opts['kernel-dir'] then api.load_kernels(opts['kernel-dir']) end
  local p=parser.program(read(arg[2]),arg[2]);validate(p)
  if cmd=='analyze' then
    write(opts.out,api.analyze_program(p,opts.passes))
  else
    local rounds=tonumber(opts['repeat'] or '2')
    local output,stats=api.optimize_program(p,opts.passes,rounds)
    write(opts.out,output)
    if opts.stats then write(opts.stats,stats) end
  end
  return 0
end
local ok,code=pcall(execute)
if not ok then io.stderr:write('metail: '..tostring(code)..'\n');return 1 end
return code
