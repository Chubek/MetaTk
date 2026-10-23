# Chapter 35 — Built-in Preprocessing and Optional Lua

`Preprocessor` transforms source text before rule/grammar parsing. Macro and
conditional operations are built in. The user's include and script callbacks
choose the source backend and host-language embedding policy.

## Start with object macros

```cpp
dsl::Preprocessor pp;
auto rules = dsl::sexpr_rules(pp.process(R"(
#define ZERO 0
(rewrite add-zero (add ?x ZERO) ?x)
)"));
assert(rules.size() == 1);
```

```d
auto pp = new Preprocessor;
auto rules = sexpr_rules(pp.process(`
#define ZERO 0
(rewrite add-zero (add ?x ZERO) ?x)
`));
assert(rules.length == 1);
```

The preprocessor also exposes `define(name, replacement)` and the public
`macros` table. An object macro expands identifier tokens. Strings, character
literals and C-style comments preserve their contents. Expansion rescans
replacement text; recursive expansion of the same macro is suppressed.
For example, defining `SELF` as `SELF` leaves its use as `SELF`.

`expand(text)` performs expansion alone. `process(text)` additionally handles
directives and removes directive lines from output. The macro table persists
across calls and included sources.

## Function macros and continued lines

```text
#define ID(x) x
#define ADD(x, y) (add x y)
ID(ADD(a, 0))
```

Function macro definitions require `(` immediately after the name. Invocations
can contain whitespace before `(`. Arguments are separated at their outer
comma level; balanced parentheses and quoted values preserve internal commas.
Arguments expand before substitution, and the replacement is rescanned.
Invalid parameter lists and mismatched argument counts raise exceptions.

`define_function(name, parameters, replacement)` provides programmatic
registration. Backslash-newline pairs splice physical lines before directive
processing, so a replacement can span several source lines.

The implemented function-macro subset has named parameters. Token-pasting,
stringification and variadic macro syntax are outside the current API.

## Conditional source selection

```text
#define ENABLED 1
#if defined(ENABLED) && ENABLED == 1
(= (add ?x 0) ?x)
#elif defined(ALTERNATIVE)
(= (mul ?x 1) ?x)
#else
(= (sub ?x 0) ?x)
#endif
```

Supported directives are `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else` and
`#endif`. Nested blocks maintain parent activity and previous branch selection.
Inactive branches do not define macros, resolve includes or execute scripts.
Unmatched, duplicate or unterminated conditional blocks raise exceptions.

Conditions accept decimal/hexadecimal integer constants, identifiers, `defined`
with or without parentheses, logical `!`, parentheses, comparisons
`== != < <= > >=`, and `&&`/`||`. Undefined identifiers evaluate to zero.
Object-macro replacements used in conditions are evaluated recursively.
Arithmetic, bitwise expressions and function-macro evaluation in conditions
are outside the implemented expression subset.

`#undef NAME` removes a macro. Active `#error message` raises an exception;
inactive `#error` directives are skipped. Unknown active directives are rejected.

## Resolve includes through a backend

```cpp
pp.include = [](std::string_view name) -> std::string {
    if (name == "common.rules") return "#define ZERO 0\n";
    throw std::runtime_error("unknown include");
};
auto output = pp.process("#include \"common.rules\"\n(= (add ?x ZERO) ?x)\n");
```

```d
pp.include = (string name) {
    if (name == "common.rules") return "#define ZERO 0\n";
    throw new Exception("unknown include");
};
auto output = pp.process("#include \"common.rules\"\n(= (add ?x ZERO) ?x)\n");
```

Paths may be quoted or angle-bracketed; the resolver receives the path without
delimiters. Include text is processed recursively and shares the macro table.
The toolkit does not select filesystem search directories or a concrete I/O
backend. A resolver may compose the abstract I/O facilities in Chapter 33.

`max_expansion_depth` defaults to 100 and `max_include_depth` to 32. Exceeding
either raises an exception. Exceptions from user callbacks propagate.

## Supply a script backend

`script(language, code)` returns generated text for `#script language code`.
`#lua code` is shorthand for language `lua`. The preprocessor macro-expands
the returned text. It does not re-run generated text as a set of directives.
Supply a trailing newline deliberately when returning several generated lines.

```d
pp.script = (string language, string code) {
    assert(language == "lua" && code == "emit_rule");
    return "(= (add ?x ZERO) ?x)";
};
assert(sexpr_rules(pp.process("#lua emit_rule\n")).length == 1);
```

This callback contract is usable for any scripting engine. The native Lua
bridge implements it for a user-owned Lua state.

## Enable native Lua in C++

Define `DSLTK_ENABLE_LUA` before including the header and supply Lua's include
and link flags:

```cpp
#define DSLTK_ENABLE_LUA
#include "DSLtk.hpp"
#include <cassert>

int main() {
    auto state = luaL_newstate();
    assert(state);
    dsl::Preprocessor pp;
    pp.script = dsl::lua_script_hook(state);
    assert(pp.process("#lua return '(= a b)'\n") == "(= a b)\n");
    assert(lua_gettop(state) == 0);
    lua_close(state);
}
```

```sh
# From the DSLtk directory, with the example saved as lua-app.cpp:
g++ -std=c++20 -I. lua-app.cpp $(pkg-config --cflags --libs lua5.4) -o lua-app
```

Use the pkg-config name supplied by the installed Lua package. The bridge
uses the supplied state; opening libraries and configuring it are user operations.

## Enable native Lua in D

```d
import DSLtk;

void main()
{
    auto state = luaL_newstate();
    assert(state !is null);
    scope(exit) lua_close(state);
    auto pp = new Preprocessor;
    pp.script = lua_script_hook(state);
    assert(pp.process("#lua return '(= a b)'\n") == "(= a b)\n");
    assert(lua_gettop(state) == 0);
}
```

```sh
ldc2 -d-version=DSLtkLua lua-app.d DSLtk.d -L-llua5.4 -of=lua-app
dmd -version=DSLtkLua lua-app.d DSLtk.d -L-llua5.4 -of=lua-app
gdc -fversion=DSLtkLua lua-app.d DSLtk.d -llua5.4 -o lua-app
```

The D declarations target the Lua 5.3/5.4 ABI. A directive loads and executes
its code with one return value, obtains generated text, and restores the original
stack height after success or failure. Keep the state alive while its hook is
used. Configure standard libraries with `luaL_openlibs` when the script needs
them. The bridge is optional; default builds use the standard library only.

## Verify preprocessing

The ordinary facility tests cover macros, inactive branches, comments,
continuations, include resolution and callback-based scripts. To enable the
native C++ bridge test, configure the standalone test project with
`-DDSLtk_TEST_LUA=ON`. For D native-bridge tests:

```sh
ldc2 -unittest -main -d-version=DSLtkLua DSLtk.d -L-llua5.4 -of=dsltk-lua-test
./dsltk-lua-test
```
