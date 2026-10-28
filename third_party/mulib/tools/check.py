#!/usr/bin/env python3
"""Compile and execute each self-contained MuLIB module example.

Run from any directory: python3 tools/check.py
Requires a C11 compiler on PATH; use CC environment variable to choose another.
"""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
compiler = os.environ.get('CC', 'cc')
examples = sorted((root / 'examples' / 'modules').glob('*.c'))
passed, failures = 0, []
with tempfile.TemporaryDirectory(prefix='mulib-tests-') as tmp:
    binary = str(Path(tmp) / 'test')
    for source in examples:
        command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                   '-I' + str(root / 'include'), str(source), '-lm', '-o', binary]
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode:
            failures.append((source.name, build.stderr))
            continue
        test = subprocess.run([binary], cwd=tmp, capture_output=True, text=True)
        if test.returncode:
            failures.append((source.name, test.stderr or test.stdout or 'nonzero exit'))
            continue
        passed += 1
print(f'MuLIB module examples: {passed}/{len(examples)} passed')
for name, reason in failures:
    print(f'FAILED {name}:\n{reason}', file=sys.stderr)
sys.exit(bool(failures))
