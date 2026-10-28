# 11. Modules, functions, parameters, and calls

A module contains one or more named functions. Function parameters are positional
signed-64 scalar values; there are no type annotations, overloads, varargs, or
calling-convention declarations in the source language.

## Define related functions

<!-- check: juniper -->
```juniper
@module(arithmetic_service)
@fn(add_bias, (x, bias))
  @return(add(x, bias))
@end

@fn(scale_bias, (x, factor, bias))
  @return(add_bias(mul(x, factor), bias))
@end
```

This parses as two functions and a call named `add_bias`. Do not infer that the
frontend has linked that call, inlined the first function, checked its external
ABI, or made it pure. Calls are conservatively effectful, including calls with
the same name as a function in the module.

## Distinguish operators from calls

Canonical operator names have known arities and semantics:

<!-- check: juniper no-preprocess -->
```juniper
fn compare(x, y) {
  return add(eq(x, y), lt(x, y));
}
fn external_value(x) {
  return device_read(x);
}
```

`eq`, `lt`, and `add` are operators. `device_read` is a call; its implementation
is not supplied by Juniper. An incorrect operator arity is a parse/validation
error, not a different overloaded function:

<!-- check: juniper error=JUN-E100 -->
```juniper
fn broken(x) {
  return add(x);
}
```

## Empty arguments and multiple evaluations

<!-- check: juniper -->
```juniper
@fn(acquire, ())
  @let(first, tick())
  @let(second, tick())
  @return(sub(second, first))
@end
```

`@fn(acquire, ())` emits an empty parameter list. `tick()` is an ordinary
zero-argument call. Selection emits `CALL` instructions and preserves their
source evaluation order. The machine instruction's first operand is the callee
symbol; the remaining operands are the evaluated arguments.

```sh
"$J" select "$MANUAL_EXAMPLES/effects.jun" -o "$WORK/calls.json"
python3 -m json.tool "$WORK/calls.json"
```

## Design interfaces explicitly

If your eventual backend wants functions with a specific number of arguments,
known callees, or a designated return register, enforce that as an application
policy before encoding. The canonical validator checks names/structure and CFG
targets, not call resolution or definite assignment.

Avoid giving a function an operator name such as `add`; expression syntax treats
that spelling as the builtin operator. Prefer names such as `project_add` for
external service calls. Use unique function and parameter names, initialize
locals on every path where they are read, and return deliberately from each
intended exit. Successful parsing is not proof of termination or executable ABI
compatibility.

[Previous](10-configuration.md) · [Index](README.md) · [Next](12-integer-semantics.md)
