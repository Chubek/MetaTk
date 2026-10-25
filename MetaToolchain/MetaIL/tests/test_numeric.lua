local num=require('metanum')
local lkopt=require('lkopt')
local json=require('metail.json')
for _,value in ipairs({5/3,1e308,1e-300,math.mininteger,math.maxinteger}) do
  assert(tonumber(json.encode(value))==value,'JSON lost numeric precision')
end
assert(lkopt.numeric==num)
local function close(a,b,tolerance)
  assert(math.abs(a-b)<=(tolerance or 1e-14)*math.max(math.abs(b),1e-300),tostring(a)..' != '..tostring(b))
end
local function fails(fn,pattern)
  local ok,err=pcall(fn)
  assert(not ok,'expected error')
  assert(tostring(err):match(pattern),tostring(err))
end
local x={1,2,3,4}
close(num.mean(x),2.5)
close(num.variance(x),5/3)
close(num.variance(x,false),1.25)
close(num.standard_deviation(x),math.sqrt(5/3))
close(num.standard_deviation(x,false),math.sqrt(1.25))
close(num.covariance(x,{2,4,6,8}),10/3)
close(num.covariance(x,{2,4,6,8},false),2.5)
close(num.correlation(x,{8,6,4,2}),-1)
close(num.median({4,2,3,1}),2.5)
close(num.quantile(x,0.25),1.75)
close(num.quantile(x,0),1)
close(num.quantile(x,1),4)
assert(num.min(x)==1 and num.max(x)==4)
assert(table.concat(x,',')=='1,2,3,4')
assert(num.variance({7},false)==0)
assert(num.mean({1e308,1e308})==1e308)
assert(num.median({-1e308,1e308})==0)
assert(num.variance({-1e308,1e308},false)==math.huge)
close(num.standard_deviation({-1e308,1e308},false),1e308)
close(num.standard_deviation({-1e-300,1e-300},false),1e-300)
close(num.correlation({-1e308,1e308},{-1e-300,1e-300}),1)
assert(num.mean({math.huge,1})==math.huge)
local nan=0/0
for _,fn in ipairs({num.mean,num.variance,num.standard_deviation,num.median,num.min,num.max}) do
  local value=fn({1,nan});assert(value~=value)
end
assert(num.quantile({1,nan},0.5)~=num.quantile({1,nan},0.5))
fails(function()num.mean({})end,'non%-empty')
fails(function()num.variance({1})end,'requires')
fails(function()num.correlation({1,1},{2,3})end,'constant')
fails(function()num.covariance({1,2},{3})end,'differ')
for _,p in ipairs({-1,2,nan}) do fails(function()num.quantile(x,p)end,'p must') end
for _,bad in ipairs({false,'hello',{1,'2'},{[1]=1,[3]=3},{1,2,foo=3},{[1]=1,['2']=2,[3]=3}}) do
  fails(function()num.mean(bad)end,'array')
end
fails(function()num.variance(x,0)end,'boolean')
fails(function()num.quantile(x,'0.5')end,'numeric')
local metadata=num.catalog()
assert(#metadata==40)
local count=0
for _,category in ipairs(metadata) do
  assert(category.callable==false and type(category.id)=='string')
  for _,op in ipairs(category.operations) do
    count=count+1
    assert(#op.name>0 and #op.symbols>0 and #op.description>0)
  end
end
assert(count==298)
metadata[1].id='changed'
assert(num.catalog()[1].id~='changed')
-- Independently load the shared object through Lua's standard module ABI.
if arg[3] then
  local open=assert(package.loadlib(arg[3],'luaopen_metanum_native'))
  local standalone=open()
  assert(standalone.compute('mean',{2,4},nil,nil)==3)
  assert(#standalone.catalog()==40)
end
print('OK: MetaNum numerical results, shape/type/domain errors, extremes, metadata, module ABI')
