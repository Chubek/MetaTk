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
