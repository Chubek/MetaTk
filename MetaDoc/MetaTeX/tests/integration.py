#!/usr/bin/env python3
"""Smoke-test compiled plugin loading, semantic IR, MathML and page breaks."""
import json
import pathlib
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory() as td:
    d = pathlib.Path(td)
    input_path = d / 'full.ltx'
    input_path.write_text(r'''
\documentclass{article}
\newcommand{\who}[1]{Welcome, \textbf{#1}!}
\newenvironment{preface}{Before: }{After.}
\begin{document}
\section{Start}\label{start}
\who{developer}
\begin{preface}Body \verb|literal|.\end{preface}
\[\frac{x^2}{\sqrt{y}}=\alpha\]
\newpage
\section{End}
Section \ref{start}.
\end{document}
''', encoding='utf-8')
    html_path=d / 'document.html'
    pdf_path=d / 'document.pdf'
    json_path=d / 'document.json'
    for target,path in [('html',html_path),('pdf',pdf_path),('json',json_path)]:
        subprocess.run([binary, '--format=latex', '--target='+target, '-o',path,input_path],check=True,capture_output=True)
    h=html_path.read_text(encoding='utf-8')
    j=json.loads(json_path.read_text(encoding='utf-8'))
    pdf=pdf_path.read_bytes()
    assert '<mfrac>' in h and '<msqrt>' in h
    assert '<strong>developer</strong>' in h
    assert 'page-break' in [b['type'] for b in j['blocks']]
    assert j['references']['start']=='1'
    assert pdf.startswith(b'%PDF-1.4') and b'xref' in pdf
    neko_path=d / 'sample.nltx'
    neko_path.write_text('\\begin{document}\\callout[Info]{A note}\\end{document}',encoding='utf-8')
    out=d / 'neko.html'
    subprocess.run([binary, '--format=nekotex', '--target=html','-o',out,neko_path],check=True,capture_output=True)
    assert 'Info: A note' in out.read_text(encoding='utf-8')
print('integration tests passed')
