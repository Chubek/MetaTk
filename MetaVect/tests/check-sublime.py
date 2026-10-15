"""Validate the Sublime grammar against the compiler lexer contract.

Checks (best-effort mirror, like the grammar itself):
- YAML parses with the required top-level keys and contexts.
- Every TokenKind spelling from frontend/Token.d is covered: reserved words
  in the keyword/boolean rules, operators and delimiters in the operator
  and punctuation rules.
- Block comments nest (block-comment pushes block-comment).
- Number/escape rules behave on samples via Python `re` (patterns are simple
  enough to be engine-agnostic): floats, integers, invalid numbers, valid
  and invalid escapes.
"""
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
SYNTAX = ROOT / "extern/Sublime/MetaVect.sublime-syntax"
TOKENS = ROOT / "frontend/Token.d"


def fail(message):
    print(f"sublime check failed: {message}")
    return 1


def main():
    data = yaml.safe_load(SYNTAX.read_text())
    for key in ("name", "scope", "file_extensions", "contexts"):
        if key not in data:
            return fail(f"missing top-level key: {key}")
    if sorted(data["file_extensions"]) != ["mv", "mvct", "mvec"]:
        return fail(f"unexpected file extensions: {data['file_extensions']}")

    contexts = data["contexts"]
    for context in ("main", "comments", "block-comment", "strings",
                    "string-body", "numbers"):
        if context not in contexts:
            return fail(f"missing context: {context}")

    # Block comments must nest like the lexer (`{- ... -}` depth counts).
    pushes = [rule.get("push") for rule in contexts["block-comment"]
              if isinstance(rule, dict)]
    if "block-comment" not in pushes:
        return fail("block-comment does not push itself (nesting lost)")

    text = SYNTAX.read_text()

    # Every token spelling in Token.d must be covered by the grammar.
    spellings = dict(re.findall(
        r'case TokenKind\.(\w+): return "([^"]*)";', TOKENS.read_text()))
    if len(spellings) < 40:
        return fail(f"could not extract token spellings ({len(spellings)})")
    keywords = [s for k, s in spellings.items()
                if k.startswith("kw") and s not in ("true", "false")]
    for word in keywords:
        if not re.search(r"match:.*\b" + re.escape(word) + r"\b", text):
            return fail(f"keyword not covered: {word}")
    if "true" not in text or "false" not in text:
        return fail("boolean literals not covered")
    operators = [s for k, s in spellings.items()
                 if k not in ("eof", "identifier", "integer", "floating",
                              "stringLiteral") and not k.startswith("kw")]
    # Sublime escapes regex metacharacters (`||` is written `\|\|`).
    plain = text.replace("\\", "")
    for spelling in operators:
        if spelling not in plain:
            return fail(f"operator/delimiter not covered: {spelling!r}")

    # Behavioral checks on the actual patterns from the file.
    by_scope = {}
    for context_rules in contexts.values():
        for rule in context_rules:
            if isinstance(rule, dict) and "match" in rule and "scope" in rule:
                by_scope.setdefault(rule["scope"], []).append(rule["match"])

    def scoped(scope, pattern):
        return any(re.search(pattern, sample)
                   for sample in by_scope.get(scope, []))

    float_pats = by_scope.get("constant.numeric.float.metavect", [])
    int_pats = by_scope.get("constant.numeric.integer.metavect", [])
    invalid_pats = by_scope.get("invalid.illegal.number.metavect", [])
    if not (float_pats and int_pats and invalid_pats):
        return fail("number scopes missing")

    def matches(pats, sample):
        return any(re.search(p, sample) for p in pats)

    def fullmatches(pats, sample):
        # Mirror Sublime's first-match-wins: a valid rule must consume the
        # whole token, otherwise the invalid rules below it claim the text.
        return any(re.fullmatch(p, sample) for p in pats)

    for sample in ("1.25", "1e3", "4.5E-2"):
        if not fullmatches(float_pats, sample):
            return fail(f"float not matched: {sample}")
    if not fullmatches(int_pats, "42"):
        return fail("integer not matched: 42")
    for sample in ("12abc", "1e", "1.0E-"):
        if fullmatches(float_pats + int_pats, sample):
            return fail(f"invalid number matched as valid: {sample}")
        if not matches(invalid_pats, sample):
            return fail(f"invalid number not flagged: {sample}")

    escape_pats = by_scope.get("constant.character.escape.metavect", [])
    bad_escape_pats = by_scope.get("invalid.illegal.escape.metavect", [])
    if not (escape_pats and bad_escape_pats):
        return fail("escape scopes missing")
    if not matches(escape_pats, r"\n"):
        return fail(r"valid escape \n not matched")
    if matches(escape_pats, r"\q"):
        return fail(r"invalid escape \q matched as valid")
    if not matches(bad_escape_pats, r"\q"):
        return fail(r"invalid escape \q not flagged")

    print("Sublime grammar checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
