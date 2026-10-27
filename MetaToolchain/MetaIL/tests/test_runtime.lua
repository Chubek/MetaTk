local root=assert(_METAIL_ROOT)
local lkopt=require('lkopt')
local parser=require('metail.parser')
local ir=require('metail.ir')
local function fails(fn,pattern)
  local ok,err=pcall(fn)
  assert(not ok,'expected failure')
  assert(tostring(err):match(pattern),tostring(err))
end
local function file(path,contents)
  local f=assert(io.open(path,'w'));assert(f:write(contents));assert(f:close())
end
local tmp=assert(arg[3],'temporary test directory required')
local path=tmp..'/trial.optk'
for _,declaration in ipairs({
  'K:rewrite {pattern="add($x,0)",replace="$y"}',
  'K:rewrite {pattern="add($x,0)",replace="$x",guard="pure:typo"}',
  'K:rewrite {pattern="add($x,0)",replace="$x",guard="pure:"}',
  'K:rewrite {pattern="add($x,0)",replace="$x",guard="purity:x"}',
  'K:rewrite {pattern="add($x,0)",replace="$x",guard="pure:x,,x"}',
  'K:rewrite {pattern="$x",replace="$x"}',
  'K:analysis {kind="nonexistent"}',
  'K:analysis {kind="numeric-statistic",source="wrong",statistic="Mean"}',
  'K:builtin "nonexistent"',
  'local other=lkopt.optimizer.new "Second"; other:builtin "constant-folding"',
  '',
}) do
  file(path,'local K=lkopt.optimizer.new "Trial"\n'..declaration)
  fails(function()lkopt.load(path)end,'trial.optk')
  assert(#lkopt.order==0 and next(lkopt.registered)==nil,'failed load polluted registry')
end
file(path,'local K=lkopt.optimizer.new "Trial"; K:builtin "constant-folding"; math.abs=nil; lkopt.numeric.mean=nil')
lkopt.load(path)
assert(math.abs(-1)==1 and lkopt.numeric.mean({1,3})==2)
fails(function()lkopt.load(path)end,'duplicate')
assert(#lkopt.order==1)
os.remove(path)
file(tmp..'/a.optk','local K=lkopt.optimizer.new "Rollback"; K:builtin "constant-folding"')
file(tmp..'/b.optk','assert(false,"broken")')
fails(function()lkopt.load_directory(tmp)end,'broken')
assert(#lkopt.order==1 and not lkopt.registered.Rollback)
os.remove(tmp..'/a.optk');os.remove(tmp..'/b.optk')
assert(#lkopt_native.scan(tmp)==0)
lkopt.load_directory(root..'/optimize-kernels')
assert(#lkopt.order==328)
assert(not ir.pure(parser.expression('div(x,y)')))
assert(not ir.pure(parser.expression('mod(x,-1)')))
assert(ir.pure(parser.expression('div(x,2)')))
assert(ir.pure(parser.expression('select(1,7,tick())')))
assert(lkopt_native.eval('shr',math.mininteger,63)==-1)
assert(lkopt_native.eval('shl',1,63)==math.mininteger)
assert(lkopt_native.eval('add',math.maxinteger,1)==math.mininteger)
assert(lkopt_native.eval('div',-7,2)==-3 and lkopt_native.eval('mod',-7,2)==-1)
assert(lkopt_native.eval('add',1.5,2)==nil)
assert(lkopt_native.eval('add\0ignored',1,2)==nil)
print('OK: transactional kernel loading, guard validation, API isolation, purity, integer boundaries')
