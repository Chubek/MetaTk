local ir=require('metail.ir')
local P={}
local binop={
  ['||']={'lor',1},['&&']={'land',2},['|']={'bor',3},['^']={'bxor',4},['&']={'band',5},
  ['==']={'eq',6},['!=']={'ne',6},['<']={'lt',7},['<=']={'le',7},['>']={'gt',7},['>=']={'ge',7},
  ['<<']={'shl',8},['>>']={'shr',8},['+']={'add',9},['-']={'sub',9},
  ['*']={'mul',10},['/']={'div',10},['%']={'mod',10}
}
local prefix={['-']='neg',['~']='bnot',['!']='lnot'}
local function tokenize(text,file)
  assert(#text<=8*1024*1024,file..': source exceeds 8 MiB limit')
  local tokens={}
  local i,line=1,1
  local function add(kind,value)
    assert(#tokens<200000,file..': token limit exceeded')
    tokens[#tokens+1]={kind=kind,value=value,line=line}
  end
  while i<=#text do
    local c=text:sub(i,i)
    if c=='\n' then line=line+1;i=i+1
    elseif c:match('%s') then i=i+1
    elseif text:sub(i,i+1)=='//' or text:sub(i,i)=='#' or text:sub(i,i+1)=='--' then
      local j=text:find('\n',i,true); i=j or (#text+1)
    elseif c:match('[%a_$]') then
      local j=i+1
      while j<=#text and text:sub(j,j):match('[%w_$]') do j=j+1 end
      add('word',text:sub(i,j-1));i=j
    elseif c:match('%d') then
      local j=i
      if text:sub(i,i+1)=='0x' or text:sub(i,i+1)=='0X' then
        j=i+2;while text:sub(j,j):match('[%da-fA-F]') and j<=#text do j=j+1 end
      else
        while text:sub(j,j):match('%d') and j<=#text do j=j+1 end
      end
      local str=text:sub(i,j-1)
      if str:match('^0[xX]') and #str:sub(3):gsub('^0+','')>16 then
        error(file..':'..line..': hexadecimal integer exceeds 64 bits')
      end
      local val=tonumber(str)
      if str=='9223372036854775808' then
        add('min-magnitude',str)
      else
        if not val or math.type(val)~='integer' then error(file..':'..line..': integer out of signed 64-bit range: '..str) end
        add('int',val)
      end
      i=j
    else
      local op=text:sub(i,i+1)
      if binop[op] then add('symbol',op);i=i+2
      elseif c:match('[%(%){%}%[%],:;=+*/%%&|%^~!<>%-]') then add('symbol',c);i=i+1
      else error(file..':'..line..': unexpected character '..string.format('%q',c)) end
    end
  end
  add('eof','<eof>')
  return tokens
end
function P.reader(text,file)
  local tokens=tokenize(text,file or '<source>')
  local pos=1
  local nesting=0
  local depths=setmetatable({},{__mode='k'})
  local R={}
  function R.peek(n) return tokens[pos+(n or 0)] end
  function R.take() local t=tokens[pos];pos=pos+1;return t end
  function R.accept(s) if R.peek().value==s then R.take();return true end return false end
  function R.fail(s)
    error(string.format('%s:%d: expected %s, found %s',file or '<source>',R.peek().line,s,tostring(R.peek().value)),0)
  end
  function R.expect(s) if not R.accept(s) then R.fail(s) end end
  function R.word()
    local t=R.peek();if t.kind~='word' or not t.value:match('^[%a_][%w_]*$') then R.fail('identifier') end
    R.take();return t.value
  end
  function R.expr(minprec)
    nesting=nesting+1
    assert(nesting<=256,(file or '<source>')..': expression nesting limit exceeded')
    minprec=minprec or 1
    local t=R.take();local n
    if t.kind=='int' then n={tag='int',value=t.value}
    elseif t.kind=='word' then
      if t.value=='true' then n={tag='int',value=1}
      elseif t.value=='false' then n={tag='int',value=0}
      elseif R.accept('(') then
        local args={}
        if not R.accept(')') then
          repeat args[#args+1]=R.expr() until not R.accept(',')
          R.expect(')')
        end
        if ir.operators[t.value] then
          if #args~=ir.operators[t.value] then error((file or '<source>')..':'..t.line..': invalid arity for '..t.value,0) end
          n={tag='op',op=t.value,args=args}
        else n={tag='call',name=t.value,args=args} end
      else
        n={tag=t.value:sub(1,1)=='$' and 'hole' or 'var',name=t.value}
      end
    elseif t.value=='(' then n=R.expr();R.expect(')')
    elseif t.value=='-' and R.peek().kind=='min-magnitude' then
      R.take();n={tag='int',value=math.mininteger}
    elseif t.value=='-' and R.peek().kind=='int' then
      n={tag='int',value=-R.take().value}
    elseif prefix[t.value] then n={tag='op',op=prefix[t.value],args={R.expr(11)}}
    else error((file or '<source>')..':'..t.line..': unexpected expression token '..tostring(t.value),0) end
    while true do
      local b=binop[R.peek().value]
      if not b or b[2]<minprec then break end
      R.take()
      local right=R.expr(b[2]+1)
      local depth=math.max(depths[n] or 1,depths[right] or 1)+1
      assert(depth<=256,(file or '<source>')..': expression depth limit exceeded')
      n={tag='op',op=b[1],args={n,right}}
      depths[n]=depth
    end
    local depth=1
    for _,child in ipairs(n.args or {}) do depth=math.max(depth,(depths[child] or 1)+1) end
    assert(depth<=256,(file or '<source>')..': expression depth limit exceeded')
    depths[n]=depth
    nesting=nesting-1
    return n
  end
  function R.done() if R.peek().kind~='eof' then R.fail('end of file') end end
  return R
end
function P.expression(src,filename)
  local r=P.reader(src,filename or '<pattern>')
  local n=r.expr();r.done();return n
end
function P.program(src,filename)
  local r=P.reader(src,filename or '<MetaIL>')
  local module,functions=nil,{}
  if r.accept('module') then module=r.word();r.expect(';') end
  while r.peek().kind~='eof' do
    r.expect('fn');local name=r.word();r.expect('(')
    local params={}
    if not r.accept(')') then
      repeat params[#params+1]=r.word() until not r.accept(',')
      r.expect(')')
    end
    r.expect('{')
    local stmts={}
    while not r.accept('}') do
      local tok=r.peek()
      if tok.kind=='eof' then r.fail('}') end
      local word=r.take().value
      local s
      if word=='let' then
        local dst=r.word();r.expect('=');s={kind='let',name=dst,expr=r.expr()};r.expect(';')
      elseif word=='return' or word=='eval' or word=='assume' then
        s={kind=word,expr=r.expr()};r.expect(';')
      elseif word=='goto' then
        s={kind='goto',target=r.word()};r.expect(';')
      elseif word=='br' then
        s={kind='br',expr=r.expr()};r.expect(',');s.yes=r.word();r.expect(',');s.no=r.word();r.expect(';')
      elseif tok.kind=='word' and word:match('^[%a_][%w_]*$') and r.accept(':') then s={kind='label',name=word}
      else error((filename or '<MetaIL>')..':'..tok.line..': unknown statement '..word,0) end
      if s.expr then ir.walk(s.expr,function(n)
        assert(n.tag~='hole',(filename or '<MetaIL>')..': pattern captures are not program variables')
        if n.tag=='var' or n.tag=='call' then
          assert(n.name:match('^[%a_][%w_]*$'),(filename or '<MetaIL>')..': invalid identifier '..n.name)
        end
      end) end
      stmts[#stmts+1]=s
    end
    functions[#functions+1]={name=name,params=params,statements=stmts}
  end
  if #functions==0 then r.fail('at least one fn') end
  r.done()
  return {module=module,functions=functions}
end
return P
