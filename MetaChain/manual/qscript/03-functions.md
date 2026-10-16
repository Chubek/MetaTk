# 3. Functions and constants

## 3.1 Functions

```qscript
fn gcd(a, b) {
    while (b != 0) { let t = a % b; a = b; b = t; }
    return a;
}
sub lcm(a, b) { return a / gcd(a, b) * b; }   # `sub` is a synonym for `fn`

return lcm(4, 6);
```

* A function takes zero to 16 integer parameters and returns one integer.
* Parameters are local variables initialized by the caller; assigning to one
  does not affect the caller.
* `return EXPR;` returns a value; reaching the end of the body returns 0.
* Functions may be called before they are defined and may be recursive or
  mutually recursive. Recursion depth is limited by the VM stack (one million
  values by default); runaway recursion traps with "stack limit exceeded".
* A call must pass exactly as many arguments as the function has parameters;
  this is checked at compile time.
* Function names live in their own namespace: a variable named `max` does not
  hide a function named `max`.

### What a function can see

A function body sees its parameters and its own locals, every `const`, and
every function (its own file's and those of libraries it `use`s). It does
**not** see variables of the top-level program, and QScript has no global
variables. Pass state in and return it:

```qscript
use random;
fn roll(state) { return range(state, 1, 6); }
let s = seed(1);
for (my i = 0; i < 3; i++) { s = step(s); say roll(s); }
```

When a function needs to return several values, pack them into one integer
(the `pack` library has helpers; `date::civil` returns `YYYYMMDD`).

Each function has its own budget of 16 live variables, independent of its
callers.

### Exiting from a function

`return` inside a function returns to the caller. `exit EXPR;` ends the whole
program with EXPR as its result, wherever it is executed.

## 3.2 Constants

```qscript
const WIDTH = 80;
const AREA = WIDTH * 25;
const MASK = (1 << 12) - 1;
const BIG = 2 ** 40;
return AREA + MASK;
```

* A constant's value is computed at compile time from literals, other
  constants and operators (including `**`, `?:`, `&&`, `||`). Function calls are
  not allowed.
* Constants may refer to constants declared later; cycles are an error.
* A constant cannot be assigned to, and a variable cannot be declared with the
  name of a constant from the same file.
* Constants are folded into the code that uses them: they cost no registers
  and do not count against the 16-variable limit. Prefer `const` over
  top-level `let` for fixed values.
* Constants are visible in every function of the file, and libraries export
  them like functions (`math::INT_MAX`).

Constant expressions follow run-time arithmetic exactly: they wrap on
overflow, and division by zero or a bad shift count is a compile-time error.

## 3.3 Calling from the host

Every function defined by the program (not only those it calls) is compiled
and can be called from C or C++, along with any library function the program
uses:

```sh
$ cat area.qs
fn rect(w, h) { return w * h; }
fn tri(b, h) { return b * h / 2; }
$ qscript call area.qs -- tri 10 7
35
```

```cpp
auto unit = qscript::compile_unit(source);
auto result = qscript::call(*unit, "tri", {10, 7});   // result->value == 35
```

The top-level statements do not run when a function is called this way.
See `qs_call` in [chapter 6](06-api.md).

## 3.4 Cost model

Calls are real MetaIL `call`/`ret` instructions, not inlining. A call saves the
caller's live variables and in-flight temporaries on the VM stack, pushes the
arguments, and restores everything afterwards, so its cost grows with the
number of live variables at the call site. In a hot loop, a function with few
live variables at its call sites is cheapest. [Chapter 9](09-internals.md)
describes the exact convention.
