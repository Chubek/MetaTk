# Command-Line Reference

Document `-o`, `-e`, `--target`, `--base`, `--catalogue`, `-L`, `-l`, `--map`, response files, `--help`, and `--version`. Unknown options fail with a usage error.

See the neighboring chapters for the surrounding workflow and the project README for build commands and current limitations.

The common form is:

```text
ArchLD [options] object.o [archive.a ...]
```

`-L DIR` adds an archive search directory and `-lNAME` searches it for
`libNAME.a`. `--map FILE` writes a human-readable placement and symbol map.
Options are processed in order where that affects input and library discovery.
Use `--` before an input whose name begins with a dash.
