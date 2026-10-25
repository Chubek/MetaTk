local ir=require('metail.ir')
local numeric=require('metanum')
local A={}
local statistics={
  Mean={op='mean',minimum=1},
  SampleVariance={op='variance',sample=true,minimum=2},
  PopulationVariance={op='variance',sample=false,minimum=1},
  SampleStdDev={op='standard_deviation',sample=true,minimum=2},
  PopulationStdDev={op='standard_deviation',sample=false,minimum=1},
  Median={op='median',minimum=1},Minimum={op='min',minimum=1},Maximum={op='max',minimum=1},
  Quantile10={op='quantile',p=0.10,minimum=1},Quantile25={op='quantile',p=0.25,minimum=1},
  Quantile75={op='quantile',p=0.75,minimum=1},Quantile90={op='quantile',p=0.90,minimum=1},
}
local kinds={}
for name in ([=[operator-count statement-count instruction-count function-count node-count
maximum-expression-depth literal-histogram variable-uses definitions unused-definitions
call-graph impure-call-sites cfg reachable-blocks unreachable-blocks dominators postdominators
liveness reaching-definitions natural-loops critical-edges cyclomatic-complexity loop-headers
block-def-use expression-redundancy constant-branches literal-range parameter-uses branch-targets
terminal-blocks opcode-histogram statement-histogram expression-purity numeric-statistic]=]):gmatch('%S+') do
  kinds[name]=true
end
function A.validate(cfg)
  assert(type(cfg)=='table' and kinds[cfg.kind],'unknown analysis: '..tostring(cfg and cfg.kind))
  if cfg.kind=='operator-count' then assert(ir.operators[cfg.op],'unknown analysis operator') end
  if cfg.kind=='statement-count' then
    assert(({let=true,['return']=true,eval=true,assume=true,br=true,['goto']=true,label=true})[cfg.statement],
      'unknown analysis statement')
  end
  if cfg.kind=='numeric-statistic' then
    assert(cfg.source=='Literals' or cfg.source=='ExpressionSizes','unknown numeric data source')
    assert(statistics[cfg.statistic],'unknown numeric statistic')
  end
