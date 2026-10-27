# 42 — Testing and reference comparisons

Run the configured CTest suite after building:

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build -R 'source-features|manual-examples' --output-on-failure
```

Native tests cover exact source input, document structure, HTML, and basic PDF
assertions, plus diagnostics and resource bounds. Integration tests exercise
the CLI, dynamic plugins, interactive terminal behavior, vector rendering,
and scripting. The manual-example target also checks its 44-chapter index and
compiles the marked examples to all four CLI targets.

`native-tex82-oracle` runs selected expansion, category-code, and integer cases
independently through MetaTeX and TeXScrape INITEX, checking both against expected
results. These comparisons need no installed TeX engine, fonts, or format
image. Broader TeXScrape Plain TeX tests use available font fixtures and may
skip font-dependent portions when those fixtures are absent.

Passing this suite does not establish complete TeX82 compatibility. Official
TRIP and representative LaTeX regression suites remain acceptance requirements
for such a claim. Add a regression for the behavior being changed rather than
only checking that a new command exists. For renderer changes, include a
structural output assertion and inspect representative pages when typography
or pagination is involved. Record backend configuration so font-dependent
results can be reproduced.

[Contents](README.md)
