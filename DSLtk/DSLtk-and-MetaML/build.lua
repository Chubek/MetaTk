-- Run with texlua build.lua [build|clean|check]. No shell escape is needed.
local lfs = require('lfs')
local script = arg[0]:gsub('\\', '/')
local directory = script:match('^(.*)/[^/]+$')
if directory and directory ~= '' then assert(lfs.chdir(directory)) end
local action = arg[1] or 'build'
local function command(s)
  print('> '..s)
  local a, b, c = os.execute(s)
  if a ~= true and a ~= 0 then error('Command failed: '..s..' ('..tostring(c or b or a)..')') end
end
local function copy(source, target)
  local f=assert(io.open(source,'rb')); local data=f:read('*a'); f:close()
  local g=assert(io.open(target,'wb')); assert(g:write(data)); g:close()
end
if action == 'clean' then
  if lfs.attributes('build','mode')=='directory' then
    for name in lfs.dir('build') do
      if name~='.' and name~='..' and lfs.attributes('build/'..name,'mode')=='file' then
        assert(os.remove('build/'..name))
      end
    end
  end
elseif action == 'check' then
  command('python3 tools/check_book.py')
elseif action == 'build' then
  if not lfs.attributes('build') then assert(lfs.mkdir('build')) end
  for pass=1,3 do
    command('lualatex -no-shell-escape -interaction=nonstopmode -halt-on-error -file-line-error -output-directory=build main.tex')
  end
  local f=assert(io.open('build/main.log','rb')); local log=f:read('*a'); f:close()
  if log:find('Overfull \\hbox',1,true) or log:find('Overfull \\vbox',1,true) then
    error('Layout overflow detected; inspect build/main.log')
  end
  copy('build/main.pdf','DSLtk-and-MetaML.pdf')
  print('Built build/main.pdf and DSLtk-and-MetaML.pdf')
else
  error('Usage: texlua build.lua [build|clean|check]')
end
