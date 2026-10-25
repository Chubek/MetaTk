-- Discovery metadata is derived from loaded declarations, never from filenames alone.
local M={}
local function words(text)
  return (text:gsub('(%l)(%u)','%1 %2'):gsub('(%u)(%u%l)','%1 %2'):lower())
end
local function append(dst,src)
  for _,value in ipairs(src or {}) do
    local found=false
    for _,old in ipairs(dst) do if old==value then found=true;break end end
    if not found then dst[#dst+1]=value end
  end
end
function M.describe(k)
  local implementation=k.kind=='builtin' and k.builtin_kind or
    k.kind=='analysis' and k.analysis_cfg.kind or k.pattern
  local tags={k.kind,'signed-64','scalar'}
  local uses={k.kind=='analysis' and 'analyze' or 'optimize'}
  local keywords={words(k.name),implementation}
  local description=k.kind=='rewrite' and (k.pattern..' -> '..k.replace) or implementation
  if k.name:match('^Numeric') then append(tags,{'numeric'}) end
  if k.kind=='analysis' and k.analysis_cfg.kind=='numeric-statistic' then
    append(tags,{'statistics','metanum'})
    append(keywords,{words(k.analysis_cfg.source),words(k.analysis_cfg.statistic)})
    append(uses,{'measure numeric statistics'})
  elseif k.kind=='analysis' then
    append(uses,{'inspect '..words(k.name)})
  else
    append(uses,{'simplify expressions'})
  end
  if k.name:find('PowerOfTwo') or k.name:find('MultiplyNegativePower') or k.name=='NumericDoubleAsShift' then
    append(tags,{'strength-reduction'});append(uses,{'replace multiplication with shifts'})
  end
  if k.kind=='analysis' and (implementation=='cfg' or implementation:find('dominator') or
      implementation:find('loop') or implementation:find('block') or implementation:find('edge')) then
    append(tags,{'control-flow'});append(uses,{'inspect control flow'})
  end
  if implementation=='liveness' or implementation=='reaching-definitions' or implementation=='block-def-use' then
    append(tags,{'dataflow'});append(uses,{'inspect variable dataflow'})
  end
  local meta=k.meta or {}
  append(keywords,meta.keywords);append(tags,meta.tags);append(uses,meta.uses)
  return {name=k.name,path=k.path or '',kind=k.kind,implementation=implementation,
    description=meta.description or description,keywords=keywords,tags=tags,uses=uses}
end
local function contains_all(text,query)
  text=text:lower()
  for token in (query or ''):lower():gmatch('%S+') do
    if not text:find(token,1,true) then return false end
  end
  return true
end
function M.matches(k,q)
  if q.kind and q.kind~='' and q.kind:lower()~=k.kind then return false end
  for _,wanted in ipairs(q.tags or {}) do
    local found=false
    for _,tag in ipairs(k.tags) do if tag:lower()==wanted:lower() then found=true;break end end
    if not found then return false end
  end
  return contains_all(table.concat({k.name,k.description,k.implementation,
    table.concat(k.keywords,' '),table.concat(k.tags,' '),table.concat(k.uses,' ')},' '),q.keywords)
    and contains_all(table.concat(k.uses,' '),q.use)
end
function M.search(lkopt,query)
  local items={}
  for _,name in ipairs(lkopt.order) do
    local k=M.describe(lkopt.registered[name])
    if M.matches(k,query) then items[#items+1]=k end
  end
  table.sort(items,function(a,b) return a.name<b.name end)
  return items
end
return M
