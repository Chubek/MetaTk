#!/usr/bin/env python3
"""Verify inventory, isolated headers, composition, examples, and focused tests.

Run from any directory. CC/CXX select compilers; --sanitizers adds ASan/UBSan.
No third-party Python packages are required. All binaries/data are temporary.
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

root = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--no-isolation', action='store_true',
                        help='skip per-header compilation, keeping combined checks')
    parser.add_argument('-j', '--jobs', type=int, default=min(4, os.cpu_count() or 1),
                        help='number of independent compiler/test jobs (default: up to 4)')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    if not compiler or not shutil.which(compiler[0]):
        parser.error('C compiler not found; set CC to an available compiler')
    have_cxx = bool(cxx and shutil.which(cxx[0]))
    flags = ['-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-UNDEBUG',
             '-I' + str(root / 'include')]
    if os.name == 'posix':
        flags += ['-pthread']
    if args.sanitizers:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    links = ([] if sys.platform == 'win32' else ['-lm'])
    if sys.platform.startswith(('linux', 'freebsd')):
        links += ['-ldl']
    headers = sorted((root / 'include').glob('mulib-*.h'))
    texts = {p.name: p.read_text() for p in headers}
    switches, dependencies, extra_switches, failures = {}, {}, {}, []
    reserved = set()
    snapshot = subprocess.run([sys.executable, str(root / 'tools/check_snapshot.py')],
                              capture_output=True, text=True)
    if snapshot.returncode:
        failures.append(('dsymspec snapshot', snapshot.stderr or snapshot.stdout))
    else:
        print(snapshot.stdout.strip())
    strata = subprocess.run([sys.executable, str(root / 'tools/stratify.py'), '--check'],
                            capture_output=True, text=True)
    if strata.returncode:
        failures.append(('dependency strata', strata.stderr or strata.stdout))
    else:
        print(strata.stdout.strip())
    tool_tests = subprocess.run([sys.executable, str(root / 'tests/test_tools.py')],
                                capture_output=True, text=True)
    if tool_tests.returncode:
        failures.append(('tool regressions', tool_tests.stderr or tool_tests.stdout))
    manifest = (root / 'MANIFEST.yaml').read_text()
    entries = {}
    for block in re.split(r'^- name: ', manifest.split('project_files:')[0], flags=re.M)[1:]:
        header = re.search(r'^  header: include/(mulib-[^\n]+\.h)$', block, re.M)
        if not header:
            failures.append(('MANIFEST.yaml', 'module entry has no header'))
            continue
        name = header.group(1)
        if name in entries:
            failures.append(('MANIFEST.yaml', 'duplicate header entry ' + name))
        entries[name] = block
    if set(entries) != set(texts):
        failures.append(('MANIFEST.yaml', 'module entries do not match the include inventory'))
    count = re.search(r'^module_count: (\d+)$', manifest, re.M)
    if not count or int(count.group(1)) != len(headers):
        failures.append(('MANIFEST.yaml', 'module_count does not match header inventory'))
    for header in headers:
        name, text = header.name, texts[header.name]
        match = re.search(r'^#\s*if(?:def\s+|\s+defined\s*\(\s*)'
                          r'(MULIB_IMPL_[A-Z0-9_]+)', text, re.M)
        block = entries.get(name, '')
        if re.search(r'^  status: reserved$', block, re.M):
            # Explicitly inventoried future modules are not implemented APIs.
            # They must remain empty/comment-only, and cannot be dependencies.
            remaining = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S).strip()
            if remaining:
                failures.append((name, 'reserved header contains code; implement and register it'))
            reserved.add(name)
            continue
        if not match:
            failures.append((name, 'missing implementation switch or empty header'))
            continue
        if not re.search(r'^  status: implemented$', block, re.M):
            failures.append((name, 'implemented header must have status: implemented'))
        switches[name] = match.group(1)
        dependencies[name] = re.findall(r'^\s*#\s*include\s*["<]'
                                        r'(mulib-[^">]+\.h)[">]', text, re.M)
        module = name[len('mulib-'):-len('.h')]
        for expected in ['manual/mulib-' + module + '.md',
                         'examples/modules/' + module + '.c']:
            if not (root / expected).is_file():
                failures.append((name, 'missing ' + expected))
        if name not in entries:
            failures.append((name, 'missing manifest entry'))
        else:
            block = entries[name]
            extras = re.search(r'^  extra_macros: \[([^\]]*)\]$', block, re.M)
            extra_switches[name] = re.findall(r'MULIB_IMPL_[A-Z0-9_]+', extras.group(1)) if extras else []
            macro = re.search(r'^  macro: (MULIB_IMPL_[A-Z0-9_]+)$', block, re.M)
            if not macro or macro.group(1) != switches[name]:
                failures.append((name, 'manifest implementation switch mismatch'))
            deps = re.search(r'^  dependencies: \[([^\]]*)\]$', block, re.M)
            actual = {d.strip() for d in deps.group(1).split(',') if d.strip()} if deps else None
            expected = {d[len('mulib-'):-len('.h')] for d in dependencies[name]}
            if actual != expected:
                failures.append((name, 'manifest dependency list differs from header includes'))
        for dependency in dependencies[name]:
            if dependency not in texts:
                failures.append((name, 'unknown MuLIB dependency ' + dependency))

    for name, deps in dependencies.items():
        for dependency in deps:
            if dependency in reserved:
                failures.append((name, 'depends on reserved header ' + dependency))
    implemented_count = re.search(r'^implemented_module_count: (\d+)$', manifest, re.M)
    if not implemented_count or int(implemented_count.group(1)) != len(switches):
        failures.append(('MANIFEST.yaml', 'implemented_module_count does not match implemented headers'))

    if len(set(switches.values())) != len(switches):
        failures.append(('headers', 'implementation switch shared by multiple modules'))
    implementation = (root / 'tests/implementation.c').read_text()
    declarations = (root / 'tests/declarations.c').read_text()
    for name in texts:
        for path, text in [('tests/implementation.c', implementation),
                           ('tests/declarations.c', declarations)]:
            if '#include "' + name + '"' not in text:
                failures.append((path, 'missing header ' + name))
    for name, switch in switches.items():
        if '#define ' + switch + '\n' not in implementation:
            failures.append(('tests/implementation.c', 'missing switch ' + switch))

    def closure(name, seen=None):
        seen = set() if seen is None else seen
        if name in seen:
            return seen
        seen.add(name)
        for dependency in dependencies.get(name, []):
            if dependency in texts:
                closure(dependency, seen)
        return seen

    temp_parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    counts = {}
    with tempfile.TemporaryDirectory(prefix='mulib-check-', dir=temp_parent) as tmp:
        tmp = Path(tmp)

        def check(label, sources=None, source_text=None, language='c', execute=True, extra_flags=None):
            with tempfile.TemporaryDirectory(prefix='case-', dir=tmp) as work:
                binary = Path(work) / 'test'
                command = (compiler if language == 'c' else cxx) + [
                    '-std=c11' if language == 'c' else '-std=c++17'] + flags
                command += extra_flags or []
                if source_text is not None:
                    command += ['-x', language, '-']
                else:
                    if language != 'c':
                        command += ['-x', language]
                    command += [str(p) for p in sources]
                command += links + ['-o', str(binary)]
                try:
                    build = subprocess.run(command, input=source_text, cwd=work,
                                           capture_output=True, text=True, timeout=90)
                    if build.returncode:
                        failures.append((label, build.stderr or build.stdout))
                        return False
                    if execute:
                        test = subprocess.run([str(binary)], cwd=work, capture_output=True,
                                              text=True, timeout=60)
                        if test.returncode:
                            failures.append((label, test.stderr or test.stdout or
                                             'exit status ' + str(test.returncode)))
                            return False
                except (OSError, subprocess.TimeoutExpired) as error:
                    failures.append((label, str(error)))
                    return False
                return True

        def check_many(cases):
            with ThreadPoolExecutor(max_workers=args.jobs) as executor:
                return sum(executor.map(lambda case: check(**case), cases))

        for category, sources in [
            ('module examples', sorted((root / 'examples/modules').glob('*.c'))),
            ('composition examples', sorted((root / 'examples').glob('*.c'))),
            ('focused tests', sorted((root / 'tests').glob('test_*.c'))),
        ]:
            passed = check_many([dict(label=str(p.relative_to(root)), sources=[p]) for p in sources])
            counts[category] = (passed, len(sources))
        integration = [
            ('smoke', [root / 'tests/smoke.c']),
            ('functional', [root / 'tests/implementation.c', root / 'tests/functional.c']),
            ('composition', [root / 'tests/implementation.c', root / 'tests/composition.c']),
            ('extensions', [root / 'tests/implementation.c', root / 'tests/extensions.c']),
            ('all implementations', [root / 'tests/all_headers.c']),
            ('C declarations', [root / 'tests/declarations.c']),
        ]
        cases = [dict(label=label, sources=paths) for label, paths in integration]
        all_switches = set(switches.values())
        for values in extra_switches.values():
            all_switches.update(values)
        defines = ''.join('#define ' + switch + '\n' for switch in sorted(all_switches))
        includes = ''.join('#include "' + header.name + '"\n' for header in reversed(headers))
        main_source = 'int main(void) { return 0; }\n'
        cases.append(dict(label='reverse-order implementations',
                          source_text=defines + includes + main_source))
        cases.append(dict(label='implementations after declarations',
                          source_text=includes + defines + includes + main_source))
        if os.name == 'posix':
            cases.append(dict(label='POSIX shell', sources=[root / 'tests/test_runtime.c'],
                              extra_flags=['-DMULIB_MINISH_ENABLE_POSIX']))
        if have_cxx:
            cases.append(dict(label='C++ declarations', sources=[root / 'tests/declarations.c'],
                              language='c++'))
        else:
            print('C++ declaration check skipped: no C++ compiler')
        counts['combined builds'] = (check_many(cases), len(cases))
        if not args.no_isolation:
            cases = []
            for header in headers:
                name = header.name
                declaration = ('#include "' + name + '"\n') * 2 + 'int main(void) { return 0; }\n'
                cases.append(dict(label='isolated declarations: ' + name, source_text=declaration))
                if have_cxx:
                    cases.append(dict(label='isolated C++ declarations: ' + name,
                                      source_text=declaration, language='c++'))
                if name in reserved:
                    continue
                enabled = set()
                for dependency in closure(name):
                    if dependency in switches:
                        enabled.add(switches[dependency])
                        enabled.update(extra_switches.get(dependency, []))
                defines = ''.join('#define ' + switch + '\n' for switch in sorted(enabled))
                cases.append(dict(label='isolated implementation: ' + name,
                                  source_text=defines + declaration))
            counts['isolated headers'] = (check_many(cases), len(cases))
    for category, (passed, total) in counts.items():
        print(f'MuLIB {category}: {passed}/{total} passed')
    print(f'MuLIB inventory: {len(switches)} implemented, {len(reserved)} reserved headers')
    for name, reason in sorted(failures):
        print(f'FAILED {name}:\n{reason}', file=sys.stderr)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
