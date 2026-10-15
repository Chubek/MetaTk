if exists("b:did_ftplugin") | finish | endif
let b:did_ftplugin = 1

" Line comments start with `--`; block comments nest as `{- ... -}`.
setlocal commentstring=--\ %s
setlocal comments=s:{-,m:-,e:-},:--
setlocal commentstart=
" File extensions handled by ftdetect: .mv, .mvec, .mvct.
setlocal suffixesadd=.mvct,.mv,.mvec
" Identifiers are [A-Za-z_][A-Za-z0-9_]*; keep word motions on identifier chars.
setlocal iskeyword=@,_,a-z,A-Z,48-57
setlocal matchpairs=(:),[:],{:}
" `mvect --check-syntax` reports `file:line:col: error: message`.
if executable('mvect')
  setlocal makeprg=mvect\ --check-syntax\ %
  setlocal errorformat=%f:%l:%c:\ error:\ %m
endif
" Spaces match the repository examples (4-space continuation indents).
setlocal shiftwidth=4
setlocal softtabstop=4
setlocal expandtab
setlocal autoindent
setlocal smartindent

let b:undo_ftplugin = "setlocal commentstring< comments< commentstart< suffixesadd< iskeyword< matchpairs< makeprg< errorformat< shiftwidth< softtabstop< expandtab< autoindent< smartindent<"
