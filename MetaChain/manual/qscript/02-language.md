# 2. The language

A QScript source file is a sequence of *items*: statements, `fn` and `const`
declarations, and `use` directives, in any order. Statements run top to
bottom; declarations are visible throughout the file.

## 2.1 Lexical structure

QScript has no separate lexer: the grammar matches characters directly (see
[chapter 5](05-grammars.md)). The rules below describe what that grammar
accepts.

**Whitespace and comments** may appear between any two tokens:

```qscript
// to end of line
# also to end of line, as in Perl
/* block comments
   may span lines */
return 0;
```

**Identifiers** start with a letter or `_` and continue with letters, digits
and `_`. They are case-sensitive. A qualified name joins identifiers with
`::`, e.g. `math::gcd` or `date::DAY`.

**Keywords** cannot be used as identifiers:

```
use fn sub const let my if elsif else unless while until do for loop
break last continue next return exit print say assert and or not true false
```

**Integer literals** are decimal, hexadecimal (`0x`), binary (`0b`) or octal
(`0o`), with optional `_` separators. A leading zero does not mean octal.

| Literal | Value |
|---------|-------|
| `42`, `1_000_000` | 42, 1000000 |
| `0xff`, `0xFF_FF` | 255, 65535 |
| `0b1010` | 10 |
| `0o17` | 15 |
| `07` | 7 |
| `true`, `false` | 1, 0 |

A literal must fit in a signed 64-bit integer. `-9223372036854775808`
(INT64_MIN) is accepted as a negated literal.

**Character literals** are single characters in single quotes and denote the
character's code: `'a'` is 97. Escapes: `\n` `\r` `\t` `\0` `\\` `\'` `\"`
and `\xHH`.

```qscript
assert 'A' == 65 && '\n' == 10 && '\x41' == 'A';
```

## 2.2 Values and arithmetic

There is one type: the signed 64-bit integer.

