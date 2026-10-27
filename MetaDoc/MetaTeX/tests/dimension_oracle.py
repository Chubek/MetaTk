"""Compare fixed-point scans, arithmetic and formatting with TeXScrape INITEX."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

native, reference = (Path(path).resolve() for path in sys.argv[1:3])
sources = []
for unit in ("pt", "sp", "in", "pc", "cm", "mm", "bp", "dd", "cc"):
    for number in ("0", "1", "-1.25", ".00001", "1.99999999999999999", "12,34567"):
        sources.append(r"\dimen0=" + number + unit + " ")
for amount in (1, 2, 3, 32767, 32768, 32769, 65535, 65536, 65537, 1073741823, -1073741823):
    sources.append(fr"\dimen0={amount}sp ")
sources.extend([
    r"\dimen1=-3pt \dimen0=1.5\dimen1 ",
    r"\count0=-3 \dimen0=.5\count0 ",
    r"\count0=3 \dimen0=\count0 pt ",
    r"\dimen0=3sp \divide\dimen0 by -2 ",
    r"\dimen0=3pt \advance\dimen0 by -1.5pt \multiply\dimen0 by -3 ",
    r"\dimendef\size=0 \size=1pt {\size=2pt {\global\size=3pt }\size=4pt }",
    r"\dimen0='10pt ",
    r'\dimen0="FFsp ',
    r"\def\value{1.25}\def\unit{pt}\dimen0=\value\unit\relax",
])
expression = r"\number\dimen0/\the\dimen0"
with tempfile.TemporaryDirectory(prefix="metatex-dimensions-") as directory:
    root = Path(directory)
    for index, setup in enumerate(sources):
        source, output = root / "case.tex", root / "case.txt"
        source.write_text(setup + expression, encoding="ascii")
        result = subprocess.run([str(native), "-f", "plain", "-T", "text", "-o", str(output), str(source)],
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, (setup, result.stdout, result.stderr)
        actual = output.read_text().strip()
        initialization = r"\nonstopmode\catcode123=1 \catcode125=2 \catcode35=6 "
        program = initialization + setup + r"\message{DIMENSION:[" + expression + r"]}\end"
        result = subprocess.run([str(reference), program], cwd=root, input="",
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, (setup, result.stdout, result.stderr)
        matches = re.findall(r"DIMENSION:\[([^]]*)\]", result.stdout.replace("\n", ""))
        assert matches and actual == matches[-1], (setup, actual, matches, result.stdout)
print(f"{len(sources)} fixed-point reference comparisons passed")
