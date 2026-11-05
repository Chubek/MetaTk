" MetaML syntax, following the lexer in MetaML.hpp / MetaML.d.
if exists('b:current_syntax')
  finish
endif
syntax case match

syntax keyword metamlKeyword module type sig struct end val let rec in import include open do and of
syntax keyword metamlConditional if then else
syntax keyword metamlRepeat for
syntax keyword metamlMatch match with when
syntax keyword metamlFunction fn fun
syntax keyword metamlBoolean true false
syntax keyword metamlType Int Float Bool String Unit Option Result Num
syntax match metamlConstructor "\<[A-Z][A-Za-z0-9_']*\>"
syntax match metamlTypeVariable "\%([A-Za-z0-9_']\)\@<!'[a-z_][A-Za-z0-9_']*"
syntax match metamlDefinition "\<[a-z_][A-Za-z0-9_']*\ze\s*\%(::\|:=\)"
syntax match metamlOperator "[-!%&*+./:<=>?@\\^|~]\+"
syntax match metamlOperator "∘"
syntax match metamlDelimiter "[()\[\]{},;]"
syntax match metamlNumber "\<\d\%(_\?\d\)*\>"
syntax match metamlNumber "\<0[xX][0-9A-Fa-f]\%(_\?[0-9A-Fa-f]\)*\>"
syntax match metamlNumber "\<0[bB][01]\%(_\?[01]\)*\>"
syntax match metamlFloat "\<\d\%(_\?\d\)*\.\d\%(_\?\d\)*\%([eE][+-]\?\d\%(_\?\d\)*\)\?\>"
syntax match metamlFloat "\<\d\%(_\?\d\)*[eE][+-]\?\d\%(_\?\d\)*\>"
syntax match metamlPlaceholder "[$#]\d\+"
syntax match metamlDirective "#[A-Za-z_][A-Za-z0-9_']*"
syntax match metamlDirective "^\s*%[A-Za-z_][A-Za-z0-9_']*"

syntax match metamlEscapeError "\\." contained
syntax match metamlEscape "\\\%([ntr0\"\\]\|u{[0-9A-Fa-f]\{1,6}}\)" contained
syntax region metamlString start=+"+ skip=+\\.+ end=+"+ contains=metamlEscapeError,metamlEscape
syntax region metamlRawString start=+\<r"+ end=+"+

syntax keyword metamlTodo TODO FIXME XXX NOTE contained
syntax match metamlComment "--.*$" contains=metamlTodo,@Spell
syntax match metamlComment "#\%([A-Za-z_0-9]\)\@!.*$" contains=metamlTodo,@Spell
syntax region metamlBlockComment start="{-" end="-}" contains=metamlBlockComment,metamlTodo,@Spell
syntax sync fromstart

highlight default link metamlKeyword Keyword
highlight default link metamlConditional Conditional
highlight default link metamlRepeat Repeat
highlight default link metamlMatch Conditional
highlight default link metamlFunction Keyword
highlight default link metamlBoolean Boolean
highlight default link metamlType Type
highlight default link metamlConstructor Type
highlight default link metamlTypeVariable Type
highlight default link metamlDefinition Function
highlight default link metamlOperator Operator
highlight default link metamlDelimiter Delimiter
highlight default link metamlNumber Number
highlight default link metamlFloat Float
highlight default link metamlPlaceholder Special
highlight default link metamlDirective PreProc
highlight default link metamlEscape SpecialChar
highlight default link metamlEscapeError Error
highlight default link metamlString String
highlight default link metamlRawString String
highlight default link metamlComment Comment
highlight default link metamlBlockComment Comment
highlight default link metamlTodo Todo
let b:current_syntax = 'metaml'
