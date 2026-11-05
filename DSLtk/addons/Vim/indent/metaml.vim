" Conservative indentation: explicit blocks/brackets and continued bindings.
if exists('b:did_indent')
  finish
endif
let b:did_indent = 1
setlocal indentexpr=MetaMLIndent(v:lnum)
setlocal indentkeys=o,O,0=end,0=in,0=else,0=then,0=with,0=\|,0),0],0}
let b:undo_indent = 'setlocal indentexpr< indentkeys<'

if exists('*MetaMLIndent')
  finish
endif

function! s:Code(lnum) abort
  let text = getline(a:lnum)
  let code = ''
  for col in range(1, strlen(text))
    let name = synIDattr(synID(a:lnum, col, 1), 'name')
    let code .= name =~# 'Comment\|String\|Escape\|Todo' ? ' ' : strpart(text, col - 1, 1)
  endfor
  return code
endfunction

function! s:Skip() abort
  return synIDattr(synID(line('.'), col('.'), 1), 'name') =~# 'Comment\|String\|Escape\|Todo'
endfunction

function! MetaMLIndent(lnum) abort
  let prev = prevnonblank(a:lnum - 1)
  if prev == 0 | return 0 | endif
  " Preserve multiline comments and strings rather than rewriting their contents.
  if synIDattr(synID(a:lnum, 1, 1), 'name') =~# 'Comment\|String'
    return -1
  endif
  let current = s:Code(a:lnum)
  let before = s:Code(prev)
  let amount = indent(prev)
  let sw = shiftwidth()
  let view = winsaveview()
  let block = [0, 0]
  try
    call cursor(a:lnum, 1)
    let block = searchpairpos('\<\%(struct\|sig\|do\)\>', '', '\<end\>', 'bnW', 's:Skip()')
    if current =~# '^\s*end\>'
      return block[0] ? indent(block[0]) : max([0, amount - sw])
    endif
    if current =~# '^\s*[])}]'
      let close = matchstr(current, '[])}]')
      let open = close ==# ')' ? '(' : close ==# ']' ? '\[' : '{'
      let pair = searchpairpos(open, '', '\V' . close, 'bnW', 's:Skip()')
      if pair[0] | return indent(pair[0]) | endif
    endif
  finally
    call winrestview(view)
  endtry
  if before =~# '\<\%(struct\|sig\|do\|then\|else\|in\|with\)\>\s*$\|\%(:=\|->\|[([{]\)\s*$'
    let amount += sw
  endif
  if current =~# '^\s*\%(in\|else\|then\|with\)\>'
    let amount -= sw
  endif
  if current =~# '^\s*|' && before =~# '^\s*|'
    return indent(prev)
  endif
  " An already column-one declaration is intentional: MetaML layout is semantic.
  if !block[0] && getline(a:lnum) =~# '^\%(\%(module\|type\|val\|import\|open\|include\)\>\|[a-z_][A-Za-z0-9_'']*\>.*\%(::\|:=\)\)'
    return 0
  endif
  return max([0, amount])
endfunction
