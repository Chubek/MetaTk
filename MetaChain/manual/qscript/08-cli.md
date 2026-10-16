# 8. Command line and REPL

```
qscript [OPTIONS] COMMAND ...
qscript --repl
qscript --help
```

## Commands

| Command | Does |
|---------|------|
| `run FILE...` | compile and run; prints program output, then `result N` |
| `check FILE...` | parse and check; prints `ok` |
| `ir FILE...` | print the MetaIL program |
| `native FILE...` | print x86-64 assembly |
| `encode FILE... -o OUT` | write MCIL bytecode |
| `object FILE... -o OUT` | write an ELF64 relocatable object |
| `build FILE... -o OUT` | write a static x86-64 Linux executable |
| `call FILE... -- FUNC [ARG...]` | call one function with integer arguments (decimal, `0x`, or `0` octal) and print its result |
| `report FILE...` | backend report: basic blocks, critical paths, makespan |
| `ast FILE` | print the AST as an S-expression |
| `tree FILE` | print the grammar match tree |
| `grammar` | print the QScript grammar |
| `parse GRAMMAR INPUT [RULE]` | parse INPUT with a grammar file and print the match tree |
| `libs` | list the standard libraries |
| `lib NAME` | print a standard library's source |

Several FILEs are compiled together as one module (earlier files act as a
prelude for later ones; see chapter 4). `-` reads standard input and cannot be
combined with other files.

## Options

Options go before the command.

| Option | Effect |
|--------|--------|
| `-L DIR`, `-LDIR` | add a library directory for `use` (repeatable) |
| `--no-stdlib` | do not resolve `use` against the embedded standard library |
| `--jit` | execute with MetaJIT |
| `--stats` | after `run`, print steps, outputs and program size to standard error |
| `--sched list\|cycle\|static` | instruction scheduler |
| `-O0` | disable the MetaIL optimizer |
| `--steps N` | execution step limit (default 10,000,000) |

## Exit status

`0` on success, `1` for compile errors, traps and I/O errors (the message is
on standard error, as `file:line:column: message`), `2` for usage errors.
A native executable built with `build` exits with the program's result
(modulo 256), or with 124 (step limit), 125 (stack), 126 (arithmetic trap,
including failed `assert`) or 127 (output error).

## Examples

```sh
$ qscript run examples/primes.qs
2
3
...
35
result 1

$ qscript --stats run examples/primes.qs > /dev/null
steps 2564912, outputs 11, instructions 732, engine interpreter

$ echo 'use combin; fn c(n, k) { return binomial(n, k); }' > c.qs
$ qscript call c.qs -- c 52 5
2598960

$ qscript build examples/fizzbuzz.qs -o fizzbuzz && ./fizzbuzz | head -3
1
2
3

$ qscript -L ./lib run main.qs          # also search ./lib/NAME.qs for `use NAME;`
```

## The REPL

`qscript --repl` (or `qscript-repl`) reads one line at a time.

* A line starting with `fn`, `sub`, `const` or `use` is a declaration. It is
  checked and remembered, and prepended to every later line.
* Any other line runs as a complete program (with the remembered
  declarations); its result is printed. Variables do not carry over between
  lines.

```
qscript> use math;
qscript> fn hyp(a, b) { return isqrt(a * a + b * b); }
qscript> return hyp(5, 12);
13
qscript> let x = 3; print x * x; return gcd(x, 12);
9
3
```

| Command | Effect |
|---------|--------|
| `:help` | help |
| `:decls` | show remembered declarations |
| `:reset` | forget them |
| `:libs` | list the standard library |
| `:quit`, `:q` | exit |
