# 07 Code Generation, CLI, and Library API

## CLI

Basic compile:

```bash
parzek compile Foo.pzg
```

With visitor generation:

```bash
parzek compile Foo.pzg --create-visitor=FooVisitor.hpp
```

Options:

- `--output-dir=DIR`
- `--output-base=NAME`
- `--stdin`
- `--help`

Outputs:

- `NAME-Parzek.hpp`
- `NAME-Parzek.cpp`
- optional visitor header

## Library API

In `Parzek.hpp`:

- `compile_grammar_string(std::string_view, const CompileOptions&)`
- `compile_grammar_file(const std::string&, const CompileOptions&)`

Both return `CompileResult`.

## Compile options

`CompileOptions` controls:

- output directory
- basename override
- optional visitor filename
- source name (diagnostics origin)

## Diagnostics model

`CompileResult` includes:

- success flag
- generated file paths
- optional visitor path
- list of diagnostics (`severity`, `message`, `file`, `line`, `column`)

## Generated code shape

Generated parser files include:

- parser class
- predicate context
- local support combinators
- rule-specific parser methods

The support block keeps generated rule bodies concise and readable.
