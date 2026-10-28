local parser=require('metail.parser')
local transform=require('metail.transform')
local ir=require('metail.ir')
local function optimize(source)
  local p=parser.program(source)
  transform.apply(p,{name='DeadAssignmentElimination',kind='builtin',builtin_kind='dead-assignments'})
  return p.functions[1].statements
end
local s=optimize('fn f() { let x=1; let x=2; return x; }')
assert(#s==2 and s[1].expr.value==2,'overwritten assignment retained')
s=optimize('fn f() { let x=tick(); let x=div(1,0); let x=2; return x; }')
assert(#s==4 and s[1].kind=='eval' and s[2].kind=='eval','effect or trap removed')
s=optimize('fn f(c) { let x=1; br c,a,b; a: let x=2; goto end; b: goto end; end: return x; }')
assert(s[1].kind=='let' and s[1].expr.value==1,'definition on join path removed')
s=optimize('fn f(n) { let x=0; loop: br n,body,end; body: let x=x+1; let n=n-1; goto loop; end: return x; }')
local lets=0;for _,stmt in ipairs(s) do if stmt.kind=='let' then lets=lets+1 end end
assert(lets==3,'loop-carried definitions removed')
local chain={'fn f(x) {'}
for i=1,200 do chain[#chain+1]='let v'..i..'='..(i==1 and 'x' or 'v'..(i-1))..';' end
chain[#chain+1]='return 0; }'
s=optimize(table.concat(chain,'\n'))
assert(#s==1 and s[1].kind=='return','long dead chain retained')
s=optimize('fn f(x) { let a=x; let b=tick(a); return 0; }')
assert(#s==3 and s[1].kind=='let' and s[2].kind=='eval' and ir.vars(s[2].expr).a,
  'dependency of effectful expression removed')
print('CFG dead-assignment tests passed')
