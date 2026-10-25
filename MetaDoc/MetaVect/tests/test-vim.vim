set nocompatible
" Load only our rules; do not depend on the user's Vim runtime or filetype setup.
syntax clear
execute 'source ' . fnameescape($METAVECT_SOURCE_DIR . '/extern/Vim/syntax/MetaVect.vim')
call setline(1, [
      \ '-- let 1',
      \ '{- outer {- nested -} let -}',
      \ 'let x = f(1.5E-2, -2) && true || false',
      \ '"left\"right" + 1',
      \ '[]{}();,',
      \ '"left\\" + 1',
      \ 'export answer : Int -> Float = f',
      \ 'x = 12abc + 1e',
      \ '"a\nb" + "bad\q"',
      \ '"oops'])
syntax sync fromstart

function! s:group(line, text)
  let column = stridx(getline(a:line), a:text) + 1
  call assert_true(column > 0, 'missing test text: ' . a:text)
  return synIDattr(synID(a:line, column, 1), 'name')
endfunction

call assert_equal('mvLineComment', s:group(1, '--'))
call assert_equal('mvLineComment', s:group(1, 'let'))
call assert_equal('mvBlockComment', s:group(2, 'nested'))
call assert_equal('mvBlockComment', s:group(2, 'let'))
call assert_equal('mvKeyword', s:group(3, 'let'))
call assert_equal('mvNumber', s:group(3, '1.5E-2'))
call assert_equal('mvOperator', s:group(3, '&&'))
call assert_equal('mvOperator', s:group(3, '||'))
call assert_equal('mvBoolean', s:group(3, 'true'))
call assert_equal('mvString', s:group(4, 'right'))
call assert_equal('mvOperator', s:group(4, '+'))
call assert_equal('mvNumber', s:group(4, '1'))
for column in range(1, strlen(getline(5)))
  call assert_equal('mvDelimiter', synIDattr(synID(5, column, 1), 'name'))
endfor
call assert_equal('mvOperator', s:group(6, '+'))
call assert_equal('mvNumber', s:group(6, '1'))
" Lexer-aligned groups: keywords, types, multi-char operators, errors, escapes.
call assert_equal('mvKeyword', s:group(7, 'export'))
call assert_equal('mvType', s:group(7, 'Int'))
call assert_equal('mvType', s:group(7, 'Float'))
call assert_equal('mvOperator', s:group(7, '->'))
call assert_equal('mvOperator', s:group(7, ':'))
call assert_equal('mvNumberError', s:group(8, '12abc'))
call assert_equal('mvNumberError', s:group(8, '1e'))
call assert_equal('mvStringEscape', s:group(9, '\n'))
call assert_equal('mvStringError', s:group(9, '\q'))
call assert_equal('mvString', s:group(10, 'oops'))
if !empty(v:errors)
  for error in v:errors
    echom error
  endfor
  cquit
endif
quit!
