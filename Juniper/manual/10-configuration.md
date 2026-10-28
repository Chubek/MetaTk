# 10. Conditional compilation and configuration

Configuration happens before IR parsing. It can choose literal values, include
files, functions, or backend declarations. Once emitted as canonical IR, that
choice is frozen into the artifact.

## Choose a compile-time constant

<!-- check: juniper -->
```juniper
#ifndef SCALE
#define @SCALE 8
#endif
@module(configuration)
@fn(scale, (x))
  @return(mul(x, @SCALE))
@end
```

Save this as `$WORK/configured.jun`. A CLI definition overrides the guarded
default:

```sh
"$J" metail "$WORK/configured.jun" -o "$WORK/scale8.mi"
"$J" metail "$WORK/configured.jun" -DSCALE=16 -o "$WORK/scale16.mi"
```

Write `-DSCALE=16`, not `-D@SCALE=16`; the driver expects a bare macro name in
`-D`. Source references use `@SCALE`, while `#ifdef`/`#ifndef` test `SCALE` without
`@`. `-D NAME=VALUE` and `-DNAME=VALUE` are both accepted; omitting `=VALUE` means
`1`. Definitions are text, not typed configuration fields.

## Choose a program fragment

<!-- check: juniper -->
```juniper
#ifndef USE_SHIFT
#define @USE_SHIFT 0
#endif
@fn(scale_eight, (x))
#if @USE_SHIFT
  @return(shl(x, 3))
#else
  @return(mul(x, 8))
#endif
@end
```

Both variants have the intended wrapping result for a valid constant shift
count, but their selected scheduling behavior need not be identical. The
lowerer conservatively fences shift operations as possible traps; multiplication
is not such a fence. Configuration can therefore affect scheduling even when
the arithmetic result is equivalent.

```sh
"$J" schedule "$WORK/scale-choice.jun" -DUSE_SHIFT=0 -o "$WORK/mul.json"
"$J" schedule "$WORK/scale-choice.jun" -DUSE_SHIFT=1 -o "$WORK/shift.json"
```

Save the preceding program as `scale-choice.jun` before running those commands.

## Keep variant dependencies real

The [capstone project](examples/capstone/Juniperfile) conditionally includes a
profile. Only actually included files enter the depfile. Record the root input,
`-D` values, include roots, pass list, and tool/runtime versions when publishing
an artifact. A dependency file does not by itself record every configuration
choice or the absence of a not-yet-created earlier include candidate.

For many variants, give each an output path:

```sh
"$J" emit "$WORK/configured.jun" -DSCALE=8 -o "$WORK/variant8.junir"
"$J" emit "$WORK/configured.jun" -DSCALE=16 -o "$WORK/variant16.junir"
"$J" schedule "$WORK/variant16.junir" --no-preprocess \
  -o "$WORK/variant16.scheduled.json"
```

Do not pass new `-D` values with `--no-preprocess`; there is no macro stage at
which they could take effect. To change a frozen variant, regenerate from macro
source or intentionally edit its canonical IR.

[Previous](09-includes.md) · [Index](README.md) · [Next](11-functions-and-calls.md)
