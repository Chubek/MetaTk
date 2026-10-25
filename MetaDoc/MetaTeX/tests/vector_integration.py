#!/usr/bin/env python3
"""Exercise loaded plugin, exact IR/SVG geometry, PDF vectors, and CLI failures."""
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zlib

binary = pathlib.Path(sys.argv[1]).resolve()
scene = 'metavect-scene 1 200 100; stroke none; fill 1 0 0; rect 10 20 30 40;'


def streams(pdf):
    result = []
    for match in re.finditer(rb'<<(.*?)>>\s*stream\r?\n(.*?)\r?\nendstream', pdf, re.S):
        data = match[2]
        if b'/FlateDecode' in match[1]:
            data = zlib.decompress(data)
        result.append(data)
    return b'\n'.join(result)


with tempfile.TemporaryDirectory() as td:
    root = pathlib.Path(td)
    sub = root / 'sub'
    sub.mkdir()
    (sub / 'figure.mvs').write_text(scene)
    (sub / 'child.tex').write_text(r'\metavectfile[width=100]{figure.mvs}{Red square}')
    source = root / 'main.tex'
    source.write_text(r'\metavect[width=100]{' + scene + r'}{Red square}\input{sub/child}')
    env = dict(os.environ, PATH=str(binary.parent) + os.pathsep + os.environ.get('PATH', ''))
    results = {}
    for target in ('json', 'html', 'pdf', 'text'):
        output = root / ('out.' + target)
        # Invoke by basename through PATH, from a directory without plugins.
        proc = subprocess.run([binary.name, '-f', 'plain', '--plugin=metavect', '--plugin', 'metavect',
                               '-T', target, '-o', output, source], cwd=root, env=env,
                              capture_output=True, timeout=15)
        assert proc.returncode == 0, proc.stderr.decode()
        results[target] = output.read_bytes()
    doc = json.loads(results['json'])
    assert doc['diagnostics'] == []
    assert len(doc['blocks']) == 2
    assert doc['blocks'][0] == doc['blocks'][1]
    assert doc['blocks'][0]['graphic'] == {
        'format': 'metavect-scene-v1', 'width': 200, 'height': 100, 'displayWidth': 100,
        'paths': [{'stroke': None, 'fill': [1, 0, 0], 'strokeWidth': 1,
                   'commands': [['M', 10, 20], ['L', 40, 20], ['L', 40, 60], ['L', 10, 60], ['Z']]}]}
    html = results['html'].decode()
    svg = ET.fromstring(re.search(r'<svg.*?</svg>', html).group())
    assert svg.attrib['viewBox'] == '0 0 200 100'
    path = svg.find('{http://www.w3.org/2000/svg}path')
    assert path.attrib == {'fill': 'rgb(100%,0%,0%)', 'stroke': 'none', 'stroke-width': '1', 'stroke-miterlimit': '10',
                           'd': 'M 10 20 L 40 20 L 40 60 L 10 60 Z '}
    pdf = results['pdf']
    assert pdf.startswith(b'%PDF-1.4') and b'%%EOF' in pdf
    content = streams(pdf)
    assert re.search(rb'1(?:\.0+)? 0(?:\.0+)? 0(?:\.0+)? rg', content), content
    assert b' re' in content or b' m' in content
    assert re.search(rb'\bf\b', content)
    assert b'/Subtype /Image' not in pdf  # Geometry is retained, not rasterized.
    assert results['text'] == b'Red square\n\nRed square\n\n'
    if shutil.which('pdftoppm'):
        prefix = root / 'raster'
        subprocess.run(['pdftoppm', '-f', '1', '-singlefile', '-r', '72', root / 'out.pdf', prefix],
                       check=True, capture_output=True, timeout=15)
        ppm = prefix.with_suffix('.ppm').read_bytes()
        header = re.match(rb'P6\s+(\d+)\s+(\d+)\s+255\s', ppm)
        assert header
        width = int(header[1])
        pixels = ppm[header.end():]
        # Point (77, 80) is inside the first red rectangle for both PDF devices.
        at = (80 * width + 77) * 3
        assert pixels[at:at+3] == b'\xff\x00\x00'

    source.write_text(r'\documentclass{article}\usepackage{metavect}\begin{document}'
                      r'\metavect{' + scene + r'}{Caption}\end{document}')
    proc = subprocess.run([binary, '-f', 'latex', '--plugin', 'metavect', '-T', 'html',
                           '-o', root / 'latex.html', source], capture_output=True, timeout=15)
    assert proc.returncode == 0 and not proc.stderr, proc.stderr.decode()

    for bad in [r'\metavect{metavect-scene 1 -1 100;}{bad}',
                r'\metavect[width=20junk]{' + scene + r'}{bad}',
                r'\metavectfile{absent.mvs}{bad}', r'\metavectfile{source.mvct}{bad}',
                r'\undefined', r'\setcounter{x}{2147483647}\addtocounter{x}{1}',
                r'\setcounter{section}{2147483647}\section{Overflow}',
                r'\setcounter{x}{12junk}']:
        source.write_text(bad)
        output = root / 'preserved.html'
        output.write_text('previous output')
        proc = subprocess.run([binary, '-f', 'latex', '--plugin', 'metavect', '-T', 'html',
                               '-o', output, source], capture_output=True, timeout=15)
        assert proc.returncode == 1, proc.stderr.decode()
        assert output.read_text() == 'previous output'
    original = source.read_bytes()
    proc = subprocess.run([binary, '-f', 'plain', '-T', 'text', '-o', source, source], capture_output=True)
    assert proc.returncode == 2 and source.read_bytes() == original
    proc = subprocess.run([binary, '-f', 'plain', '--plugin', '../bad', source], capture_output=True)
    assert proc.returncode == 2 and b'invalid plugin name' in proc.stderr
print('Vector plugin integration passed')
