if exists('b:did_ftplugin')
  finish
endif
let b:did_ftplugin = 1

setlocal comments=s1:{-,mb:\ ,ex:-},:--
setlocal commentstring=--\ %s
setlocal suffixesadd=.mml,.mmli,.mlli
setlocal iskeyword+=39
setlocal expandtab shiftwidth=2 softtabstop=2
setlocal formatoptions-=t
setlocal formatoptions+=croql

command! -buffer MetaMLCheck call metaml#Check()
command! -buffer -nargs=* MetaMLRun call metaml#Run([<f-args>])
command! -buffer MetaMLAST call metaml#AST()
command! -buffer -nargs=1 -complete=customlist,metaml#ProjectCommands MetaMLMammal call metaml#Project(<q-args>)

let b:undo_ftplugin = 'setlocal comments< commentstring< suffixesadd< iskeyword< expandtab< shiftwidth< softtabstop< formatoptions< '
      \ . '| delcommand MetaMLCheck | delcommand MetaMLRun | delcommand MetaMLAST | delcommand MetaMLMammal'
