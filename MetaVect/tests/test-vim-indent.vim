set nocompatible
" Indent test: source ftplugin/indent directly (no user runtime dependency).
execute 'source ' . fnameescape($METAVECT_SOURCE_DIR . '/extern/Vim/indent/MetaVect.vim')
setlocal shiftwidth=4

function! s:indented(lines, target) abort
  %delete _
  call setline(1, a:lines)
  " NOTE: `:{range}=` in Ex mode only prints line numbers; `==` reindents.
  call cursor(a:target, 1)
  normal! ==
  return indent(a:target)
endfunction

" Net open delimiter on the previous line indents one level.
call assert_equal(4, s:indented(['x = f(', '1'], 2))
" Lambda `=>` continuation indents.
call assert_equal(4, s:indented(['f = fn(a: Int) =>', 'a'], 2))
" `let ... in` continuation indents.
call assert_equal(4, s:indented(['export size : Int = let base : Int = 20 in', 'if base > 0 then identity(base) else 0'], 2))
" `then` continuation indents.
call assert_equal(4, s:indented(['x = if a then', 'b else c'], 2))
" Closing delimiter dedents.
call assert_equal(0, s:indented(['x = f(', '1', ')'], 3))
" Brackets inside strings do not indent.
call assert_equal(0, s:indented(['x = "("', 'y = 1'], 2))
" Brackets inside comments do not indent.
call assert_equal(0, s:indented(['-- (', 'y = 1'], 2))
call assert_equal(0, s:indented(['{- ( -}', 'y = 1'], 2))
" An unclosed `{-` still indents its continuation.
call assert_equal(4, s:indented(['{- comment', 'still comment -}'], 2))
if !empty(v:errors)
  for error in v:errors
    echom error
  endfor
  cquit
endif
quit!
