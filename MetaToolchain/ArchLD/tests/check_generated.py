#!/usr/bin/env python3
"""Tangle in isolation and compare both bundled outputs byte for byte."""
import pathlib
import shutil
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='archld-tangle-') as tmp:
    work = pathlib.Path(tmp)
    (work / 'src').mkdir()
    shutil.copyfile(root / 'src/ArchLD.w', work / 'src/ArchLD.w')
    subprocess.run(['ctangle', 'src/ArchLD.w', '-', 'src/ArchLD.c'], cwd=work, check=True)
    for name in ('ArchLD.c', 'ArchLD.h'):
        assert (root / 'src' / name).read_bytes() == (work / 'src' / name).read_bytes(), (
            name + ' is stale; run make tangle')
print('PASS: CWEB source and bundled C/header agree')