* `+ - *` wrap around on overflow (two's complement): `9223372036854775807 + 1`
  is `-9223372036854775808`. Use the `checked` library to detect overflow.
* `/` truncates toward zero and `%` takes the sign of the dividend (as in C).
  The `math` library has `div_floor`, `div_ceil` and `mod_floor`.
* `>>` is a **logical** shift (zeros enter from the left): `-1 >> 60` is 15.
  `bits::sar` is the arithmetic shift.
* Comparisons and logical operators produce 0 or 1. Any non-zero value is true.

Some operations **trap**: the program stops with a diagnostic instead of
producing a value.

| Trap | Example |
|------|---------|
| division or remainder by zero | `1 / 0`, `5 % 0` |
| `INT64_MIN / -1` (and `% -1`) | `-9223372036854775808 / -1` |
| shift count outside 0..63 | `1 << 64`, `1 >> -1` |
| failed `assert` | `assert 1 == 2;` |
| stack overflow (too deep recursion) | `fn f(n) { return f(n + 1); }` |
| step limit exceeded (10 million by default) | `while (1) {}` |

## 2.3 Variables and scope

```qscript
let x = 10;         # declare and initialize
my  y = x * 2;      # `my` is a synonym for `let`
x = y + 1;          # assign to an existing variable
x += 5;             # compound assignment
x++;                # increment (statement only)
return x;
```

* Every variable must be declared with an initial value before use.
* A block `{ ... }` opens a new scope. An inner declaration may shadow an outer
  one; a name may not be declared twice in the same scope.
* The bodies of `if`, `unless`, `while`, `until`, `for` and `do` always have
  their own scope, even without braces.
* At most **16 variables** may be live at once in one function (MetaIL has 32
  registers; QScript reserves 16 for variables). Variables in finished blocks do
  not count. Constants (`const`) never count.

Compound assignment operators: `+= -= *= /= %= &= |= ^= <<= >>= **=`.
`x++` and `x--` are statements, not expressions.

## 2.4 Operators

From lowest to highest precedence:

| Level | Operators | Associativity | Notes |
|-------|-----------|---------------|-------|
| 1 | `? :` | right | `c ? a : b`, only the chosen branch is evaluated |
| 2 | `\|\|` `or` | left | short-circuit; result 0 or 1 |
| 3 | `&&` `and` | left | short-circuit; result 0 or 1 |
| 4 | `\|` | left | bitwise or |
| 5 | `^` | left | bitwise xor |
| 6 | `&` | left | bitwise and |
| 7 | `==` `!=` | left | |
| 8 | `<` `<=` `>` `>=` | left | |
| 9 | `<<` `>>` | left | `>>` is logical |
| 10 | `+` `-` | left | |
| 11 | `*` `/` `%` | left | |
| 12 | unary `-` `+` `!` `not` `~` | prefix | `!`/`not` give 0 or 1; `~` is bitwise not |
| 13 | `**` | right | power; binds tighter than unary minus |

As in mathematics (and Raku and Python), `-2 ** 2` is `-(2 ** 2)` = `-4`, and
`2 ** 3 ** 2` is `2 ** 9` = 512. A negative exponent gives the truncated
reciprocal: `2 ** -1` is 0, `1 ** -5` is 1, `(-1) ** -3` is -1. Powers wrap
around like multiplication.

`and`, `or` and `not` are spelled-out synonyms of `&&`, `||` and `!` with the
same precedence.

The comparison and bitwise levels follow C, so `x & 1 == 0` parses as
`x & (1 == 0)`; write `(x & 1) == 0`.

## 2.5 Statements

### Expressions, output, assertions

```qscript
use math;
say 1, 2, 3;           # print each value on its own line (`print` is the same)
assert 2 + 2 == 4;     # trap with "assertion failed" if false
gcd(10, 4);            # a call as a statement; the value is discarded
```

### Conditionals

```qscript
let n = 7;
if (n < 0) { say -1; }
elsif (n == 0) { say 0; }
else { say 1; }

unless (n % 2 == 0) say 1;    # `unless (c)` is `if (!c)`; it may have an `else`
```

### Loops

```qscript
let i = 0;
while (i < 3) i++;
until (i == 0) i--;                   # `until (c)` is `while (!c)`
do { i++; } while (i < 5);            # body runs at least once
do { i--; } until (i == 0);
for (let k = 0; k < 10; k += 3) say k;
for (;;) { break; }                   # every part of `for` is optional
loop { i++; last if i == 4; }         # `loop` is `while (1)`
return i;
```

`break` (alias `last`) leaves the innermost loop; `continue` (alias `next`)
starts its next iteration. In a `for` loop, `continue` runs the step first; in
`do ... while/until` it jumps to the condition. Using either outside a loop is
a compile-time error.

A `let` in the `for` initializer is scoped to the loop.

### Statement modifiers

Simple statements (expression statements, assignments, `x++`, `print`/`say`,
`assert`, `return`, `exit`, `break`/`last`, `continue`/`next`) may be followed by
`if COND` or `unless COND`, as in Perl:

```qscript
for (my i = 0; i < 10; i++) {
    next if i % 2;
    say i unless i == 4;
    last if i > 6;
}
```

### Ending the program

* `return EXPR;` at the top level ends the program with that result. Inside a
  function it returns from the function.
* `exit EXPR;` ends the whole program from anywhere, including inside functions.
* `return;` and `exit;` mean `return 0;` and `exit 0;`.
* Running off the end of the program gives result 0.

### Empty statements and blocks

`;` on its own is an empty statement; `{ }` is an empty block.

## 2.6 Program structure

```qscript
use math;                    # library imports (anywhere at top level)
const LIMIT = 100;           # compile-time constants
fn square(x) { return x * x; }   # functions

let total = 0;               # statements
for (my i = 1; i <= LIMIT; i++) total += square(i) % 7;
return total;
```

Declarations may appear before or after their uses. Functions see their own
parameters and locals, all constants, and all functions; they do **not** see
the top-level program's variables (see [chapter 3](03-functions.md)).
