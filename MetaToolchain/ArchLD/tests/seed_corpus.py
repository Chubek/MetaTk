#!/usr/bin/env python3
"""Generate real ELF32/64, COMDAT, PIC and GNU archive fuzz seeds."""
import pathlib
import subprocess
import sys
import tempfile

dest = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'build/corpus')
dest.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
    d = pathlib.Path(tmp)
    source = d / 'entry.s'
    source.write_text('''.global _start, data, bss
.text
_start: call helper
.data
data: .long 37
.bss
bss: .skip 64
.comm common,16,16
.section .text.helper,"axG",@progbits,helper,comdat
.global helper
helper: ret
''')
    for bits in (32, 64):
        subprocess.run(['as', '--' + str(bits), '-o', str(dest / f'elf{bits}.o'),
                        str(source)], check=True)
    (d / 'pic.c').write_text('extern int x; int f(void) {return x+1;}\n')
    subprocess.run(['cc', '-c', '-fPIC', '-o', str(dest / 'pic.o'), str(d / 'pic.c')], check=True)
    subprocess.run(['ar', 'rcs', str(dest / 'ordinary.a'), str(dest / 'elf64.o')], check=True)
print(f'Seeded {dest}')
