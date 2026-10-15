"""Static cross-checks for the Vim integration.

- Help file has required tags, documents every highlight group, and ends
  with a help modeline.
- Syntax groups cover the lexer keywords from frontend/Token.d; the nested
  block-comment region must not use `keepend` (an inner `-}` would then also
  terminate the outer region).
- ftplugin/indent/ftdetect expose the expected options; file extensions
  agree with the Sublime grammar.
"""
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
VIM = ROOT / "extern/Vim"


def fail(message):
    print(f"vim check failed: {message}")
    return 1


def main():
    docs = (VIM / "docs/MetaVect.txt").read_text()
    if "*metavect.txt*" not in docs.splitlines()[0]:
        return fail("help file must start with a *metavect.txt* tag")
    for tag in ("*metavect*", "*metavect-contents*", "*metavect-filetype*",
                "*metavect-syntax*", "*metavect-indent*", "*metavect-tex*",
                "*metavect-commands*", "*metavect-install*"):
        if tag not in docs:
            return fail(f"help tag missing: {tag}")
    if not docs.rstrip().endswith("vim:tw=78:ts=8:ft=help:norl:"):
        return fail("help modeline missing")

    syntax = (VIM / "syntax/MetaVect.vim").read_text()
    for group in ("mvKeyword", "mvBoolean", "mvType", "mvNumber",
                  "mvNumberError", "mvString", "mvStringEscape",
                  "mvStringError", "mvOperator", "mvDelimiter",
                  "mvLineComment", "mvBlockComment"):
        if group not in docs:
            return fail(f"help does not document {group}")
        if group not in syntax:
            return fail(f"syntax file missing {group}")
    if "syn case match" not in syntax:
        return fail("syntax must enforce case-sensitive matching")
    block = next(line for line in syntax.splitlines()
                 if line.startswith("syn region mvBlockComment"))
    if "keepend" in block:
        return fail("keepend on mvBlockComment breaks nesting")
    if "contains=mvBlockComment" not in block:
        return fail("block comments must nest via contains")

    tokens = (ROOT / "frontend/Token.d").read_text()
    spellings = dict(re.findall(
        r'case TokenKind\.(\w+): return "([^"]*)";', tokens))
    keywords = [s for k, s in spellings.items() if k.startswith("kw")]
    keyword_lines = [line for line in syntax.splitlines()
                     if line.startswith("syn keyword mvKeyword")]
    for word in keywords:
        if word in ("true", "false"):
            if word not in syntax:
                return fail(f"boolean missing: {word}")
        elif not any(re.search(rf"(?<!\w){word}(?!\w)", line)
                     for line in keyword_lines):
            return fail(f"keyword missing from mvKeyword: {word}")

    ftplugin = (VIM / "ftplugin/MetaVect.vim").read_text()
    for setting in ("commentstring=--", "makeprg=mvect",
                    "errorformat=", "b:undo_ftplugin"):
        if setting not in ftplugin:
            return fail(f"ftplugin missing: {setting}")

    indent = (VIM / "indent/MetaVect.vim").read_text()
    for setting in ("GetMetaVectIndent", "indentexpr=",
                    "b:undo_indent"):
        if setting not in indent:
            return fail(f"indent file missing: {setting}")

    ftdetect = (VIM / "ftdetect/MetaVect.vim").read_text()
    sublime = yaml.safe_load(
        (ROOT / "extern/Sublime/MetaVect.sublime-syntax").read_text())
    for extension in sublime["file_extensions"]:
        if f"*.{extension}" not in ftdetect:
            return fail(f"ftdetect missing extension: {extension}")

    print("Vim integration checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
