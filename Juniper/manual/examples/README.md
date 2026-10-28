# Runnable manual examples

These sources accompany the [36-chapter manual](../README.md). Build Juniper
first and set `J` and `METAIL_HOME` as explained in chapter 2.

| File/project | Purpose |
|---|---|
| [affine.jun](affine.jun) | Basic scalar expressions, locals, and optimization |
| [effects.jun](effects.jun) | Calls and possible traps that must remain observable |
| [loop.jun](loop.jun) | Explicit CFG, reassignment, and a counted loop |
| [lazy.jun](lazy.jun) | Conditional calls/division through lazy select |
| [parallel.jun](parallel.jun) | Two independent multiplies and a two-wide resource model |
| [includes/Juniperfile](includes/Juniperfile) | Root-relative nested includes and guards |
| [capstone](capstone/README.md) | Configurable fusion and a complete CMake artifact pipeline |
| [inspect_schedule.py](inspect_schedule.py) | Schema/ID/latency/issue/resource validation |
| [Support.hpp](Support.hpp) | Explicit application helpers for standalone C++ snippets |

```sh
"$J" schedule "$MANUAL_EXAMPLES/parallel.jun" -o "$WORK/parallel.json"
python3 "$MANUAL_EXAMPLES/inspect_schedule.py" "$WORK/parallel.json"
```

The complete C++ programs live in the chapters rather than diverging duplicate
files. The checker extracts exactly the marked Markdown code blocks and builds
them as a separate CMake consumer:

```sh
python3 "$JUNIPER_ROOT/manual/check_examples.py" \
  --juniper "$J" --metail-home "$METAIL_HOME" \
  --cpp --with-metaarch --build-dir "$WORK/manual-check"
```

Generated C++ source is written under the checker's build directory. Each test
receives `METAIL_HOME` and the Juniper source root as command-line arguments.
Copy a desired snippet plus `Support.hpp` into your own project if you want to
edit it independently. Do not edit the installed/source examples in place for
experiments; copy the project first.

Programs that mention `tick`, `device_read`, or another external call demonstrate
IR semantics and effect handling. They do not come with runtime implementations
of those calls, and no Juniper CLI execution command is assumed.
