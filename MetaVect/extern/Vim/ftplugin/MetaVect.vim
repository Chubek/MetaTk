if exists("b:did_ftplugin") | finish | endif
let b:did_ftplugin = 1
setlocal commentstring=--\ %s
setlocal suffixesadd=.mv,.mvec,.mvct
let b:undo_ftplugin = "setlocal commentstring< suffixesadd<"
