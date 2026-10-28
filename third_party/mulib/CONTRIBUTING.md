# Contributing to MuLIB

Every new module must have a single self-contained `include/mulib-<name>.h`, a public header guard,
`extern "C"` declarations, a `MULIB_IMPL_<NAME>` implementation switch, a dedicated Doxygen comment
for the file and functions, one detailed `manual/mulib-<name>.md`, and a runnable
`examples/modules/<name>.c` example. Register it in `MANIFEST.yaml`.

Implementation switches must be defined in exactly one source translation unit. All public
objects use explicit ownership rules and `mulib_` symbol prefixes. Avoid globals that
cause symbol collisions, document failures and allocation limits, use `size_t` for
capacities, and check multiplication for overflow before allocating. Generic containers
currently copy fixed-stride bytes and are not automatic destructors of nested objects.

Before changing an interface, run `python3 tools/check.py`, the CMake/CTest suite,
strict GCC and Clang compilation if available, and ASan/UBSan functional tests.
Avoid copying third-party sources without recording licensing and provenance.
