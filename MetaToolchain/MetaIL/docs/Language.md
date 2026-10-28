# MetaIL language and IR contract

## Program model

A MetaIL module declares one or more functions. Functions have positional scalar arguments and a sequential list of statements divided into labeled basic blocks. A label must be unique within its function; `goto` and `br` targets must resolve. The first statement is the function entry. The front end parses to AST nodes: `int`, `var`, `op`, `call`, and patterns also use `hole`.

### EBNF (overview)

```ebnf
program     = [ "module" identifier ";" ], (function | external), { function | external } ;
external    = "extern", "fn", identifier, "(", [abi_type, {",", abi_type}, [",", "..."]], ")",
              "->", (abi_type | "void"), "from", string, ["symbol", string], ";" ;
abi_type    = "i8" | "u8" | "i16" | "u16" | "i32" | "u32" | "i64" | "u64" | "ptr" ;
function    = "fn", identifier, "(", [ identifier, { ",", identifier } ], ")", "{", { statement }, "}" ;
statement   = identifier, ":"
            | "let", identifier, "=", expression, ";"
            | "return", expression, ";"
            | "eval", expression, ";"
            | "assume", expression, ";"
            | "goto", identifier, ";"
            | "br", expression, ",", identifier, ",", identifier, ";" ;
expression  = 64-bit signed integer | identifier | "(", expression, ")"
            | unary_operator, expression | expression, binary_operator, expression
            | identifier, "(", [ expression, { ",", expression } ], ")" ;
```

### Operators (high to low precedence)

1. Prefix unary `-`, `~`, `!`.
2. `*`, `/`, `%`.
3. `+`, `-`.
4. `<<`, `>>`.
5. `<`, `<=`, `>`, `>=`.
6. `==`, `!=`.
7. Bitwise `&`, `^`, `|`.
8. Logical `&&`, `||`.

Operators named `add/sub/mul/div/mod`, `band/bor/bxor/shl/shr`, `neg/bnot/lnot`, `eq/ne/lt/le/gt/ge`, `land/lor`, and `select` can use prefix functional syntax.

## Integer and effects contract

- `int` uses a signed 64-bit integer and arithmetic wraps on overflow for add/sub/mul/neg.
- `div` and `mod` truncate toward zero; they are not rewritten across division by zero or minimum integer over `-1`.
- Right shift on signed negative values is **arithmetic**; left shift wraps. Shift counts must be 0..63 for defined folding.
- Comparisons yield 0 or 1; logical operators treat nonzero as true and yield 0/1.
- `select` evaluates only the selected arm. `land`/`lor` in this IR are strict (not short-circuit), whereas `select` is lazy.
- Unknown function calls may have side effects. A rewrite requiring `pure:x` does not match a call tree.
- Purity also excludes potentially trapping division/remainder and invalid or unknown shift counts. Dead assignments preserve these expressions as `eval`.
- Blocks are delimited by labels and terminators (`goto`, `br`, `return`). Fall-through is supported.
- `assume` is parsed and preserved but is not yet used to refine a path-sensitive fact lattice.

Identifiers use `[A-Za-z_][A-Za-z0-9_]*`; `$captures` are restricted to rewrite
patterns. Decimal literals must fit signed 64-bit, with a special case for
`-9223372036854775808`. Hexadecimal literals represent at most 64 bits (thus
`0xffffffffffffffff` is `-1`). Inputs are limited to 8 MiB, 200,000 tokens, and
expression nesting/tree depth 256. Malformed or over-limit programs are rejected.

MetaNum statistics use Lua binary64 numbers outside this scalar integer IR.
Their floating-point results are analysis reports, never integer rewrite facts.

## External C ABI declarations

At least one function definition is still required. External declarations record
the local callee, exported symbol (defaults to the local name), library and ABI
signature, and survive every optimizer pass. They do not imply purity, nor do they
load a library while compiling. Declared calls have checked fixed/minimum variadic
arity; void results may only appear directly in `eval`. Other calls retain legacy
unresolved-call semantics. External/local definition conflicts and duplicate
local/exported names are errors.

Quoted metadata accepts `\"`, `\\`, `\n`, `\r`, `\t`; controls are rejected in library
names, which are ASCII linker names or paths containing `/`. Paths beginning `-`
or `@` are rejected to prevent option/response-file interpretation. Symbols use
`[A-Za-z_.][A-Za-z0-9_.$]*`; local names use ordinary IR identifiers.

`metail linkage` and the C/C++ linkage APIs emit a version-1 JSON manifest, with
declaration-ordered imports and deduplicated library/driver-argument arrays.
`"c"` becomes `-lc`; `"./libfoo.so"` remains one path argument. Append these after
referring objects without a shell. Library resolution and runtime loader paths
belong to the link driver. Integer extension, pointer narrowing checks and
variadic ABI setup belong to a target lowerer; ABI types do not change signed-64
expression semantics. Extra variadic expressions are signed-64 scalar arguments,
not implicitly floating-point or aggregate values. See [README](../README.md#libc-and-shared-library-imports).

## Extending the language

Add tokens/grammar to `lua/metail/parser.lua`, constructors and printing to `lua/metail/ir.lua`, native checked constant arithmetic to `lua/lkopt.c`, dataflow to `lua/metail/analysis.lua`, and transform passes to `lua/metail/transform.lua`. Add a parsing/round-trip test and a semantic test for each operator change.
