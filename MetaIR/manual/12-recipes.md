# 12. Integration recipes

## Parse a custom format into a DSLtk AST

```cpp
parsetk::Registry grammar;
grammar.add(parsetk::Lexeme<"@Number">(parsetk::Pattern::Repo::Integer));
grammar.add(parsetk::ProductionRule<"#Sum">(
    parsetk::Production::New("@Number ('+' @Number)*")));
metachain::Toolchain tc;
auto syntax = tc.parse_ast("10 + 20 + 12", grammar, "#Sum");
```

The root is `#Sum`. Its children alternate number and punctuation token leaves.
Lower the number spellings using checked integer conversion, not `atoi`, and
emit addition operations. Decide explicitly whether numbers outside signed
64-bit range are language errors, larger compile-time values, or another type.
The generic parser cannot infer that policy from a regex.

## Normalize an AST without losing leaf tags

```cpp
auto tree = dsl::node<"add">(
    dsl::leaf<"integer">(42), dsl::leaf<"integer">(0));
auto encoded = metachain::ast_to_term(tree);

using namespace rewrite;
Rule remove_zero("add-zero",
    node("node", {
        atom("add"), var("x"),
        node("leaf", {atom("integer"), atom("0")})}),
    var("x"));

metachain::Toolchain tc;
auto normal = tc.normalize(encoded, {remove_zero});
if (normal) {
    auto decoded = metachain::term_to_ast(*normal);
}
```

The representation is intentionally explicit: a node's tag is its first atom,
not an implicit variable naming convention. Rewriting must preserve the
`leaf`/`node` encoding if conversion back to AST is desired.

## Produce an executable without touching the filesystem

```cpp
metachain::Toolchain tc;
auto program = tc.compile("const r0, 42\nhalt r0\n");
if (program) {
    auto elf_bytes = tc.executable(*program);
    // Send bytes to a trusted deployment service after checking elf_bytes.
}
```

The application controls output naming, atomic storage, permissions, signing,
transport, and execution. This separation is useful for build servers that
generate artifacts on one machine and execute on another compatible target.

## Read/link several files with detailed reports

```cpp
ld_tk::Options options;
options.entry_symbol = "_start";
options.max_output_size = 16 * 1024 * 1024;
ld_tk::Linker linker(options);
auto result = linker.link_files({"start.o", "library.o"});
if (result) {
    std::string layout = result.report.dump();
}
```

Input names appear in diagnostics. This low-level API retains the layout report;
the facade's `link` method intentionally returns only the artifact and unified
diagnostics. Choose the lower-level component when its richer result is useful.

## Use a VM output callback

```cpp
auto parsed = metail::parse("const r0, 42\nprint r0\nhalt r0\n");
if (parsed) {
    metail::VM machine;
    machine.set_output_sink([](const metavm::Value& value) {
        // Forward value.as_integer() to an application logger or message bus.
    });
    machine.load(parsed.program);
    auto status = machine.run();
}
```

Using the VM directly exposes registers, PC, traces, and callbacks. Use
`metail::run` or `Toolchain::run` when you prefer a compact structured result.
The direct VM does not automatically reverify arbitrary changes to a program
you construct; call `verify` before loading untrusted instructions.

## Store binary programs safely

Validate text first, then encode. Decode and verify again when loading from
untrusted storage. Keep the binary format's own version and your application's
semantic version separate. Source filenames and maps are not included in MCIL,
so retain a sidecar source map if your application needs diagnostics against
the original source after a binary round-trip.

For integrity, add a checksum/signature and envelope outside MCIL rather than
repurposing its reserved bytes. Reserved bytes must remain zero in version 1.
Do not use native struct dumps as a faster-looking incompatible serialization.

## Embed textual commands behind a protocol

Map a protocol command enum to known `evaluate` command strings and pass the
payload separately. Return structured diagnostics rather than parsing stderr.
For binary outputs, send `TextResult::bytes` with an explicit content type and
length. For execution results, use `Toolchain::run` directly if you need the
full returned integer and captured output separately.

Apply authentication, request-size limits, concurrency limits, and wall-clock
cancellation in the host service. Library operation counts are not a substitute
for request isolation, especially around regex matching and filesystem I/O.
