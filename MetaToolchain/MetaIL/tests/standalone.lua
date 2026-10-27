package.path=arg[1]..'/lua/?.lua;'..package.path
package.cpath=arg[2]..'/?.so;'..package.cpath
assert(_VERSION=='Lua 5.4')
dofile(arg[1]..'/tests/test_numeric.lua')
local weighstone=require('weighstone_native')
local f=weighstone.transfer('band',nil,{lo=511,hi=511,zero=~511,one=511})
assert(f.lo==0 and f.hi==511 and f.total)
assert(weighstone.inverse(3)==-6148914691236517205 and weighstone.inverse(2)==nil)
