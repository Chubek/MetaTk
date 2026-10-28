"""Compile/run complete chapter programs and documented snapshot probes."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build' / 'verification'

def command(args, cwd=ROOT, timeout=120):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True, timeout=timeout)
    return result.returncode, result.stdout, result.stderr

def compile_run_uncached(item):
    name, source, compiler, extra, expected_failure = item
    directory = BUILD / name
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / 'main.cpp'
    path.write_text(source)
    args = shlex.split(compiler) + ['-std=c++20', '-O0', '-Wall', '-Wextra',
                                  '-I' + str(ROOT.parent / 'DSLtk'), str(path),
                                  '-o', str(directory / 'program')] + extra
    try:
        status, out, err = command(args)
        (directory / 'compile.log').write_text(shlex.join(args) + '\n' + out + err)
        if expected_failure:
            return {'name': name, 'passed': status != 0, 'kind': 'expected compile rejection',
                    'status': status}
        if status:
            return {'name': name, 'passed': False, 'kind': 'compile', 'status': status}
        status, out, err = command([str(directory / 'program')], cwd=directory, timeout=20)
        (directory / 'stdout.txt').write_text(out)
        (directory / 'stderr.txt').write_text(err)
        return {'name': name, 'passed': status == 0, 'kind': 'compile and run',
                'status': status, 'stdout': out}
    except (subprocess.TimeoutExpired, OSError) as error:
        return {'name': name, 'passed': False, 'kind': str(error)}

def compile_run(item):
    name, source, compiler, extra, expected_failure = item
    fingerprint = hashlib.sha256((source + compiler + repr(extra) + HEADER_HASH +
                                 COMPILER_VERSION).encode()).hexdigest()
    record = BUILD / name / 'result.json'
    if record.exists():
        old = json.loads(record.read_text())
        if old.get('fingerprint') == fingerprint and old.get('passed'):
            return old
    result = compile_run_uncached(item)
    result['fingerprint'] = fingerprint
    record.write_text(json.dumps(result, indent=2) + '\n')
    print(('PASS ' if result['passed'] else 'FAIL ') + name, flush=True)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default=os.environ.get('CXX', 'g++'))
    parser.add_argument('--probes', action='store_true')
    parser.add_argument('--listings', action='store_true')
    parser.add_argument('--jobs', type=int, default=3)
    args = parser.parse_args()
    global HEADER_HASH, COMPILER_VERSION
    HEADER_HASH = hashlib.sha256((ROOT.parent / 'DSLtk/DSLtk.hpp').read_bytes()).hexdigest()
    COMPILER_VERSION = command(shlex.split(args.compiler) + ['--version'])[1]
    BUILD.mkdir(parents=True, exist_ok=True)
    jobs, inventory = [], []
    if args.probes:
        text = (ROOT / 'projects/header-probes.cpp').read_text()
        for name in ['PREDICATE', 'RESULT', 'TASK', 'PATTERN']:
            jobs.append(('probe-' + name.lower(), text, args.compiler,
                         ['-DPROBE_' + name], name != 'PATTERN'))
    if args.listings:
        for chapter in sorted((ROOT / 'chapters').glob('ch*.ltx')):
            for index, match in enumerate(re.finditer(
                    r'\\begin\{lstlisting\}(\[.*?\])\s*\n(.*?)\\end\{lstlisting\}',
                    chapter.read_text(), re.S), 1):
                options, source = match.groups()
                if 'style=dslcpp' not in options:
                    continue
                name = chapter.stem.split('-')[0] + f'-listing-{index:02d}'
                complete = '#include' in source and re.search(r'(?m)^int\s+main\s*\(', source)
                inventory.append({'name': name, 'chapter': chapter.name,
                                  'complete': bool(complete), 'caption': options})
                if not complete:
                    continue
                extra = []
                if 'DSLTK_ENABLE_LUA' in source:
                    status, out, _ = command(['pkg-config', '--cflags', '--libs', 'lua5.4'])
                    if status:
                        raise SystemExit('Lua example requires pkg-config and lua5.4 development files')
                    extra = shlex.split(out)
                    if '#define DSLTK_ENABLE_LUA' not in source:
                        extra.append('-DDSLTK_ENABLE_LUA')
                jobs.append((name, source, args.compiler, extra, False))
        (BUILD / 'listing-inventory.json').write_text(json.dumps(inventory, indent=2) + '\n')
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(compile_run, jobs))
    kind = 'listings' if args.listings else 'probes'
    (BUILD / (kind + '.json')).write_text(json.dumps(results, indent=2) + '\n')
    failures = [item for item in results if not item['passed']]
    for item in failures:
        print('FAIL', item['name'], item['kind'], '— see build/verification/' + item['name'])
    print(f'{len(results) - len(failures)}/{len(results)} {kind} passed')
    raise SystemExit(bool(failures))

if __name__ == '__main__':
    main()
