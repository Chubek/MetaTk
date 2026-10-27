# Jackscript preprocessing

Select `--profile jackscript` to preprocess `.jcks` sources:

```sh
build/gppx --profile jackscript examples/jackscript/ReadCSV.jcks
build/gppx --profile jackscript '-DINPUT="customers.csv"' examples/jackscript/ReadCSV.jcks
```

The profile uses `#` directives, `$(NAME)` and `$(FUNCTION(args))` macros,
C-style comments, and protected string literals. Ordinary Jackscript identifiers
are preserved. A shebang is copied through. Registered `.jcks` readers validate
UTF-8, reject embedded NUL bytes, and remove an optional UTF-8 BOM.

The example demonstrates generation of Reader/String library calls. GPPX emits
source; execution and those library implementations belong to the downstream
Jackscript runtime.

From C, include `<GPPX-Jackscript.h>` and call
`gppx_jackscript_enable(state)`. The public reader callback can also be registered
for another extension. From C++, use `engine.set("profile", "jackscript")`.
Selecting the profile through CLI configuration installs the same UTF-8 reader.
Reader registrations persist when another syntax profile is selected.
