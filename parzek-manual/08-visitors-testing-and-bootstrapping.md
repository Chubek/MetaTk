# 08 Visitors, Testing, and Bootstrapping

## Visitor generation

When enabled, Parzek emits a visitor header with one hook per syntactic rule:

- `visit_<rule>(const std::string&)`

This gives a stable integration point for semantic passes.

## Recommended workflow

1. Author/update grammar
2. Compile grammar with `parzek compile`
3. Compile generated C++ parser
4. Validate parser behavior on sample inputs
5. Add or update visitor hooks for semantic use

## Bootstrapping strategy

Parzek itself follows staged bootstrapping:

1. Preprocess grammar text
2. Parse grammar with combinator-based machinery
3. Generate C++ parser source
4. Use generated parsers in downstream flows

## Testing strategy

Production-ready coverage should include:

- preprocess macro edge cases (`!#n`, `#n!`, `#@`, `#!`, `##`)
- comments support (`//`, `/* */`)
- naming validation
- operator parsing (`|`, `*`, `+`, `?`, grouping)
- adjacency `~`
- channel-aware predicates (`CHAN`)
- CLI flow + error handling
- generated parser compile smoke tests

## Operational hardening checklist

- Keep generated output deterministic.
- Fail fast on invalid grammar syntax.
- Preserve diagnostic location detail.
- Sanitize output basenames/paths.
- Keep grammar examples as regression inputs.
