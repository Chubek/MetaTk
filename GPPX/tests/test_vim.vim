" Run with vim -Nu NONE -n -es -S this-file, GPPX_VIM_RUNTIME set by runner.
set nocompatible
execute 'set runtimepath^=' . fnameescape($GPPX_VIM_RUNTIME)
filetype plugin indent on
syntax enable
new sample.gppx
call assert_equal('gppx', &filetype)
setlocal shiftwidth=2 expandtab
call setline(1, ['#if 1', '#repeat 2', 'value', '#endrepeat', '#else',
      \ '#raw', '  literal', '#if raw-text', '#endraw', '#endif',
      \ '/*', '#if comment', '*/', 'tail'])
normal! gg=G
call assert_equal([0, 2, 4, 2, 0, 2, 2, 0, 2, 0, 0, 0, 0, 0],
      \ map(range(1, 14), 'indent(v:val)'))
call assert_equal('gppxRaw', synIDattr(synID(8, 2, 1), 'name'))
call assert_equal('gppxBlockComment', synIDattr(synID(12, 2, 1), 'name'))
call setline(1, ['#define X 1', '$(X) {{X}} "text" // comment'])
syn sync fromstart
call assert_equal('gppxDirectiveName', synIDattr(synID(1, 2, 1), 'name'))
call assert_equal('gppxMacro', synIDattr(synID(2, 1, 1), 'name'))
call assert_equal('gppxTemplateMacro', synIDattr(synID(2, 6, 1), 'name'))
call assert_equal('gppxString', synIDattr(synID(2, 12, 1), 'name'))
call assert_equal('gppxLineComment', synIDattr(synID(2, 19, 1), 'name'))
if !empty(v:errors)
  for error in v:errors
    echom error
  endfor
  cquit
endif
qa!
