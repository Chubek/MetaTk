#!/usr/bin/env python3
"""End-to-end plugin composition, source-relative files, and error atomicity."""
import json
import pathlib
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="plugins-", dir=sys.argv[2]) as temporary:
    root = pathlib.Path(temporary)
    nested = root / "nested"
    nested.mkdir()
    (nested / "scene.mvs").write_text("metavect-scene 1 200 100; rect 0 0 200 100;", encoding="utf-8")
    (nested / "code.txt").write_text("  value = 100  # literal\n", encoding="utf-8")
    (nested / "body.tex").write_text(r"""
\newenvironment{message}[1][Result]{\textbf{#1}: }{ Done.}
\begin{message}Ready.\end{message}
\begin{warning}[Check]First.\par Second.\end{warning}
\codefile[python]{code.txt}
\metavectdeffile[height=25]{\chart}{scene.mvs}
\chart{Saved scene}
\begin{equation}x^2\label{eq}\end{equation}
Equation \eqref{eq}.
""", encoding="utf-8")
    source = root / "main.nltx"
    source.write_text(r"\begin{document}\input{nested/body}\end{document}", encoding="utf-8")
    outputs = {}
    for target in ("json", "html", "pdf", "text"):
        destination = root / ("result." + target)
        subprocess.run([binary, "--plugin", "metavect", "--plugin", "metavect", "-f", "nekotex", "-T", target,
                        "-o", destination, source], check=True, capture_output=True, timeout=15)
        outputs[target] = destination.read_bytes()
    document = json.loads(outputs["json"])
    assert document["diagnostics"] == []
    assert document["references"]["eq"] == "1"
    notes = [b for b in document["blocks"] if b["type"] == "note"]
    assert len(notes) == 2 and all(b["role"] == "warning" for b in notes)
    code = next(b for b in document["blocks"] if b["type"] == "code")
    assert code["language"] == "python" and code["spans"][0]["text"] == "  value = 100  # literal\n"
    graphic = next(b for b in document["blocks"] if b["type"] == "vector-graphic")
    assert graphic["graphic"]["displayWidth"] == 50 and len(graphic["graphic"]["paths"]) == 1
    assert b'data-kind="warning"' in outputs["html"] and b'data-language="python"' in outputs["html"]
    assert b'width:50pt' in outputs["html"] and b'<svg' in outputs["html"]
    assert b'(  value = 100  # literal) Tj' in outputs["pdf"]
    assert b'0.25000000 0 0 -0.25000000' in outputs["pdf"]
    assert b'Equation (1).' in outputs["text"]
    for malformed in (r"\begin{equation}x\end{align}", r"\newenvironment{x}[1]{#2}{}",
                      r"\codeblock{unfinished", r"\begin{verbatim}unfinished",
                      r"\metavect[width=1,width=2]{metavect-scene 1 10 10;}{}"):
        source.write_text(malformed, encoding="utf-8")
        destination = root / "preserved.html"
        destination.write_text("existing output", encoding="utf-8")
        result = subprocess.run([binary, "--plugin", "metavect", "-f", "nekotex", "-T", "html",
                                 "-o", destination, source], capture_output=True, timeout=15)
        assert result.returncode != 0 and destination.read_text() == "existing output", malformed
        assert b"error" in result.stderr.lower(), result.stderr
print("Plugin integration passed")
