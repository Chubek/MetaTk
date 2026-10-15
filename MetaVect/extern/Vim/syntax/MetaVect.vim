if exists("b:current_syntax") | finish | endif
syn keyword mvKeyword let in fun fn type forall if then else match with module import export
syn keyword mvType Int Float Bool String Unit Path Color Vector Matrix
syn keyword mvBoolean true false
syn match mvNumber "\<\d\+\(\.\d\+\)\?\([eE][+-]\?\d\+\)\?\>"
syn region mvString start=+"+ skip=+\\."+ end=+"+
syn match mvLineComment "--.*$"
syn region mvBlockComment start="{-" end="-}"
syn match mvOperator "->\|=>\|::\|==\|!=\|<=\|>=\|[+*/=<>!|&:-]"
hi def link mvKeyword Keyword
hi def link mvType Type
hi def link mvBoolean Boolean
hi def link mvNumber Number
hi def link mvString String
hi def link mvLineComment Comment
hi def link mvBlockComment Comment
hi def link mvOperator Operator
let b:current_syntax = "metavect"
