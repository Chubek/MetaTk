Chapter 9 - CLI and REPL
========================

The CLI uses Klyspec for argument parsing and PikoRL as the REPL backend.

Tracing and Symbol Dumps
------------------------
``ekippx-cli --trace`` enables runtime trace collection for batch expansion. ``--trace-file PATH`` writes the SerdeTk-backed trace document; ``--trace-format json`` emits a readable JSON document and ``--trace-format msgpack`` emits the compact SerdeTk MessagePack carrier.

``ekippx-cli --dump-symtbl PATH`` writes registered directives, functions, expanders, conditionals, macros, symbols, and loaded plugin names. The dump intentionally excludes environment values and file contents.

PikoRL Host Bindings
--------------------
Interactive mode embeds PikoRL directly. The REPL exposes ``ekippx_expand(TEXT)``, ``ekippx_trace()``, and ``ekippx_symbols()`` as host-provided bindings, so batch expansion, trace inspection, and symbol-table inspection share the same runtime model.
