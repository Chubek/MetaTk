# 34 — JSON and text output

JSON is the most direct CLI view of the semantic document. Its current format
marker is `metatex-document-v1`. Top-level fields include `title`, `author`,
`date`, `blocks`, `references`, and `diagnostics`.

```sh
build/metatex -f plain -T json -o document.json document.tex
build/metatex -f plain -T text -o document.txt document.tex
```

Each block has a type, level, ordered flag, identifier, and spans. Each span
has a type, text, and an extra field for information such as a link destination.
Vector blocks also include typed scene dimensions, paints, and path commands.
Source-level macro definitions and complete token histories are not serialized.

Text output concatenates visible span text, uses simple heading/list markers,
and separates blocks with newlines. It resolves reference labels but does not
preserve font appearance or graphical geometry. Math is represented by its
stored source, making text useful for debugging and indexing rather than
faithful visual reproduction.

The batch CLI refuses to write output when the document has errors, so a
failed compilation will not normally leave an error-bearing JSON file.
Embedders can inspect `Document::diagnostics` directly and call `renderJSON`
when diagnostic serialization is desired. Treat the version marker as part of
an integration contract and validate required fields before consuming a file
from a different MetaTeX build.

[Contents](README.md)
