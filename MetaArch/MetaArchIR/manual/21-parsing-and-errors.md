# Chapter 21 — Parsing and diagnostics

## Parse with a useful filename

```cpp
auto program = mair::parse(
    "form 11, rd=x10, rs1=x10, imm=-1\n", "decrement.mair");
auto target = mair::target("riscv64", "MetaArchDB");
auto image = mair::assemble(program, target);
```

`parse` accepts a string view, an optional source filename and optional `Limits`.
The returned Program owns its statement data; the original source buffer can be
released. The default filename is `<memory>`.

Parsing is target-independent. It can accept an encoder name that later fails
lookup, or a form index that is invalid for a selected target. This separation
lets applications choose targets after constructing assembly without implying
that the source is valid for every target.

## Catch structured errors

```cpp
try {
    auto p = mair::parse("align 3\n", "bad.mair");
    (void)p;
} catch (const mair::Error &e) {
    const auto &where = e.location;
    std::string message = e.what();
    // Present where.file, where.line, where.column and message in your UI.
    (void)where;
    (void)message;
}
```

Line and column numbers are one-based when supplied. Columns are byte-oriented;
many builder/semantic failures point at the statement's beginning rather than
the precise operand character. Manually built statements without `at` locations
can have empty filenames and zero positions.

## Failure propagation

MetaArch database diagnostics are converted to `mair::Error` messages. Rendering
and assembly attach the failing statement's location when propagating encoding
errors. Parsing and emission throw instead of returning a partially constructed
result to the caller.

Not every exception is a semantic error: allocation can throw `std::bad_alloc`,
and filesystem operations can throw `std::filesystem::filesystem_error`. An
application boundary should decide how to display those separately. Diagnostic
wording is useful for people, but should not be treated as a stable machine-readable
error-code protocol.

[Previous](20-operand-api.md) · [Contents](README.md) · [Next](22-ownership.md)
