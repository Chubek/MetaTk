"""CLI integration and differential tests; uses only the Python standard library."""
import pathlib
import itertools
import random
import re
import subprocess
import sys
import tempfile

cli, compiler = sys.argv[1:]


def run(args, source=None, ok=True):
    result = subprocess.run(args, input=source, text=True, capture_output=True, timeout=15)
    if (result.returncode == 0) != ok:
        raise AssertionError(f"{args}: exit {result.returncode}: {result.stderr}")
    return result


TOKEN = re.compile(
    r'''(?:u8|[LuU])?(?:"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')'''
    r"|[A-Za-z_]\w*|(?:\d[\w.]*|\.\d[\w.]*)"
    r"|>>=|<<=|\.\.\.|##|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[*/%+&^|=-]=|\S"
)


def tokens(text):
    return TOKEN.findall(text)


cases = [
    "#define f(x) x x\n#define h(x) f(x)\nh(f(1))\n",
    "#define x y\n#define y x\nx y x\n",
    "#define f(x) x\n#define a f\na(a)(2)\n",
    "#define f(x) x\nf(f)(1)\n",
    "#define f(x) x\n#define g(x) f(x)\nf(g)(2)\n",
    "#define f(x) x\n#define g f\ng (g) (2)\n",
    "#define A() B\n#define B() A\nA()()()\n",
    "#define cat(a,b) a##b\n#define X 1\ncat(X,2) cat(,X)\n",
    "#define s(x) #x\ns(a/**/b) s(a+b) s( a\n b )\n",
    "#define f(...) #__VA_ARGS__\nf(a,b) f(a, b) f()\n",
    "#define f(a,b,c) a##b##c\nf(,,) f(x,,y) f(,x,)\n",
    "#define f(a,b,c) a b##c\nf(x,,y)\n",
    "#define X /* comment\n comment */ 42\nX\n",
    "#define X \\\n 10\nX __LINE__\n",
    "#if -2 < -1 && -1 > -2 && ~0 == -1\nyes\n#endif\n",
    "#if (1 ? -1 : 0U) > 0 && (-8 >> 2) == -2\nyes\n#endif\n",
    "#if 0 && (1 / 0)\nno\n#elif 1 || (1 << 100)\nyes\n#endif\n",
    "#if 'A' == 65 && 0xffUL == 255\nyes\n#endif\n",
    "#line 50 \"virtual.c\"\n__LINE__ __FILE__\n__LINE__\n",
    '#define S(x) #x\nS(%:) S(%:%:)\n',
    '#define S(...) #__VA_ARGS__\nS(a , b) S(a,b)\n',
    '#define E\n#define S_(x) #x\n#define S(x) S_(x)\nS(a E+b)\n',
    '#define F(x) #x\n#define G(x) F(a x+b)\nG()\n',
    '#define F(x) a x\n#define S_(x) #x\n#define S(x) S_(x)\nS(F()+b)\n',
    '#line 3 "a\\\\b.c"\n__FILE__\n',
    # Rescan example from the C standard.
    """#define x 3
#define f(a) f(x * (a))
#undef x
#define x 2
#define g f
#define z z[0]
#define h g(~
#define m(a) a(w)
#define w 0,1
#define t(a) a
f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);
g(x+(3,4)-w) | h 5) & m(f)^m(m);
""",
]

rng = random.Random(1729)


def expression(depth):
    if not depth:
        return str(rng.randrange(20)) + rng.choice(["", "U"])
    op = rng.choice(["+", "-", "*", "&", "|", "^", "<", ">=", "==", "!=", "&&", "||"])
    return f"({expression(depth - 1)} {op} {expression(depth - 1)})"


# Exercise combinations of signedness, precedence and boolean conversions in one
# deterministic translation unit, in addition to the hand-written edge cases.
cases.append("\n".join(
    f"#if {expression(3)}\ncase_{i}_true\n#else\ncase_{i}_false\n#endif"
    for i in range(128)
))

whitespace_cases = []
for empty, following, before, after in itertools.product(
    ["E", "I()", "F(,)"], ["a", "+", "I(b)"], ["", " "], ["", " "]
):
    text = "a" + before + empty + after + following
    whitespace_cases.extend([text, "I(" + text + ")", "F(a " + empty + "," + following + ")"])
cases.append(
    "#define E\n#define I(x) x\n#define F(x,y) x y\n#define S_(x) #x\n#define S(x) S_(x)\n"
    + "\n".join("S(" + text + ")" for text in whitespace_cases)
)

for index, source in enumerate(cases):
    actual = run([cli, "-"], source).stdout
    expected = run([compiler, "-E", "-P", "-undef", "-x", "c", "-"], source).stdout
    assert tokens(actual) == tokens(expected), (
        f"differential case {index}:\n{source}\nactual: {actual}\nexpected: {expected}"
    )

assert tokens(run([cli, "-DVALUE=4", "-U", "VALUE", "-D", "VALUE=9"], "VALUE").stdout) == ["9"]
assert "unknown option" in run([cli, "--bogus"], ok=False).stderr
assert "missing argument" in run([cli, "-I"], ok=False).stderr
assert "only one input" in run([cli, "one.c", "two.c"], ok=False).stderr
assert "MetaPP" in run([cli, "--version"]).stdout
assert "#error failure" in run([cli], "#error failure", ok=False).stderr
assert "warning: careful" in run([cli], "#warning careful").stderr
assert tokens(run([cli], "/*" + "long comment " * 100000 + "*/\n__LINE__").stdout) == ["2"]

with tempfile.TemporaryDirectory(prefix="metapp-cli-") as tmp:
    root = pathlib.Path(tmp)
    (root / "header.h").write_text("#pragma once\n#define VALUE 12\n")
    (root / "input.c").write_text('#include <header.h>\nint value = VALUE;\n')
    out = root / "output.c"
    run([cli, "-I", tmp, "-o", str(out), str(root / "input.c")])
    assert tokens(out.read_text()) == tokens("int value = 12;")
    run([compiler, "-x", "c", "-fsyntax-only", str(out)])
    (root / "HEADER.h").write_text("literal_header\n")
    (root / "two words.h").write_text("space_header\n")
    (root / "dir").mkdir()
    (root / "dir" / "nested.h").write_text("nested_header\n")
    include_source = "#define HEADER changed\n#include <HEADER.h>\n#include <two words.h>\n#include <dir//nested.h>\n"
    actual = run([cli, "-I", tmp], include_source).stdout
    expected = run([compiler, "-E", "-P", "-I", tmp, "-x", "c", "-"], include_source).stdout
    assert tokens(actual) == tokens(expected), (actual, expected)
    out.write_text("keep this output on failure")
    run([cli, "-o", str(out), str(root / "missing.c")], ok=False)
    assert out.read_text() == "keep this output on failure"

print(f"{len(cases)} differential cases and CLI integration checks passed")
