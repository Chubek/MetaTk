#!/usr/bin/env python3
"""DVI CLI checks plus optional independent TeX/DVItype interoperability."""
from pathlib import Path
import shutil
import re
import struct
import subprocess
import sys
import tempfile

runner = Path(sys.argv[1]).resolve()
def word(value):
    return struct.pack('>I', value & 0xffffffff)
def make_dvi(body, font=False):
    units = word(25400000) + word(473628672) + word(1000)
    definition = bytes([243, 0]) + word(0) + word(655360)*2 + bytes([0, 4]) + b'test'
    data = bytes([247, 2]) + units + bytes([0])
    if font:
        data += definition
    page = len(data)
    data += bytes([139]) + word(0)*10 + word(-1) + body + bytes([140])
    post = len(data)
    data += bytes([248]) + word(page) + units + word(0)*2 + bytes([0, 0, 0, 1])
    if font:
        data += definition
    return data + bytes([249]) + word(post) + bytes([2, 223, 223, 223, 223])

def run(*args, code=0, cwd=None):
    result = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True,
                            text=True, timeout=30)
    assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
    return result

with tempfile.TemporaryDirectory(prefix='metavm-dvi-') as temporary:
    tmp = Path(temporary)
    source = tmp/'rules.dvi'
    source.write_bytes(make_dvi(bytes([132])+word(10)+word(20)))
    direct = run(runner, source).stdout
    assert 'rule page=0 h=0 v=0 height=10 width=20' in direct
    assert 'eop page=0 h=20 v=0' in direct
    assert run(runner, '--threaded', source).stdout == direct
    run(runner, code=2)
    run(runner, '--unknown', source, code=2)
    run(runner, '--metrics', code=2)
    run(runner, tmp/'missing.dvi', code=3)
    source.write_bytes(b'bad')
    assert 'DVI byte' in run(runner, source, code=1).stderr
    source.write_bytes(make_dvi(bytes([171, 65]), font=True))
    assert 'missing metric' in run(runner, source, code=1).stderr
    metrics = tmp/'metrics.tsv'
    metrics.write_text('# FONT CHARACTER WIDTH\n0 65 123 # DVI units\n')
    trace = run(runner, '--metrics', metrics, source).stdout
    assert 'font=0 char=65 advance=123' in trace
    assert 'eop page=0 h=123 v=0' in trace
    metrics.write_text('0 65 2147483648\n')
    run(runner, '--metrics', metrics, source, code=1)
    metrics.write_text('0 65 1\n0 65 2\n')
    run(runner, '--metrics', metrics, source, code=1)
    run(runner, '--metrics', tmp/'missing.tsv', source, code=3)
    tex, dvitype = shutil.which('tex'), shutil.which('dvitype')
    if tex and dvitype:
        # Direct shipout: no plain-TeX page-number glyphs or external font metrics.
        (tmp/'independent.tex').write_text(
            '\\nopagenumbers\n\\shipout\\hbox{\\vrule height 10pt width 20pt depth 0pt}\\end\n')
        run(tex, '-interaction=nonstopmode', '-halt-on-error', 'independent.tex', cwd=tmp)
        independent = tmp/'independent.dvi'
        reference = run(dvitype, '-output-level=4', independent).stdout
        native = run(runner, independent).stdout
        assert 'setrule height 655360, width 1310720' in reference, reference
        assert 'height=655360 width=1310720' in native, native
        assert run(runner, '--threaded', independent).stdout == native
        print('Independent TeX-generated DVI and DVItype rule oracle passed')
        # Obtain actual scaled advances from an independent interpreter, then
        # compare native glyph positions and final h (not merely final counts).
        (tmp/'text.tex').write_text(
            '\\nopagenumbers\\font\\f=cmr10 \\shipout\\hbox{\\f AB}\\end\n')
        run(tex, '-interaction=nonstopmode', '-halt-on-error', 'text.tex', cwd=tmp)
        reference = run(dvitype, '-output-level=4', tmp/'text.dvi').stdout
        glyphs = re.findall(r'(?m)^(\d+): setchar(\d+) h:=(-?\d+)\+(-?\d+)=(-?\d+),', reference)
        fonts = re.findall(r'fntnum(\d+) current font', reference)
        assert len(glyphs) == 2 and len(fonts) == 1, reference
        metrics.write_text(''.join(f'{fonts[0]} {c} {w}\n' for _, c, _, w, _ in glyphs))
        trace = run(runner, '--metrics', metrics, tmp/'text.dvi').stdout
        for offset, character, before, width, _ in glyphs:
            line = next(line for line in trace.splitlines() if line.startswith(offset+' glyph '))
            assert f' h={before} ' in line and f'char={character} advance={width}' in line, line
        assert f'eop page=0 h={glyphs[-1][4]} ' in trace, trace
        assert run(runner, '--threaded', '--metrics', metrics, tmp/'text.dvi').stdout == trace
        print('Independent DVItype font advances and TeX glyph-position oracle passed')
    else:
        print('SKIP independent TeX/DVItype oracle: both executables required')
print('DVI CLI traces, metrics, errors and both Machinery engines passed')
