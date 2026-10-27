"""Check native glue registers against the independent TeX82 implementation."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

native, reference = (Path(path).resolve() for path in sys.argv[1:3])
sources = []
for order in ("pt", "fil", "fill", "filll"):
    sources.extend([
        fr"\skip0=1.25pt plus 2{order} minus .5{order}\relax",
        fr"\skip0=-1.25pt plus -2{order} minus .5{order}\relax",
        fr"\skip0=1pt plus 2{order}\relax\multiply\skip0 by -3 \divide\skip0 by 2 ",
        fr"\skip0=1pt plus 2{order}\relax\advance\skip0 by -\skip0\relax",
    ])
for left in ("pt", "fil", "fill", "filll"):
    for right in ("pt", "fil", "fill", "filll"):
        sources.append(fr"\skip0=1pt plus 2{left} minus 3{right}\relax\advance\skip0 by 2pt plus 4{right} minus 1{left}\relax")
sources.extend([
    r"\skip1=1pt plus 2fil minus 3pt\relax\skip0=-\skip1\relax",
    r"\dimen0=2pt \skip0=\dimen0 plus .5fil\relax",
    r"\count0=-2 \skip0=\count0 pt plus 1fil\relax",
    r"\skipdef\gap=0 \gap=1pt plus 2pt\relax{\global\advance\gap by 2pt plus 1fil\relax}",
    r"\skip0=1pt plus 0filll\relax\advance\skip0 by 0pt plus 1fil\relax",
    r"\skip0=1pt plus 1fil\relax\advance\skip0 by 0pt plus 0filll\relax",
])
with tempfile.TemporaryDirectory(prefix="metatex-glue-") as directory:
    root = Path(directory)
    for setup in sources:
        source, output = root / "case.tex", root / "case.txt"
        source.write_text(setup + r"\the\skip0", encoding="ascii")
        result = subprocess.run([str(native), "-f", "plain", "-T", "text", "-o", str(output), str(source)],
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, (setup, result.stdout, result.stderr)
        actual = output.read_text().strip()
        initialization = r"\nonstopmode\catcode123=1 \catcode125=2 \catcode35=6 "
        program = initialization + setup + r"\message{GLUE:[\the\skip0]}\end"
        result = subprocess.run([str(reference), program], cwd=root, input="",
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, (setup, result.stdout, result.stderr)
        matches = re.findall(r"GLUE:\[([^]]*)\]", result.stdout.replace("\n", ""))
        assert matches and actual == matches[-1], (setup, actual, matches, result.stdout)
print(f"{len(sources)} glue reference comparisons passed")
