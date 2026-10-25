package.path=arg[1]..'/lua/?.lua;'..package.path
package.cpath=arg[2]..'/?.so;'..package.cpath
assert(_VERSION=='Lua 5.4')
dofile(arg[1]..'/tests/test_numeric.lua')
