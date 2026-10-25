"""Validate the TeX/LaTeX integration.

- Version 0.1.0 is consistent across MetaVect.h, MetaVect.d,
  extern/metavect.sty, extern/LaTeX/metavect.dtx, and extern/TeX/MetaVect.tex.
- The listings language covers the lexer keywords, type names, booleans,
  and both comment forms.
- docstrip extraction (latex metavect.ins) reproduces the shipped .sty
  byte-for-byte.
- The example document extern/TeX/Build.tex compiles with pdflatex
  (skipped gracefully when no LaTeX engine is installed).
"""
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
VERSION = "0.1.0"
DATE = "2026/10/15"


def fail(message):
    print(f"tex check failed: {message}")
    return 1


def main():
    # Builds run in temporary directories, so every path handed to a
    # subprocess (TEXINPUTS entries, document names) must be absolute.
    global ROOT
    ROOT = ROOT.resolve()
    header = (ROOT / "MetaVect.h").read_text()
    for part, value in (("MAJOR", "0"), ("MINOR", "1"), ("PATCH", "0")):
        if f"#define METAVECT_VERSION_{part} {value}" not in header:
            return fail(f"MetaVect.h version {part} drifted")
    d_module = (ROOT / "MetaVect.d").read_text()
    if 'return "0.1.0"' not in d_module:
        return fail("MetaVect.d version string drifted")

    sty = (ROOT / "extern/metavect.sty").read_text()
    dtx = (ROOT / "extern/LaTeX/metavect.dtx").read_text()
    tex = (ROOT / "extern/TeX/MetaVect.tex").read_text()
    for name, content in (("metavect.sty", sty), ("metavect.dtx", dtx)):
        if VERSION not in content or DATE not in content:
            return fail(f"{name} missing version/date")
    if VERSION not in tex:
        return fail("TeX/MetaVect.tex missing version")

    # Listings language must mirror the lexer keyword/type/boolean sets.
    for word in ("let", "fun", "fn", "forall", "if", "then", "else",
                 "match", "with", "module", "import", "export"):
        if word not in sty:
            return fail(f"listings language missing keyword: {word}")
    for name in ("Int", "Float", "Bool", "String", "Unit", "Path", "Color",
                 "Vector", "Matrix", "true", "false"):
        if name not in sty:
            return fail(f"listings language missing name: {name}")
    for comment in ("--", r"\{-"):
        if comment not in sty:
            return fail(f"listings language missing comment: {comment}")
    for macro in (r"\MetaVectVersion", r"\MetaVectInput",
                  r"\MetaVectInputOpt", "metavectcode"):
        if macro not in sty:
            return fail(f"metavect.sty missing macro: {macro}")

    # The .dtx package section must be the source of the shipped .sty.
    # docstrip copies guarded lines verbatim (a single `%` line would be
    # dropped as documentation, so in-code comments use `%%`), drops the
    # guard lines, and wraps the output in a generated header/footer.
    package = re.search(r"%<[*]package>(.*)%</package>", dtx, re.S)
    if not package:
        return fail("metavect.dtx has no package section")
    try:
        start = next(i for i, line in enumerate(sty.splitlines())
                     if line.startswith(r"\NeedsTeXFormat"))
        end = next(i for i, line in enumerate(sty.splitlines())
                   if line.startswith(r"\endinput"))
    except StopIteration:
        return fail("metavect.sty missing docstrip body markers")
    sty_lines = [line for line in sty.splitlines()[start:end] if line.strip()]
    dtx_lines = [line for line in package.group(1).splitlines()
                 if line.strip()]
    if sty_lines != dtx_lines:
        return fail("metavect.sty differs from the .dtx package section")

    latex = shutil.which("latex")
    if latex is None:
        print("tex checks passed (docstrip/pdflatex skipped: no latex)")
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        for name in ("metavect.dtx", "metavect.ins"):
            (tmpdir / name).write_bytes(
                (ROOT / "extern/LaTeX" / name).read_bytes())
        proc = subprocess.run(
            [latex, "-interaction=nonstopmode", "metavect.ins"],
            cwd=tmpdir, capture_output=True, text=True, timeout=120)
        generated = tmpdir / "metavect.sty"
        if proc.returncode != 0 or not generated.exists():
            print(proc.stdout[-2000:])
            return fail("docstrip extraction failed")
        if generated.read_bytes() != (ROOT / "extern/metavect.sty").read_bytes():
            return fail("docstrip output differs from shipped metavect.sty")

    pdflatex = shutil.which("pdflatex")
    if pdflatex is None:
        print("tex checks passed (pdflatex skipped: not installed)")
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        env_texinputs = (f"{ROOT / 'extern'}:{ROOT / 'extern/LaTeX'}:"
                         f"{ROOT / 'extern/TeX'}:")
        proc = subprocess.run(
            [pdflatex, "-interaction=nonstopmode", "-halt-on-error",
             str((ROOT / "extern/TeX/Build.tex").resolve())],
            cwd=tmpdir, capture_output=True, text=True, timeout=180,
            env={"TEXINPUTS": env_texinputs, "PATH": "/usr/bin:/bin"})
        if proc.returncode != 0:
            print(proc.stdout[-3000:])
            return fail("Build.tex did not compile")
        if not (tmpdir / "Build.pdf").exists():
            return fail("Build.pdf was not produced")

    print("tex checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
