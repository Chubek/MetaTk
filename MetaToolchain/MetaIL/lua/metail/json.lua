local M={}
local function escaped(s)
  return '"'..s:gsub('[%z\1-\31\\"]',function(c)
    local map={['"']='\\"',['\\']='\\\\',['\n']='\\n',['\r']='\\r',['\t']='\\t'}
    return map[c] or string.format('\\u%04x',c:byte())
  end)..'"'
end
local function array(t)
  local count,max=0,0
  for k in pairs(t) do
    if type(k)~='number' or k<1 or k%1~=0 then return false end
    count=count+1;max=math.max(max,k)
  end
  return count==max
end
local function serialize(v,depth)
  depth=depth or 0;assert(depth<100,'JSON nesting exceeds limit')
  local ty=type(v)
  if ty=='nil' then return 'null'
  elseif ty=='string' then return escaped(v)
  elseif ty=='boolean' then return tostring(v)
  elseif ty=='number' then
    assert(v==v and v~=math.huge and v~=-math.huge,'non-finite JSON number')
    if math.type(v)=='integer' then return tostring(v) end
    -- Lua's tostring uses too few digits for binary64 round trips. JSON always
    -- uses a decimal point, including when an embedding host changes the locale.
    return (string.format('%.17g',v):gsub(',','.'))
  elseif ty=='table' then
    local xs={}
    if array(v) then for i=1,#v do xs[#xs+1]=serialize(v[i],depth+1) end
      return '['..table.concat(xs,',')..']'
    else
      local ks={};for k in pairs(v) do ks[#ks+1]=k end
      table.sort(ks,function(a,b)return tostring(a)<tostring(b) end)
      for _,k in ipairs(ks) do xs[#xs+1]=escaped(tostring(k))..':'..serialize(v[k],depth+1) end
      return '{'..table.concat(xs,',')..'}'
    end
  end
  error('cannot encode '..ty)
end
M.encode=serialize
return M
