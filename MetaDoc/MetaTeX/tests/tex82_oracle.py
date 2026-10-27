"""Compare native MetaTeX expansion/register results with TeXScrape INITEX.

The reference process is used only in tests; MetaTeX evaluates each source
independently. No format file, fonts, or installed TeX engine are required.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

native, reference = map(lambda value: Path(value).resolve(), sys.argv[1:3])
cases = [
    (r"\def\a{A}\edef\b{\a}\def\a{B}", r"\b\a", "AB"),
    (r"\def\a{A}{\def\a{B}\xdef\b{\a}}", r"\b\a", "BA"),
    (r"\def\a{A}\edef\b{\noexpand\a}\def\a{B}", r"\b", "B"),
    (r"\def\a{A}\def\take#1{[#1]}", r"\expandafter\take\a", "[A]"),
    (r"\expandafter\def\csname foo\endcsname{OK}", r"\foo", "OK"),
    (r"\def\name{foo}\def\foo{OK}", r"\csname\name\endcsname", "OK"),
    (r"\expandafter\def\csname foo bar\endcsname{A}\def\foobar{B}", r"\csname foo bar\endcsname\foobar", "AB"),
    (r"\def\f#1aba{#1}", r"\f aababa", "aba"),
    (r"\def\f#1;{#1}", r"\f {a;b};", "a;b"),
    (r"\def\f abc#1!{#1}", r"\f abcOK!", "OK"),
    ("", r"\number--42/\number'17/\number\"FF/\number`A".replace(r'\"', '"'), "42/15/255/65"),
    ("", r"\romannumeral1994/\romannumeral0/\romannumeral-1", "mcmxciv//"),
    (r"\count0=1 {\count0=2 {\global\count0=3 }}", r"\the\count0", "3"),
    (r"\countdef\n=10 \n=7 \advance\n by 5 \multiply\n by -3 \divide\n by 5 ", r"\the\n", "-7"),
    (r"\count0=5 \count1=-\count0 ", r"\the\count1", "-5"),
    (r"\count0=7 \edef\saved{\the\count0}\count0=9 ", r"\saved/\the\count0", "7/9"),
    (r"\count0=7 ", r"\ifnum\count0>6 OK\else BAD\fi", "OK"),
    (r"\catcode`\@=11 \def\foo@bar{OK}", r"\foo@bar", "OK"),
    (r"\catcode`\~=13 \def~{A}\def\~{B}", r"~\~", "AB"),
    (r"\def\literal{~}\catcode`\~=13 \def~{A}", r"\literal~", "~A"),
    (r"\catcode`\~=13 \def~{A}\def\saved{~}\catcode`\~=12 ", r"\saved~", "A~"),
    (r"{\catcode`\~=13 {\global\catcode`\~=12 }}", "~", "~"),
    (r"{\global\catcode`\~=13 \catcode`\~=12 }\def~{OK}", "~", "OK"),
    (r"\catcode`\~=13 \def~{A}\edef\saved{\noexpand~}\def~{B}", r"\saved", "B"),
    (r"\catcode`\@=11 ", r"\number\catcode`\@", "11"),
    (r"\catcode94=7 ", r"^^41^^42^^43", "ABC"),
    (r"\catcode94=7 \def\fo^^6f^^7bOK^^7d", r"\foo", "OK"),
    (r"\catcode94=7 ", r"^^5e^61", "a"),
    (r"\catcode33=7 ", r"!!41", "A"),
    (r"\def\a{A}{\def\a{B}\aftergroup\def\aftergroup\b\aftergroup{\aftergroup X\aftergroup}}", r"\a\b", "AX"),
    (r"\toks0={OK}", r"\the\toks0", "OK"),
    (r"\toks0={A}\toks1=\toks0\toks0={B}", r"\the\toks1\the\toks0", "AB"),
    (r"\toks0={A}{\toks0={B}{\global\toks0={C}}}", r"\the\toks0", "C"),
    (r"\toksdef\words=0 \words={OK}", r"\the\words", "OK"),
    (r"\def\a{old}\toks0={\a}\edef\b{\the\toks0}\def\a{new}", r"\b", "new"),
    (r"\def\a{old}\def\empty{}\toks0={\a}\edef\b{\expandafter\empty\the\toks0}\def\a{new}", r"\b", "old"),
    (r"\toks0={\def\a#1{#1}}\edef\b{\the\toks0}\b", r"\a{OK}", "OK"),
    (r"\toks0={A}\toks0=\expandafter{\the\toks0 B}", r"\the\toks0", "AB"),
    (r"\toksdef\words=0 \let\saved=\words\def\words{BAD}\saved={OK}", r"\the\saved", "OK"),
]

with tempfile.TemporaryDirectory(prefix="metatex-oracle-") as directory:
    root = Path(directory)
    for index, (setup, expression, expected) in enumerate(cases):
        source = root / f"case-{index}.tex"
        output = root / f"case-{index}.txt"
        source.write_text(setup + expression, encoding="ascii")
        result = subprocess.run([str(native), "-f", "plain", "-T", "text", "-o", str(output), str(source)],
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result.stdout + result.stderr
        assert output.read_text().strip() == expected, (index, output.read_text(), expected)
        initialization = r"\nonstopmode\catcode123=1 \catcode125=2 \catcode35=6 "
        program = initialization + setup + r"\message{ORACLE:[" + expression + r"]}\end"
        result = subprocess.run([str(reference), program], cwd=root, input="",
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "ORACLE:[" + expected + "]" in result.stdout, (index, result.stdout, expected)
print(f"{len(cases)} native/reference comparisons passed")