end
local function sorted_keys(t)
  local a={};for k in pairs(t) do a[#a+1]=k end;table.sort(a);return a
end
local function exprs_in_stmt(s)
  local out={};if s.expr then ir.walk(s.expr,function(n) out[#out+1]=n end) end
  return out
end
local function union(dst,src)
  local changed=false
  for k in pairs(src or {}) do if not dst[k] then dst[k]=true;changed=true end end
  return changed
end
local function copy(t)
  local r={};for k,v in pairs(t) do r[k]=v end;return r
end
local function difference(a,b)
  local d={};for k in pairs(a) do if not b[k] then d[k]=true end end;return d
end
local function equal(a,b)
  for k,v in pairs(a) do if v~=b[k] then return false end end
  for k,v in pairs(b) do if v~=a[k] then return false end end
  return true
end
local function intersect(a,b)
  local r={};for k in pairs(a) do if b[k] then r[k]=true end end;return r
end
function A.cfg(fn)
  local ss=fn.statements
  local marks={[1]=true};local index_label={}
  for i,s in ipairs(ss) do
    if s.kind=='label' then
      if index_label[s.name] then error('duplicate label '..s.name..' in '..fn.name) end
      index_label[s.name]=i;marks[i]=true
    end
    if (s.kind=='goto' or s.kind=='br' or s.kind=='return') and i<#ss then marks[i+1]=true end
  end
  local starts=sorted_keys(marks)
  if #ss==0 then return {blocks={},reachable={},by_label={}} end
  local blocks,by_label={},{}
  for i,start in ipairs(starts) do
    local b={id=i,first=start,last=(starts[i+1] or (#ss+1))-1,succ={},pred={}}
    b.name=(ss[start].kind=='label') and ss[start].name or ('bb'..i)
    blocks[i]=b
    for j=b.first,b.last do if ss[j].kind=='label' then by_label[ss[j].name]=i end end
  end
  for i,b in ipairs(blocks) do
    local last=ss[b.last]
    local names={}
    if last.kind=='goto' then names={last.target}
    elseif last.kind=='br' then names={last.yes,last.no}
    elseif last.kind~='return' and i<#blocks then b.succ={i+1} end
    for _,name in ipairs(names) do
      local dest=by_label[name]
      if not dest then error('unresolved label '..name..' in '..fn.name) end
      if not (dest==b.succ[1] or dest==b.succ[2]) then b.succ[#b.succ+1]=dest end
    end
  end
  for i,b in ipairs(blocks) do
    for _,s in ipairs(b.succ) do blocks[s].pred[#blocks[s].pred+1]=i end
  end
  local reachable={}
  local function visit(i)
    if not i or reachable[i] then return end
    reachable[i]=true
    for _,j in ipairs(blocks[i].succ) do visit(j) end
  end
  visit(1)
  return {blocks=blocks,reachable=reachable,by_label=by_label}
end
local function cfg_to_json(cfg)
  local out={}
  for _,b in ipairs(cfg.blocks) do
    out[#out+1]={name=b.name,first=b.first,last=b.last,
      successors=(function()local z={};for _,i in ipairs(b.succ) do z[#z+1]=cfg.blocks[i].name end;return z end)(),
      predecessors=(function()local z={};for _,i in ipairs(b.pred) do z[#z+1]=cfg.blocks[i].name end;return z end)(),
      reachable=not not cfg.reachable[b.id]}
  end
  return out
end
local function uses_def(fn,cfg)
  local ud={}
  for i,b in ipairs(cfg.blocks) do
    local use,def={},{}
    for j=b.first,b.last do
      local s=fn.statements[j]
      if s.expr then
        for name in pairs(ir.vars(s.expr)) do if not def[name] then use[name]=true end end
      end
      if s.kind=='let' then def[s.name]=true end
    end
    ud[i]={use=use,def=def}
  end
  return ud
end
local function dataflow_liveness(fn,cfg)
  local ud=uses_def(fn,cfg);local input,output={},{}
  for i in ipairs(cfg.blocks) do input[i]={};output[i]={} end
  local changed=true;local loops=0
  while changed do
    changed=false;loops=loops+1
    assert(loops<10000,'liveness did not converge')
    for i=#cfg.blocks,1,-1 do
      local b=cfg.blocks[i];local nextout={}
      for _,j in ipairs(b.succ) do union(nextout,input[j]) end
      local nextin=copy(ud[i].use);union(nextin,difference(nextout,ud[i].def))
      if not equal(nextin,input[i]) or not equal(nextout,output[i]) then changed=true end
      input[i],output[i]=nextin,nextout
    end
  end
  local result={}
  for i,b in ipairs(cfg.blocks) do
    result[b.name]={live_in=sorted_keys(input[i]),live_out=sorted_keys(output[i]),
      use=sorted_keys(ud[i].use),def=sorted_keys(ud[i].def)}
  end
  return result
end
local function dominators(cfg,reverse)
  local ns=cfg.blocks;local dom={}
  local active=copy(cfg.reachable)
  local exits={}
  if reverse then
    -- Initialize only exit-reaching nodes to the universal set. Otherwise an
    -- infinite SCC incorrectly acquires every node as a postdominator.
    active={}
    local stack={}
    for i,b in ipairs(ns) do if cfg.reachable[i] and #b.succ==0 then
      exits[i]=true;active[i]=true;stack[#stack+1]=i
    end end
    while #stack>0 do
      local i=table.remove(stack)
      for _,p in ipairs(ns[i].pred) do if cfg.reachable[p] and not active[p] then
        active[p]=true;stack[#stack+1]=p
      end end
    end
  else exits[1]=true end
  for i in ipairs(ns) do dom[i]=(exits[i] or not active[i]) and {[i]=true} or copy(active) end
  local changed=true;local loops=0
  while changed do
    changed=false;loops=loops+1;assert(loops<10000,'dominators did not converge')
    for i,b in ipairs(ns) do
      if active[i] and not exits[i] then
        local prev={}
        for _,p in ipairs(reverse and b.succ or b.pred) do
          if cfg.reachable[p] then prev[#prev+1]=p end
        end
        local nextdom
        if #prev==0 then nextdom={}
        else
          nextdom=copy(dom[prev[1]])
          for j=2,#prev do nextdom=intersect(nextdom,dom[prev[j]]) end
        end
        nextdom[i]=true
        if not equal(nextdom,dom[i]) then dom[i]=nextdom;changed=true end
      end
    end
  end
  local result={}
  for i,b in ipairs(ns) do
    local names={}
    for j in pairs(dom[i]) do names[#names+1]=ns[j].name end
    table.sort(names);result[b.name]=names
  end
  return result,dom
end
local function reaching_definitions(fn,cfg)
  local defs_by_var={};local gen,kill={},{ }
  for i,b in ipairs(cfg.blocks) do
    local g={}
    for j=b.first,b.last do
      local s=fn.statements[j]
      if s.kind=='let' then
        local key=s.name..'@'..j
        defs_by_var[s.name]=defs_by_var[s.name] or {}
        defs_by_var[s.name][key]=true
        for k in pairs(g) do if k:match('^'..s.name:gsub('(%W)','%%%1')..'@') then g[k]=nil end end
        g[key]=true
      end
    end
    gen[i]=g
  end
  for i,b in ipairs(cfg.blocks) do
    kill[i]={}
    local assigned={}
    for j=b.first,b.last do local s=fn.statements[j];if s.kind=='let' then assigned[s.name]=true end end
    for name in pairs(assigned) do union(kill[i],defs_by_var[name]) end
    kill[i]=difference(kill[i],gen[i])
  end
  local input,output={},{ }
  for i in ipairs(cfg.blocks) do input[i]={};output[i]=copy(gen[i]) end
  local changed=true;local loops=0
  while changed do
    changed=false;loops=loops+1;assert(loops<10000,'reaching defs did not converge')
    for i,b in ipairs(cfg.blocks) do
      local inext={};for _,p in ipairs(b.pred) do
        if not cfg.reachable[i] or cfg.reachable[p] then union(inext,output[p]) end
      end
      local onext=copy(gen[i]);union(onext,difference(inext,kill[i]))
      if not equal(input[i],inext) or not equal(output[i],onext) then changed=true end
      input[i],output[i]=inext,onext
    end
  end
  local result={};for i,b in ipairs(cfg.blocks) do
    result[b.name]={entry=sorted_keys(input[i]),exit=sorted_keys(output[i])}
  end
  return result
end
local function loops_report(cfg)
  local _,dom=dominators(cfg,false)
  local loops={}
  for _,b in ipairs(cfg.blocks) do
    for _,to in ipairs(b.succ) do
      if cfg.reachable[b.id] and dom[b.id][to] then
        local members={[to]=true,[b.id]=true};local stack={}
        if b.id~=to then stack[1]=b.id end
        while #stack>0 do
          local id=table.remove(stack)
          for _,p in ipairs(cfg.blocks[id].pred) do
            if cfg.reachable[p] and not members[p] then members[p]=true;stack[#stack+1]=p end
          end
        end
        local names={};for id in pairs(members) do names[#names+1]=cfg.blocks[id].name end
        table.sort(names)
        loops[#loops+1]={header=cfg.blocks[to].name,latch=b.name,blocks=names}
      end
    end
  end
  return loops
end
local function program_walk(program,callback)
  for _,f in ipairs(program.functions) do
    for _,s in ipairs(f.statements) do if s.expr then ir.walk(s.expr,function(n)callback(n,f,s)end) end end
  end
end
local function statements(program)
  local a={}
  for _,f in ipairs(program.functions) do for _,s in ipairs(f.statements) do a[#a+1]=s end end
  return a
end
local function total(program,func)
  local out={}
  for _,f in ipairs(program.functions) do out[f.name]=func(f,A.cfg(f)) end
  return out
end
function A.run(program,cfg)
  A.validate(cfg)
  local kind=cfg.kind;local result
  if kind=='numeric-statistic' then
    result={}
    local spec=statistics[cfg.statistic]
    for _,f in ipairs(program.functions) do
      local values={}
      for _,s in ipairs(f.statements) do if s.expr then
        if cfg.source=='ExpressionSizes' then values[#values+1]=ir.node_count(s.expr)
        else ir.walk(s.expr,function(n) if n.tag=='int' then values[#values+1]=n.value end end) end
      end end
      local entry={count=#values,source=cfg.source,statistic=cfg.statistic,precision='float64'}
      if #values<spec.minimum then entry.status='insufficient-data'
      else
        local option=spec.p
        if option==nil then option=spec.sample end
        local value=numeric[spec.op](values,option)
        if value~=value then entry.status='nan'
        elseif value==math.huge then entry.status='positive-infinity'
        elseif value==-math.huge then entry.status='negative-infinity'
        else entry.status='ok';entry.value=value end
      end
      result[f.name]=entry
    end
  elseif kind=='operator-count' then
    local op=assert(cfg.op,'operator-count requires op');local count=0
    program_walk(program,function(n) if n.tag=='op' and n.op==op then count=count+1 end end)
    result={op=op,count=count}
  elseif kind=='statement-count' then
    local ty=cfg.statement;local count=0
    for _,s in ipairs(statements(program)) do if s.kind==ty then count=count+1 end end
    result={statement=ty,count=count}
  elseif kind=='instruction-count' then
    result={count=#statements(program)}
  elseif kind=='function-count' then result={count=#program.functions}
  elseif kind=='node-count' then
    local n=0;program_walk(program,function()n=n+1 end);result={count=n}
  elseif kind=='maximum-expression-depth' then
    local d=0;for _,s in ipairs(statements(program)) do if s.expr then d=math.max(d,ir.depth(s.expr)) end end
    result={depth=d}
  elseif kind=='literal-histogram' then
    local h={};program_walk(program,function(n) if n.tag=='int' then local k=tostring(n.value);h[k]=(h[k] or 0)+1 end end)
    result=h
  elseif kind=='variable-uses' then
    local h={};program_walk(program,function(n) if n.tag=='var' then h[n.name]=(h[n.name] or 0)+1 end end)
    result=h
  elseif kind=='definitions' then
    local h={};for _,s in ipairs(statements(program)) do if s.kind=='let' then h[s.name]=(h[s.name] or 0)+1 end end;result=h
  elseif kind=='unused-definitions' then
    result=total(program,function(f)
      local uses={};for _,s in ipairs(f.statements) do if s.expr then ir.vars(s.expr,uses) end end
      local unused={};for _,s in ipairs(f.statements) do if s.kind=='let' and not uses[s.name] then unused[#unused+1]=s.name end end
      return unused
    end)
  elseif kind=='call-graph' then
    result={};for _,f in ipairs(program.functions) do
      local called={};for _,s in ipairs(f.statements) do
        if s.expr then ir.walk(s.expr,function(n) if n.tag=='call' then called[n.name]=true end end) end
      end
      result[f.name]=sorted_keys(called)
    end
  elseif kind=='impure-call-sites' then
    result=total(program,function(f)
      local sites={};for i,s in ipairs(f.statements) do if s.expr then
        ir.walk(s.expr,function(n) if n.tag=='call' then sites[#sites+1]={line=i,callee=n.name} end end)
      end end;return sites
    end)
  elseif kind=='cfg' then result=total(program,function(_,graph)return cfg_to_json(graph)end)
  elseif kind=='reachable-blocks' then result=total(program,function(_,graph)
      local found={};for i in pairs(graph.reachable) do found[#found+1]=graph.blocks[i].name end
      table.sort(found);return found
    end)
  elseif kind=='unreachable-blocks' then result=total(program,function(_,graph)
      local found={};for i,b in ipairs(graph.blocks) do if not graph.reachable[i] then found[#found+1]=b.name end end
      return found
    end)
  elseif kind=='dominators' then result=total(program,function(_,graph)return dominators(graph,false)end)
  elseif kind=='postdominators' then result=total(program,function(_,graph)return dominators(graph,true)end)
  elseif kind=='liveness' then result=total(program,function(f,graph)return dataflow_liveness(f,graph)end)
  elseif kind=='reaching-definitions' then result=total(program,function(f,graph)return reaching_definitions(f,graph)end)
  elseif kind=='natural-loops' then result=total(program,function(_,graph)return loops_report(graph)end)
  elseif kind=='critical-edges' then result=total(program,function(_,graph)
      local edges={};for _,b in ipairs(graph.blocks) do if #b.succ>1 then
        for _,j in ipairs(b.succ) do if #graph.blocks[j].pred>1 then
          edges[#edges+1]={from=b.name,to=graph.blocks[j].name} end end
      end end;return edges
    end)
  elseif kind=='cyclomatic-complexity' then result=total(program,function(_,graph)
      local e=0;for _,b in ipairs(graph.blocks) do e=e+#b.succ end
      return {nodes=#graph.blocks,edges=e,complexity=e-#graph.blocks+2}
    end)
  elseif kind=='loop-headers' then result=total(program,function(_,graph)
      local set={};for _,lp in ipairs(loops_report(graph)) do set[lp.header]=true end
      return sorted_keys(set)
    end)
  elseif kind=='block-def-use' then result=total(program,function(f,graph)
      local out={};for i,b in ipairs(graph.blocks) do
        local ud=uses_def(f,graph)[i]
        out[b.name]={use=sorted_keys(ud.use),def=sorted_keys(ud.def)}
      end;return out
    end)
  elseif kind=='expression-redundancy' then result=total(program,function(f)
      local seen={};local repeated={}
      for i,s in ipairs(f.statements) do if s.expr and ir.pure(s.expr) then
        local key=ir.serialize(s.expr)
        if seen[key] then repeated[#repeated+1]={first=seen[key],again=i,expression=key}
        else seen[key]=i end
      end end;return repeated
    end)
  elseif kind=='constant-branches' then result=total(program,function(f)
      local sites={};for i,s in ipairs(f.statements) do
        if s.kind=='br' and s.expr.tag=='int' then sites[#sites+1]={line=i,value=s.expr.value} end
      end;return sites
    end)
  elseif kind=='literal-range' then
    local lo,hi=nil,nil
    program_walk(program,function(n) if n.tag=='int' then
      lo=lo and math.min(lo,n.value) or n.value
      hi=hi and math.max(hi,n.value) or n.value
    end end)
    result={min=lo,max=hi,has_literals=lo~=nil}
  elseif kind=='parameter-uses' then result=total(program,function(f)
      local counts={};for _,p in ipairs(f.params) do counts[p]=0 end
      for _,s in ipairs(f.statements) do if s.expr then
        ir.walk(s.expr,function(n)if n.tag=='var' and counts[n.name] then counts[n.name]=counts[n.name]+1 end end)
      end end
      return counts
    end)
  elseif kind=='branch-targets' then result=total(program,function(f)
      local counts={};for _,s in ipairs(f.statements) do
        if s.kind=='goto' then counts[s.target]=(counts[s.target] or 0)+1 end
        if s.kind=='br' then counts[s.yes]=(counts[s.yes] or 0)+1;counts[s.no]=(counts[s.no] or 0)+1 end
      end;return counts
    end)
  elseif kind=='terminal-blocks' then result=total(program,function(_,graph)
      local names={};for _,b in ipairs(graph.blocks) do if #b.succ==0 then names[#names+1]=b.name end end
      return names
    end)
  elseif kind=='opcode-histogram' then
    local h={};program_walk(program,function(n) if n.tag=='op' then h[n.op]=(h[n.op] or 0)+1 end end)
    result=h
  elseif kind=='statement-histogram' then
    local h={};for _,s in ipairs(statements(program)) do h[s.kind]=(h[s.kind] or 0)+1 end
    result=h
  elseif kind=='expression-purity' then
    local yes,no=0,0
    for _,s in ipairs(statements(program)) do if s.expr then if ir.pure(s.expr) then yes=yes+1 else no=no+1 end end end
    result={pure=yes,impure=no}
  else error('unimplemented analysis: '..kind) end
  return result
end
return A
