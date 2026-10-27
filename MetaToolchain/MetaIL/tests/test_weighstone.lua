-- Behavioral tests use the independent C signed-64 evaluator, not Weighstone.
local parser=require('metail.parser')
local ir=require('metail.ir')
local analysis=require('metail.analysis')
local transform=require('metail.transform')
local api=require('metail.api')
local native=require('weighstone_native')
local function expr(s) return transform.fold_expr(parser.expression(s)) end
local function optimize(n,pass)
  local p={functions={{name='f',params={'x','y'},statements={{kind='return',expr=ir.copy(n)}}}}}
  local stats=transform.apply(p,{name=pass,kind='builtin',builtin_kind='weighstone-'..pass})
  return p.functions[1].statements[1].expr,stats.changes
end
local function evaluate(n,env,trace)
  if n.tag=='int' then return n.value end
  if n.tag=='var' then return assert(env[n.name]) end
  if n.tag=='call' then
    for _,a in ipairs(n.args) do local v,e=evaluate(a,env,trace);if v==nil then return nil,e end end
    trace[#trace+1]=n.name
    return env.x+#trace*7
  end
  local a,e=evaluate(n.args[1],env,trace)
  if a==nil then return nil,e end
  if n.op=='select' then return evaluate(n.args[a~=0 and 2 or 3],env,trace) end
  local b
  if n.args[2] then b,e=evaluate(n.args[2],env,trace);if b==nil then return nil,e end end
  return lkopt_native.eval(n.op,a,b)
end
local values={math.mininteger,math.mininteger+1,math.maxinteger,math.maxinteger-1,0,1,-1,2,-2,63,64,65,-65,255,256,-256}
for width=1,62 do
  local p=1<<width
  for _,v in ipairs({p-1,p,p+1,-p-1,-p,-p+1}) do values[#values+1]=v end
end
local seed=0x1234567
local function random()
  seed=seed*6364136223846793005+1442695040888963407
  return seed
end
for _=1,128 do values[#values+1]=random() end
local cases,rewrites=0,0
local function equivalent(before,after,vectors)
  for _,x in ipairs(vectors or values) do
    local env,at,bt={x=x,y=~x},{},{}
    local a,ae=evaluate(before,env,at)
    local b,be=evaluate(after,env,bt)
    assert(a==b and ae==be and table.concat(at,',')==table.concat(bt,','),
      'semantics: '..ir.format(before)..' => '..ir.format(after)..' at '..x)
    cases=cases+1
  end
end
local function check(pass,source,expected)
  local before=expr(source)
  local after,changes=optimize(before,pass)
  if expected then
    assert(ir.same(after,expr(expected)),pass..': '..source..' => '..ir.format(after)..' expected '..expected)
    assert(changes>0,'rewrite did not fire: '..source)
    rewrites=rewrites+1
  else assert(ir.same(before,after),'unknown/unsafe rewrite fired: '..source..' => '..ir.format(after)) end
  equivalent(before,after)
end
for _,t in ipairs({
  {'known-bits','band(bor(x,73),73)','73'},
  {'known-bits','band(band(x,73),127)','band(x,73)'},
  {'known-bits','band(127,band(x,73))','band(x,73)'},
  {'known-bits','bor(bor(x,73),9)','bor(x,73)'},
  {'known-bits','bor(9,bor(x,73))','bor(x,73)'},
  {'known-bits','bxor(x,band(y,0))','x'},
  {'known-bits','band(shl(x,13),8191)','0'},
  {'known-bits','band(mul(x,48),15)','0'},
  {'known-bits','band(add(shl(x,5),shl(y,7)),31)','0'},
  {'known-bits','band(sub(shl(x,5),shl(y,7)),31)','0'},
  {'known-bits','band(neg(shl(x,9)),511)','0'},
  {'known-bits','band(add(mul(x,8),3),7)','3'},
  {'known-bits','band(sub(mul(x,8),3),7)','5'},
  {'known-bits','band(mul(bor(shl(x,3),3),5),7)','7'},
  {'known-bits','band(neg(bor(shl(x,3),3)),7)','5'},
  {'known-bits','band(add(mul(tick(),8),3),7)'},
  {'known-bits','band(add(mul(x,4),3),7)'},
  {'known-bits','band(bor(tick(),73),73)'},
  {'known-bits','band(shl(x,64),0)'},
  {'known-bits','band(div(x,0),0)'},
  {'known-bits','band(x,73)'},
  {'ranges','lt(band(x,511),512)','1'},
  {'ranges','le(band(x,511),511)','1'},
  {'ranges','gt(band(x,511),511)','0'},
  {'ranges','ge(band(x,511),0)','1'},
  {'ranges','eq(bor(x,8),0)','0'},
  {'ranges','ne(bor(x,8),0)','1'},
  {'ranges','lt(add(band(x,255),9223372036854775807),0)'},
  {'ranges','lt(add(x,1),x)'},
  {'ranges','lt(band(tick(),511),512)'},
  {'ranges','lt(band(div(x,0),511),512)'},
  {'ranges','lt(x,y)'},
  {'ranges','select(bor(x,4),tick(),div(x,0))','tick()'},
  {'ranges','select(band(x,0),div(x,0),tick())','tick()'},
  {'ranges','select(bor(tick(),4),x,y)'},
  {'ranges','select(x,tick(),y)'},
  {'ranges','div(band(x,255),-1)','neg(band(x,255))'},
  {'ranges','div(x,-1)'},
  {'ranges','mod(sub(band(x,15),7),9)','sub(band(x,15),7)'},
  {'ranges','mod(band(x,255),-9223372036854775808)','band(x,255)'},
  {'ranges','mod(x,-9223372036854775808)'},
  {'ranges','mod(x,0)'},
  {'ranges','mod(x,-1)'},
  {'ranges','div(band(x,15),16)','0'},
  {'ranges','mod(add(mul(band(x,255),8),3),8)','3'},
  {'ranges','mod(shl(x,5),32)','0'},
  {'ranges','mod(add(mul(x,8),3),8)'},
  {'ranges','land(x,bor(y,4))','ne(x,0)'},
  {'ranges','land(bor(y,4),x)','ne(x,0)'},
  {'ranges','lor(x,band(y,0))','ne(x,0)'},
  {'ranges','lor(band(y,0),x)','ne(x,0)'},
  {'ranges','land(x,bor(tick(),4))'},
  {'ranges','lor(x,band(tick(),0))'},
  {'ranges','eq(lt(x,y),1)','lt(x,y)'},
  {'ranges','ne(lt(x,y),0)','lt(x,y)'},
  {'ranges','eq(lt(x,y),0)','lnot(lt(x,y))'},
  {'ranges','ne(lt(x,y),1)','lnot(lt(x,y))'},
  {'ranges','eq(x,1)'},
  {'strength','mul(tick(),-1024)','neg(shl(tick(),10))'},
  {'strength','mul(-9223372036854775808,tick())','shl(tick(),63)'},
  {'strength','div(band(x,1023),32)','shr(band(x,1023),5)'},
  {'strength','div(band(x,1023),-32)','neg(shr(band(x,1023),5))'},
  {'strength','div(shl(x,5),32)','shr(shl(x,5),5)'},
  {'strength','mod(shl(x,5),32)','0'},
  {'strength','mod(band(x,1023),-32)','band(band(x,1023),31)'},
  {'strength','mod(band(tick(),1023),32)','band(band(tick(),1023),31)'},
  {'strength','div(x,32)'},
  {'strength','mod(x,32)'},
  {'strength','div(x,-1)'},
  {'strength','mod(shl(tick(),5),32)'},
  {'strength','mul(x,1023)'},
  {'shifts','shl(shl(x,31),32)','shl(x,63)'},
  {'shifts','shl(shl(x,32),32)','0'},
  {'shifts','shr(shr(tick(),62),63)','shr(tick(),63)'},
  {'shifts','shl(shr(x,63),63)','band(x,-9223372036854775808)'},
  {'shifts','shr(shl(band(tick(),1023),54),54)'},
  {'shifts','shr(shl(band(tick(),1023),53),53)','band(tick(),1023)'},
  {'shifts','shr(shl(sub(band(x,1),1),63),63)','sub(band(x,1),1)'},
  {'shifts','shr(shl(x,1),1)'},
  {'shifts','shl(shl(tick(),32),32)'},
  {'shifts','shr(shr(x,64),1)'},
  {'shifts','shl(shr(x,-1),-1)'},
  {'shifts','band(shr(shl(tick(),55),55),511)','band(tick(),511)'},
  {'shifts','band(shr(shl(x,55),55),1023)'},
  {'shifts','band(shr(shl(x,64),64),1)'},
  {'algebra','add(7,add(tick(),9223372036854775807))','add(tick(),-9223372036854775802)'},
  {'algebra','mul(7,mul(5,tick()))','mul(tick(),35)'},
  {'algebra','band(7,band(5,tick()))','band(tick(),5)'},
  {'algebra','bor(7,bor(9,tick()))','bor(tick(),15)'},
  {'algebra','bxor(7,bxor(5,tick()))','bxor(tick(),2)'},
  {'algebra','sub(sub(tick(),9223372036854775807),2)','add(tick(),9223372036854775807)'},
  {'algebra','add(sub(tick(),12),3)','add(tick(),-9)'},
  {'algebra','sub(add(tick(),12),3)','add(tick(),9)'},
  {'algebra','add(mul(x,7),mul(x,9))','mul(x,16)'},
  {'algebra','sub(shl(x,63),x)','mul(x,9223372036854775807)'},
  {'algebra','add(mul(tick(),7),mul(tick(),9))'},
  {'algebra','add(mul(div(x,0),7),mul(div(x,0),9))'},
  {'algebra','add(mul(x,7),mul(y,9))'},
  {'algebra','eq(add(tick(),9223372036854775807),1)','eq(tick(),-9223372036854775806)'},
  {'algebra','ne(sub(tick(),7),3)','ne(tick(),10)'},
  {'algebra','eq(bxor(tick(),7),3)','eq(tick(),4)'},
  {'algebra','lt(add(x,7),3)'},
  {'algebra','eq(mul(tick(),3),9)','eq(tick(),3)'},
  {'algebra','ne(mul(-3,tick()),9)','ne(tick(),-3)'},
  {'algebra','eq(mul(x,3),1)','eq(x,-6148914691236517205)'},
  {'algebra','eq(mul(x,2),8)'},
  {'algebra','eq(neg(tick()),-9223372036854775808)','eq(tick(),-9223372036854775808)'},
  {'algebra','ne(bnot(tick()),9223372036854775807)','ne(tick(),-9223372036854775808)'},
  {'algebra','eq(sub(7,tick()),3)','eq(tick(),4)'},
  {'algebra','eq(add(7,tick()),3)','eq(tick(),-4)'},
  {'algebra','eq(bxor(7,tick()),3)','eq(tick(),4)'},
  {'algebra','lt(neg(x),3)'},
  {'algebra','div(div(tick(),3),7)','div(tick(),21)'},
  {'algebra','div(div(x,9223372036854775807),2)'},
  {'algebra','div(div(x,-1),2)'},
  {'algebra','div(div(x,3),0)'},
  {'algebra','mod(mod(tick(),21),7)','mod(tick(),7)'},
  {'algebra','mod(mod(x,20),7)'},
  {'algebra','mod(mod(x,-1),7)'},
}) do check(table.unpack(t)) end
for op,inverse in pairs({eq='ne',ne='eq',lt='ge',le='gt',gt='le',ge='lt'}) do
  check('algebra','lnot('..op..'(tick(),tock()))',inverse..'(tick(),tock())')
end
-- All valid shift widths, including saturation boundaries and sign bit.
for width=1,63 do
  check('strength','mul(x,'..tostring(1<<width)..')','shl(x,'..width..')')
  check('shifts','shl(shr(x,'..width..'),'..width..')','band(x,'..tostring(-1<<width)..')')
end

-- Check inferred ranges and bits against actual successful evaluations, including
-- wraps at both signed edges. Unknown/trapping results must not become proofs.
local function contains(f,v)
  return v>=f.lo and v<=f.hi and (v & f.zero)==0 and (v & f.one)==f.one
end
local operators={'add','sub','mul','div','mod','band','bor','bxor','shl','shr','eq','ne','lt','le','gt','ge','land','lor'}
local function facts(vals)
  local f={lo=math.maxinteger,hi=math.mininteger,zero=-1,one=-1}
  for _,v in ipairs(vals) do f.lo=math.min(f.lo,v);f.hi=math.max(f.hi,v);f.zero=f.zero & ~v;f.one=f.one & v end
  return f
end
for trial=1,300 do
  local xs,ys={},{}
  for i=1,4 do
    xs[i]=values[(random() & 0x7fffffff)%#values+1]
    ys[i]=values[(random() & 0x7fffffff)%#values+1]
  end
  if trial%3==0 then xs={trial,trial+1,trial+2};ys={trial%64} end
  for _,op in ipairs(operators) do
    local f=native.transfer(op,facts(xs),facts(ys))
    for _,x in ipairs(xs) do for _,y in ipairs(ys) do
      local v=lkopt_native.eval(op,x,y)
      assert(v==nil and not f.total or v~=nil and contains(f,v),op..': unsound facts')
      cases=cases+1
    end end
  end
end
for _,args in ipairs({{'bogus',nil,nil},{'add',{lo=2,hi=1,zero=0,one=0},nil},
    {'add',{lo=0,hi=1,zero=1,one=1},nil},{'add',{lo=0.5,hi=1,zero=0,one=0},nil}}) do
  assert(not pcall(native.transfer,args[1],args[2],args[3]))
end
assert(native.inverse(2)==nil and native.inverse(0)==nil and native.inverse(math.mininteger)==nil)
assert(not pcall(native.inverse,1.5) and not pcall(native.inverse,'3'))
for _,v in ipairs(values) do
  if (v & 1)~=0 then assert(lkopt_native.eval('mul',v,native.inverse(v))==1) end
  for _,op in ipairs({'neg','bnot','lnot'}) do
    local f=native.transfer(op,facts({v,v+1}),nil)
    assert(contains(f,lkopt_native.eval(op,v)))
    assert(contains(f,lkopt_native.eval(op,v+1)))
  end
end

-- Local propagation uses value facts, not expressions depending on reassigned
-- variables. Labels, branch targets and loop headers invalidate the environment.
local source=[[fn f(x) {
 let a = x & 255;
 let x = -1;
 let b = a < 256;
 let a = x;
 let c = a < 0;
 goto join;
join:
 return a < 256;
}]]
local p=api.parse(source)
transform.apply(p,{name='ranges',kind='builtin',builtin_kind='weighstone-ranges'})
assert(p.functions[1].statements[3].expr.value==1)
assert(p.functions[1].statements[5].expr.value==1)
assert(p.functions[1].statements[8].expr.tag=='op')
local data=analysis.run(api.parse('fn f(x) { let a = x & 255; return a / 16; }'),{kind='integer-facts'})
assert(data.f[1].min==0 and data.f[1].max==255 and data.f[2].max==15)
local effect=analysis.integer_facts(expr('band(tick(),0)'))
assert(effect.lo==0 and effect.hi==0 and not effect.pure)

-- Fixed-seed expression fuzzing tests every family and their composition against
-- the evaluator. Calls, lazy arms, invalid shifts and divisors are included.
local leaves={'x','y','tick()','0','1','-1','63','64','-9223372036854775808','9223372036854775807'}
local function tree(depth)
  if depth==0 or (random() & 3)==0 then return expr(leaves[(random() & 0x7fffffff)%#leaves+1]) end
  local which=(random() & 0x7fffffff)%24
  if which<18 then return {tag='op',op=operators[which+1],args={tree(depth-1),tree(depth-1)}} end
  if which<21 then return {tag='op',op=({'neg','bnot','lnot'})[which-17],args={tree(depth-1)}} end
  return {tag='op',op='select',args={tree(depth-1),tree(depth-1),tree(depth-1)}}
end
for _=1,400 do
  local before=tree(4)
  local after=before
  for _,pass in ipairs({'algebra','known-bits','ranges','shifts','strength'}) do
    local single=optimize(before,pass)
    equivalent(before,single,{math.mininteger,math.maxinteger,0,-1,1,random()})
    after=optimize(after,pass)
  end
  equivalent(before,after,{math.mininteger,math.maxinteger,0,-1,1,random()})
end
-- Compare against exactly the pre-Weighstone pipeline, including its cleanup.
-- This is an IR-operation count, not a claim about a particular machine's speed.
local file=assert(io.open(_METAIL_ROOT..'/examples/weighstone.mi'))
local corpus=file:read('*a');file:close()
local legacy,modern=api.parse(corpus),api.parse(corpus)
local kernels=api.passes(nil,'transform')
for _=1,2 do
  for _,k in ipairs(kernels) do
    if not k.name:match('^Weighstone') then transform.apply(legacy,k) end
  end
  for _,kind in ipairs({'constant-folding','simplify-branches','dead-assignments','unreachable-code'}) do
    transform.apply(legacy,{name='cleanup',kind='builtin',builtin_kind=kind})
  end
end
modern=api.optimize_ir(modern,nil,2)
local function execute(f,x)
  local env,trace={x=x},{}
  for _,s in ipairs(f.statements) do
    local v,e=evaluate(s.expr,env,trace)
    if v==nil or s.kind=='return' then return v,e,table.concat(trace,',') end
    assert(s.kind=='let','unexpected example statement')
    env[s.name]=v
  end
end
for i,f in ipairs(legacy.functions) do
  for _,x in ipairs(values) do
    local a,ae,at=execute(f,x)
    local b,be,bt=execute(modern.functions[i],x)
    assert(a==b and ae==be and at==bt,'pipeline semantics: '..f.name)
    cases=cases+1
  end
end
local function count(p)
  local n=0
  for _,f in ipairs(p.functions) do for _,s in ipairs(f.statements) do if s.expr then
    ir.walk(s.expr,function(x) if x.tag=='op' then n=n+1 end end)
  end end end
  return n
end
assert(count(modern)<count(legacy),'Weighstone corpus did not improve')
print(string.format('Weighstone: %d firing regressions, %d boundary/random fact and equivalence checks',rewrites,cases))
print(string.format('Example IR operators: legacy %d -> Weighstone %d',count(legacy),count(modern)))
