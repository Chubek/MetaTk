if exists("b:did_indent") | finish | endif
let b:did_indent = 1

setlocal autoindent
setlocal indentexpr=GetMetaVectIndent()
setlocal indentkeys=0{,0},0),0],!^F,o,O,e
" Continuation keywords dedent/indent handled inside the function, not by keys.
setlocal shiftwidth=4

" Indent rules (no source execution, purely lexical):
" - Nest one level per net open delimiter ([{()}]}) on the previous code line.
" - One extra level when the previous code line ends with `=`, `=>`, `then`,
"   `else`, or `in` (expression continuations).
" - Dedent one level when the current line starts with a closing delimiter.
" Strings and comments are stripped before counting so their brackets do not
" affect indentation. Block comments nest (`{- ... -}`).
function! GetMetaVectIndent() abort
  let l:lnum = v:lnum
  if l:lnum <= 1
    return 0
  endif
  let l:prev = l:lnum - 1
  while l:prev > 0 && getline(l:prev) =~# '^\s*$'
    let l:prev -= 1
  endwhile
  if l:prev <= 0
    return 0
  endif
  let l:prevline = s:CodeLine(getline(l:prev))
  let l:curline = substitute(getline(l:lnum), '^\s*', '', '')
  let l:base = indent(l:prev)
  let l:sw = shiftwidth()
  let l:level = l:base / l:sw
  let l:net = s:NetOpens(l:prevline)
  let l:level += l:net
  if l:prevline =~# '\(=\|=>\|\<then\>\|\<else\>\|\<in\>\)\s*$'
    let l:level += 1
  endif
  if l:curline =~# '^[\]})]'
    let l:level -= 1
  endif
  if l:level < 0
    let l:level = 0
  endif
  return l:level * l:sw
endfunction

" Strip one line to code only: remove strings, then line comment, then nested
" block comments. Operates on a single line; multi-line block comments leave
" the line fully commented (treated as blank for indent purposes is handled by
" the caller skipping blank lines, but an unbalanced `{-` on this line still
" counts its opens below via NetOpens on the remainder).
function! s:CodeLine(line) abort
  let l:s = a:line
  " Remove double-quoted strings with valid escapes (best effort; an
  " unterminated string removes to end of line, matching the lexer error).
  let l:s = substitute(l:s, '"\(\\[nrt"\\]\|[^"\\]\)*\("\|$\)', '""', 'g')
  " Line comment starts with `--` and runs to end of line.
  let l:s = substitute(l:s, '--.*$', '', '')
  " Iteratively remove innermost `{- ... -}` pairs on this line (nesting).
  let l:prev = ''
  while l:s !=# l:prev
    let l:prev = l:s
    let l:s = substitute(l:s, '{-[^-}]*-}', '', 'g')
  endwhile
  return l:s
endfunction

function! s:NetOpens(line) abort
  " Single pass so a `{-` opener is counted exactly once: balanced `{- -}`
  " pairs net to zero, a dangling `{-` indents its continuation lines, and a
  " stray `-}` outside any opener counts its `}` as a dedent (mirroring how
  " the lexer sees `-` then `}` there).
  let l:opens = 0
  let l:depth = 0
  let l:i = 0
  let l:n = strlen(a:line)
  while l:i < l:n
    let l:c = a:line[l:i]
    let l:next = l:i + 1 < l:n ? a:line[l:i+1] : ''
    if l:c ==# '{' && l:next ==# '-'
      let l:opens += 1
      let l:depth += 1
      let l:i += 2
    elseif l:c ==# '-' && l:next ==# '}'
      let l:opens -= 1
      if l:depth > 0
        let l:depth -= 1
      endif
      let l:i += 2
    elseif l:c ==# '(' || l:c ==# '[' || l:c ==# '{'
      let l:opens += 1
      let l:i += 1
    elseif l:c ==# ')' || l:c ==# ']' || l:c ==# '}'
      let l:opens -= 1
      let l:i += 1
    else
      let l:i += 1
    endif
  endwhile
  return l:opens
endfunction

let b:undo_indent = "setlocal autoindent< indentexpr< indentkeys< shiftwidth<"
