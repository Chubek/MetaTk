"""Validate archives/XML and execute independent PDF, PS and ditroff readers."""
import io
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

exe, root = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])

def run(*args):
    result = subprocess.run([str(x) for x in args], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=30)
    assert result.returncode == 0, (args, result.returncode, result.stderr.decode(errors="replace"))
    return result.stdout

with tempfile.TemporaryDirectory(prefix="metatex-devices-", dir=exe.parent) as directory:
    temp = pathlib.Path(directory)
    source = temp / "input.ltx"
    source.write_text(r"""\documentclass{article}
\title{Device test}\author{Author \& Co}
\begin{document}\maketitle\section{First}
Hello (world). $x^2+\frac{1}{2}$
\newpage\section{Second}After the break.
\end{document}
""")
    names = run(exe, "--list-devices").decode().splitlines()
    assert names == ["html", "pdf", "text", "json", "ps", "troff-ir", "epub"]
    outputs = {}
    for target in names:
        output = temp / ("result." + target)
        run(exe, "-f", "latex", "-T", target, "-o", output, source)
        outputs[target] = output.read_bytes()
    assert outputs["pdf"].startswith(b"%PDF-1.4")
    assert b"libHaru" in outputs["pdf"]
    assert outputs["ps"].startswith(b"%!PS-Adobe-3.0")
    assert b"%%Pages: 2" in outputs["ps"]
    assert b"p1\n" in outputs["troff-ir"] and b"p2\n" in outputs["troff-ir"]
    assert json.loads(outputs["json"])["title"] == "Device test"
    with zipfile.ZipFile(io.BytesIO(outputs["epub"])) as book:
        assert book.testzip() is None
        first = book.infolist()[0]
        assert first.filename == "mimetype" and first.compress_type == zipfile.ZIP_STORED
        assert first.extra == b"" and first.header_offset == 0
        assert book.read("mimetype") == b"application/epub+zip"
        for name in book.namelist()[1:]:
            ET.fromstring(book.read(name))
        opf = ET.fromstring(book.read("OEBPS/package.opf"))
        ns = {"p": "http://www.idpf.org/2007/opf", "h": "http://www.w3.org/1999/xhtml"}
        items = opf.findall("p:manifest/p:item", ns)
        assert any(i.get("properties") == "nav" for i in items)
        assert any("mathml" in i.get("properties", "") for i in items)
        assert b"Hello (world)." in book.read("OEBPS/content.xhtml")
        nav = ET.fromstring(book.read("OEBPS/nav.xhtml"))
        content = ET.fromstring(book.read("OEBPS/content.xhtml"))
        ids = {e.get("id") for e in content.iter() if e.get("id")}
        for link in nav.findall(".//h:a", ns):
            assert link.get("href").split("#")[1] in ids
    # Serialization is deterministic, including EPUB timestamps and PDF metadata.
    for target in ("epub", "pdf"):
        output = temp / "again"
        run(exe, "-T", target, "-o", output, source)
        assert output.read_bytes() == outputs[target]
    if shutil.which("pdftotext"):
        text = run("pdftotext", temp / "result.pdf", "-").decode()
        assert "Hello (world)." in text and "After the break." in text
        assert text.count("\f") == 2
    if shutil.which("gs"):
        run("gs", "-q", "-dBATCH", "-dNOPAUSE", "-sDEVICE=nullpage", temp / "result.ps")
        run("gs", "-q", "-dBATCH", "-dNOPAUSE", "-sDEVICE=nullpage", temp / "result.pdf")
    if shutil.which("grops"):
        ps = run("grops", temp / "result.troff-ir")
        assert ps.startswith(b"%!PS-Adobe")
        if shutil.which("gs"):
            (temp / "troff.ps").write_bytes(ps)
            run("gs", "-q", "-dBATCH", "-dNOPAUSE", "-sDEVICE=nullpage", temp / "troff.ps")
    # Execute every new example/template in all seven devices.
    files = [root / "examples" / name for name in
             ("FirstClassMathExam.tex", "FirstClassMathExam.ltx", "CFG.nltx")]
    files += [root / "lua" / name for name in ("lmtex.tex", "DecisionTree.tex", "WEB.ltx")]
    files += [root / "templates" / name for name in ("plain.tex", "article.ltx", "notebook.nltx")]
    for file in files:
        dialect = {".tex": "plain", ".ltx": "latex", ".nltx": "nekotex"}[file.suffix]
        for target in names:
            run(exe, "-f", dialect, "-T", target, "-o", temp / "example", file)
    # Vector paths survive in PostScript and SVG/EPUB, without rasterization.
    source.write_text(r"\metavect{metavect-scene 1 100 50; fill 1 0 0; rect 5 5 30 20;}{Vector}")
    run(exe, "-f", "plain", "--plugin", "metavect", "-T", "ps", "-o", temp / "vector.ps", source)
    if shutil.which("gs"):
        run("gs", "-q", "-dBATCH", "-dNOPAUSE", "-sDEVICE=nullpage", temp / "vector.ps")
    run(exe, "-f", "plain", "--plugin", "metavect", "-T", "epub", "-o", temp / "vector.epub", source)
    with zipfile.ZipFile(temp / "vector.epub") as book:
        ET.fromstring(book.read("OEBPS/content.xhtml"))
        assert b'properties="svg"' in book.read("OEBPS/package.opf")
        assert b"<path " in book.read("OEBPS/content.xhtml")
print("Alternate devices, libHaru PDF, examples and templates passed")
