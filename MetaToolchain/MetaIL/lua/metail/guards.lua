local ir=require('metail.ir')
local G={}
local predicates={
  const=function(n) return n.tag=='int' end,
  nonzero=function(n) return n.tag=='int' and n.value~=0 end,
  positive=function(n) return n.tag=='int' and n.value>0 end,
  nonnegative=function(n) return n.tag=='int' and n.value>=0 end,
  ['power-of-two']=function(n) return n.tag=='int' and n.value>0 and (n.value & (n.value-1))==0 end,
  pure=ir.pure,
  boolean=function(n) return n.tag=='int' and (n.value==0 or n.value==1) end,
}
function G.parse(text,holes)
  assert(type(text)=='string','guard must be a string')
  local out={}
  if text=='' then return out end
  for clause in (text..';'):gmatch('(.-);') do
    local kind,names=clause:match('^%s*([%w_-]+)%s*:%s*(.-)%s*$')
    assert(kind and predicates[kind],'invalid or unknown guard: '..clause)
    for name in (names..','):gmatch('(.-),') do
      name=name:match('^%s*(.-)%s*$')
      assert(name:match('^[%a_][%w_]*$') and holes['$'..name],'unknown guard capture: '..name)
      out[#out+1]={kind=kind,name='$'..name}
    end
  end
  return out
end
function G.test(clauses,env)
  for _,c in ipairs(clauses) do
    if not predicates[c.kind](assert(env[c.name],'missing capture '..c.name)) then return false end
  end
  return true
end
return G
