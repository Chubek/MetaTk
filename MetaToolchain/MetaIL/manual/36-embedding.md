# 36. Embedding and deployment

An embedded application needs the native library, compatible Lua 5.4 runtime, Lua
modules, and kernels. MetaNum is a build-time header dependency; DSLtk is a public
header dependency for the C++ API and is included in MetaIL's installation.

For a predictable deployment, pass the installed runtime root explicitly:

```cpp
metail::Context context("/opt/myapp/share/metail");
```

Alternatively set `METAIL_HOME`. Explicit roots and environment roots are checked
strictly rather than silently falling back to an unrelated installation. Contexts
anchor their root to an absolute path during creation, so changing the process
working directory later does not redirect runtime lookup.

Native operations do not spawn CLI subprocesses or write temporary program files.
Structured inputs are converted directly under protected Lua calls. This avoids
shell quoting issues and permits efficient integration with an application's own
IR generation, although conversion still copies tree structure.

Keep a Context alive across related operations to avoid repeatedly creating the
runtime and loading kernels. Keep Module values outside it when you need results
to survive context replacement. Load custom kernels into every context that must
execute them; a registry snapshot alone does not install passes in another runtime.

For shared builds, test both the installed CLI and an installed consumer from
outside the source directory. A working source-tree fallback can otherwise mask
missing runtime data in a deployment package.

[Contents](README.md) · [Next: Limits](37-limits-and-concurrency.md)
