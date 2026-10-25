if exists("b:current_syntax") | finish | endif
syn case match

" MetaVect lexer mirror. The compiler lexer (frontend/Lexer.d) is authoritative;
" this file is a best-effort editor highlight. Groups below follow TokenKind:
" keywords, booleans, operators, delimiters, numbers, strings, comments.
" Type names are conventional only: the lexer treats them as identifiers.

" Reserved words (TokenKind kwLet..kwExport). `true`/`false` are below as booleans.
syn keyword mvKeyword let in fun fn type forall if then else match with module import export
" Boolean literals (kwTrue/kwFalse, LiteralKind boolean).
syn keyword mvBoolean true false
" Conventional type names (lexer identifiers, highlighted for readability).
syn keyword mvType Int Float Bool String Unit Path Color Vector Matrix

" Numbers: integers and floats share one group, mirroring the lexer which
" decides kind while scanning: digits, optional fraction (digits on both sides
" of the dot), optional exponent with mandatory digits. `1.` lexes as integer
" `1` plus `.`; `.5` lexes as `.` plus integer `5`.
syn match mvNumber "\<\d\+\(\.\d\+\)\?\([eE][+-]\?\d\+\)\?\>"

" Best-effort errors for inputs the lexer rejects as invalidNumber.
syn match mvNumberError "\<\d\+[A-Za-z_]\S*\>"
syn match mvNumberError "\<\\d\\+\\(\\.\\d\\+\\)\\?[eE][+-]\\?\\>"

" Operators, longest spellings first. Single-character `:` `.` `-` are in the
" trailing class; their multi-character forms (`::` `->` `=>` `--` `{-`) are
" matched by earlier alternatives or by the comment rules below.
syn match mvOperator "->\|=>\|::\|==\|!=\|<=\|>=\|&&\|||\|[+*/=<>!|&:.-]"
" Delimiters. `()` is intentionally two delimiters here: the lexer emits two
" paren tokens and only the parser reads them as a unit literal.
syn match mvDelimiter "[()\[\]{},;]"

" Items matching at the same start position are resolved last-defined-wins, so
" comment and string starts are declared last: their leading '-' and '"' must
" not be highlighted as operators when they begin a comment or a string.
" `--` starts a line comment; `x--1` is `x` followed by a comment.
syn match mvLineComment "--.*$"
" `{- ... -}` nests (contains itself). `--` inside a block comment stays part
" of the block comment because only contained items apply inside a region.
" NOTE: no `keepend` here: with self-nesting, an inner `-}` would otherwise
" also terminate the outer region and break nesting.
syn region mvBlockComment start="{-" end="-}" contains=mvBlockComment
" Double-quoted strings. The region spans lines (the lexer rejects raw
" newlines, but highlighting the run is the useful best effort). Only
" \n \r \t \" \\ are valid escapes; any other backslash sequence is an error.
syn region mvString start=+"+ skip=+\\[nrt"\\]+ end=+"+ contains=mvStringEscape,mvStringError
syn match mvStringEscape +\\[nrt"\\]+ contained
syn match mvStringError +\\[^nrt"\\]+ contained

syn sync fromstart

hi def link mvKeyword Keyword
hi def link mvType Type
hi def link mvBoolean Boolean
hi def link mvNumber Number
hi def link mvNumberError Error
hi def link mvString String
hi def link mvStringEscape SpecialChar
hi def link mvStringError Error
hi def link mvLineComment Comment
hi def link mvBlockComment Comment
hi def link mvOperator Operator
hi def link mvDelimiter Delimiter
let b:current_syntax = "metavect"
