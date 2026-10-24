#!/usr/bin/env python3
"""Run C/C++ embedders, inspect ELF output independently, execute amd64 outputs."""
from pathlib import Path
import argparse
import os
import platform
import shutil
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--c', required=True)
p.add_argument('--cpp', required=True)
p.add_argument('--source', required=True)
p.add_argument('--work', type=Path, required=True)
a = p.parse_args()
if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
    print('SKIP: real embedding fixtures require a Linux x86-64 host')
    raise SystemExit(77)
if not all(shutil.which(tool) for tool in ('as', 'ar', 'readelf')):
    print('SKIP: embedding fixtures require as, ar and readelf')
    raise SystemExit(77)
a.work.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
if Path('/tmp/opencode').is_dir():
    env['TMPDIR'] = '/tmp/opencode'

def run(args, code=0):
    r = subprocess.run(list(map(str, args)), capture_output=True, text=True, env=env, timeout=60)
    assert r.returncode == code, (args, r.returncode, r.stdout, r.stderr)
    return r.stdout

(a.work / 'helper.s').write_text('.globl helper\n.text\nhelper: ret\n')
(a.work / 'value.inc').write_text('.byte VALUE\n')
run(['as', '--64', a.work / 'helper.s', '-o', a.work / 'helper.o'])
run(['ar', 'rcs', a.work / 'helper.a', a.work / 'helper.o'])
print(run([a.c, a.source, a.work]).strip())
print(run([a.cpp, a.source, a.work]).strip())
for name in ('c-linked64', 'c-linked32', 'cpp-linked', 'archive-linked'):
    file = a.work / name
    headers = run(['readelf', '-hW', file])
    assert 'EXEC' in headers and ('ELF32' if name.endswith('32') else 'ELF64') in headers
    segments = run(['readelf', '-lW', file])
    assert 'RWE' not in segments and 'RW E' not in segments
    symbols = run(['readelf', '-sW', file])
    assert '_start' in symbols
    if name != 'archive-linked':
        assert 'ABS' in symbols and ('exit_status' if name == 'cpp-linked' else 'exit_code') in symbols
    if name != 'c-linked32':
        run([file], 37)
print('PASS: same-process embedding, ELF32/64 metadata, W-X and amd64 execution')
