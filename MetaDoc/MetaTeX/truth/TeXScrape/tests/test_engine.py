#!/usr/bin/env python3
"""Native TeX82 smoke/regression suite using TeXScrape's actual DVI bytes.

No third-party Python modules. Requires kpsewhich/Computer Modern .tfm fonts to
exercise PlainTeX; skips only font-dependent cases when fonts aren't installed.
"""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

engine = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
inspector = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else None


def run(args, cwd, env=None, timeout=30):
    result = subprocess.run([str(engine), *args], cwd=cwd, env=env,
                            input=b'', stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=timeout)
    return result.returncode, result.stdout.decode('latin1', 'replace')


def dvi_scan(path):
    """Walk all defined DVI opcodes, validate BOP/EOP, stack and postamble."""
    data = path.read_bytes()
    assert data.startswith(b'\xf7\x02'), 'missing DVI preamble'
    p, pages, depth, max_depth, last_bop, post_p = 0, 0, 0, 0, -1, -1
    text = bytearray()
    counts = []

    def take(n):
        nonlocal p
        if n < 0 or p + n > len(data):
            raise AssertionError(f'truncated DVI at byte {p}, count={n}')
        result = data[p:p+n]
        p += n
        return result

    def integer(n):
        return int.from_bytes(take(n), 'big')

    while p < len(data):
        op_start = p
        op = integer(1)
        if op <= 127:
            text.append(op)
        elif 128 <= op <= 131:   # set1..set4
            char = integer(op - 127)
            if char <= 127:
                text.append(char)
        elif op in (132, 137):
            take(8)
        elif 133 <= op <= 136:   # put1..put4
            char = integer(op - 132)
            if char <= 127:
                text.append(char)
        elif op == 138:         # nop
            pass
        elif op == 139:         # bop: ten counters and previous bop
            assert depth == 0
            counts.append([int.from_bytes(take(4), 'big', signed=True) for _ in range(10)])
            previous = int.from_bytes(take(4), 'big', signed=True)
            assert previous == last_bop, (previous, last_bop)
            last_bop = op_start
            pages += 1
        elif op == 140:         # eop
            assert depth == 0
        elif op == 141:
            depth += 1
            max_depth = max(max_depth, depth)
        elif op == 142:
            depth -= 1
            assert depth >= 0
        elif 143 <= op <= 146:
            take(op - 142)
        elif op in (147, 152, 161, 166):
            pass
        elif 148 <= op <= 151:
            take(op - 147)
        elif 153 <= op <= 156:
            take(op - 152)
        elif 157 <= op <= 160:
            take(op - 156)
        elif 162 <= op <= 165:
            take(op - 161)
        elif 167 <= op <= 170:
            take(op - 166)
        elif 171 <= op <= 234:
            pass
        elif 235 <= op <= 238:
            take(op - 234)
        elif 239 <= op <= 242:
            length = integer(op - 238)
            take(length)
        elif 243 <= op <= 246:
            take(op - 242)      # font number
            take(12)           # checksum, scale, design size
            area, name = integer(1), integer(1)
            take(area + name)
        elif op == 247:
            assert integer(1) == 2
            take(12)
            take(integer(1))
        elif op == 248:
            post_p = op_start
            assert integer(4) == last_bop
            take(12 + 8)
            stack_declared = integer(2)
            pages_declared = integer(2)
            assert pages_declared == pages
            assert max_depth <= stack_declared
        elif op == 249:
            assert integer(4) == post_p
            assert integer(1) == 2
            padding = data[p:]
            assert len(padding) >= 4 and set(padding) == {223}
            break
        else:
            raise AssertionError(f'unsupported DVI opcode {op} at byte {op_start}')
    else:
        raise AssertionError('no post_post in DVI')
    assert depth == 0
    assert post_p >= 0
    return pages, bytes(text).decode('ascii', 'replace'), counts


