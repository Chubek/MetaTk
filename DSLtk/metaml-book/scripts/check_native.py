#!/usr/bin/env python3
"""Compile and run standalone native book listings; fragments are explanatory."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
from check_book import ROOT, REPO, LISTING


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='c++')
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--d', default=shutil.which('ldc2'))
    parser.add_argument('--c-library', type=Path, default=REPO / 'build/libmetaml-c.a')
    args = parser.parse_args()
    count = 0
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='native-', dir=ROOT / 'build') as tmp:
        for n in range(1, 43):
            for i, (language, source, caption) in enumerate(
                    LISTING.findall((ROOT / f'chapters/ch-{n}.me').read_text())):
                if language not in {'cpp', 'c', 'd'}:
                    continue
                if language == 'd' and not args.d:
                    print('SKIP D listing: no LDC compiler selected')
                    continue
                path = Path(tmp) / f'ch_{n}_{i}.{language}'
                path.write_text(source)
                binary = path.with_suffix('.bin')
                if language == 'cpp':
                    commands = [[args.cxx, '-std=c++20', '-O0', '-pthread', '-I', str(REPO),
                                 str(path), '-o', str(binary)]]
                elif language == 'd':
                    commands = [[args.d, '-i', '-I' + str(REPO), str(path), '-of=' + str(binary)]]
                else:
                    obj = path.with_suffix('.o')
                    commands = [[args.cc, '-std=c99', '-I', str(REPO), '-c', str(path), '-o', str(obj)],
                                [args.cxx, str(obj), str(args.c_library.resolve()), '-pthread',
                                 '-o', str(binary)]]
                for command in [*commands, [str(binary)]]:
                    subprocess.run(command, cwd=REPO, check=True, timeout=180)
                count += 1
                print(f'PASS ch-{n}: {caption}', flush=True)
    print(f'Native listings: {count} passed')


if __name__ == '__main__':
    main()