def main():
    with tempfile.TemporaryDirectory(prefix='texscrape-test-') as td:
        work = Path(td)
        status, result = run([r'\relax\end'], work)
        assert status == 0 and 'No pages of output' in result, result
        print('PASS: INITEX starts and executes TeX primitives')

        kpse = shutil.which('kpsewhich')
        cm = subprocess.run([kpse, 'cmr10.tfm'], capture_output=True, text=True).stdout.strip() if kpse else ''
        man = subprocess.run([kpse, 'manfnt.tfm'], capture_output=True, text=True).stdout.strip() if kpse else ''
        hyphen = subprocess.run([kpse, 'hyphen.tex'], capture_output=True, text=True).stdout.strip() if kpse else ''
        if not all((cm, man, hyphen)):
            print('SKIP: font-dependent PlainTeX tests (need cmr10, manfnt, hyphen.tex via kpsewhich)')
            return

        env = os.environ.copy()
        env['TEXINPUTS'] = os.pathsep.join([str(root/'PlainTeX'), str(Path(hyphen).parent), str(work)])
        env['TEXFONTS'] = os.pathsep.join([str(Path(cm).parent), str(Path(man).parent)])
        env['TEXFORMATS'] = str(work)
        status, result = run([r'\input PlainTeX \dump'], work, env, timeout=45)
        assert status == 0 and (work/'PlainTeX.fmt').stat().st_size > 10000, result
        print('PASS: PlainTeX format generated using bundled source')

        (work/'hello.tex').write_text(r'''
\nonstopmode
\hsize=3in \vsize=4in
\def\who#1{Hello, #1!}
\count42=11 \advance\count42 by 3
\ifnum\count42=14 \who{TeXScrape} \else WRONG \fi
\par
The integer is \the\count42.
This long paragraph contains line breaking, spaces, ligatures, and ordinary TeX hyphenation with words like internationalization and compatibility.
\par
$$a^2+b^2=c^2$$
\bye
''', encoding='ascii')
        status, result = run(['&PlainTeX', 'hello.tex'], work, env)
        assert status == 0, result
        assert 'Output written on hello.dvi' in result, result
        pages, chars, _ = dvi_scan(work/'hello.dvi')
        assert pages >= 1 and 'Hello' in chars and 'TeXScrape' in chars, (pages, chars)
        assert 'WRONG' not in chars
        if inspector:
            inspect = subprocess.run([str(inspector), '--json', str(work/'hello.dvi')],
                                     capture_output=True, text=True)
            assert inspect.returncode == 0, inspect.stderr
            overview = json.loads(inspect.stdout)
            assert overview['pages'] == pages and any('cmr10' in f['name'] for f in overview['fonts'])
            assert '1776.' not in overview['comment'], 'TeX date was never copied to eqtb'
            corrupted = work/'corrupt.dvi'
            corrupted.write_bytes((work/'hello.dvi').read_bytes()[:-4])
            bad = subprocess.run([str(inspector), str(corrupted)], capture_output=True, text=True)
            assert bad.returncode != 0 and 'DVI' in bad.stderr
            print('PASS: native C++ DVI parser accepts complete files and rejects truncation')
        print(f'PASS: macro, registers, conditionals, math, line breaking, DVI ({pages} page)')

        (work/'include.tex').write_text(r'\def\part{INCLUDED} \part\par', encoding='ascii')
        (work/'multi.tex').write_text(r'''\nonstopmode
\input include
\vfill\eject
\noindent Second page.\par\bye
''', encoding='ascii')
        status, result = run(['&PlainTeX', 'multi.tex'], work, env)
        assert status == 0, result
        pages, chars, _ = dvi_scan(work/'multi.dvi')
        assert pages == 2 and 'INCLUDED' in chars and 'Second' in chars, (pages, chars, result)
        print('PASS: file inclusion, explicit page break, two-page DVI')

        (work/'boxes.tex').write_text(r'''\nonstopmode
\setbox0=\hbox{Inside box}
\ifdim\wd0>0pt \box0 \else WRONG \fi
\par\bye
''', encoding='ascii')
        status, result = run(['&PlainTeX', 'boxes.tex'], work, env)
        assert status == 0, result
        pages, chars, _ = dvi_scan(work/'boxes.dvi')
        assert pages == 1 and 'Inside' in chars and 'WRONG' not in chars, (pages, chars, result)
        print('PASS: box registers, dimension comparison and shipout')

        # The original TeX82 40-byte Pascal file-name buffer silently truncated
        # absolute paths. The native port must handle modern filesystem paths.
        long_dir = work/'source-in-a-deep-subdirectory-with-a-lengthy-name'
        long_dir.mkdir()
        long_file = long_dir/'long-named-document-with-TeX82.tex'
        long_file.write_text(r'\noindent Long filename works.\par\bye', encoding='ascii')
        status, result = run(['&PlainTeX', str(long_file)], work, env)
        assert status == 0 and long_file.with_suffix('.dvi').exists(), result
        pages, chars, _ = dvi_scan(long_file.with_suffix('.dvi'))
        assert pages == 1 and 'Long' in chars
        print('PASS: long absolute source paths are preserved (not truncated at 40 bytes)')

        for example, expected_pages in [('hello', 2), ('registers', 1), ('boxes', 1), ('math', 1)]:
            filename = work/f'example-{example}.tex'
            filename.write_bytes((root/'examples'/f'{example}.tex').read_bytes())
            status, result = run(['&PlainTeX', filename.name], work, env)
            assert status == 0, f'{example}: {result}'
            actual_pages, _chars, _ = dvi_scan(filename.with_suffix('.dvi'))
            assert actual_pages == expected_pages, (example, actual_pages, result)
        print('PASS: example documents covering macro, math, boxes, pages, registers')

        (work/'error.tex').write_text(r'\nonstopmode \undefinedTeXScrapePrimitive \bye', encoding='ascii')
        status, result = run(['&PlainTeX', 'error.tex'], work, env)
        assert status != 0 and 'Undefined control sequence' in result, (status, result)
        print('PASS: erroneous inputs report failure')


if __name__ == '__main__':
    main()
