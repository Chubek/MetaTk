#!/usr/bin/env python3
"""Compile VMLibrary's portable examples to verified modules and C factories.

This does not generate historical guest runtimes. Native APIs remain separate.
All compilation/emission finishes before any destination artifact is replaced.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def load_profiles(root):
    manifest = json.loads((root / 'MANIFEST.yaml').read_text(encoding='utf-8'))
    if manifest.get('schema') != 'metavm.catalog/1':
        raise ValueError('unsupported catalog schema')
    profiles = manifest['machines']
    if not profiles:
        raise ValueError('empty catalog')
    names, symbols = set(), set()
    for profile in profiles:
        name, symbol = profile['name'], profile['symbol']
        if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_-]*', name):
            raise ValueError('invalid machine name: ' + name)
        if symbol != re.sub('[^A-Za-z0-9_]', '_', name):
            raise ValueError('invalid machine symbol: ' + symbol)
        if name in names or symbol in symbols:
            raise ValueError('duplicate machine identity: ' + name)
        if profile['portable_example'] != name + '.mvm':
            raise ValueError('invalid example path: ' + name)
        names.add(name)
        symbols.add(symbol)
        source = root / 'VMLibrary' / name / profile['portable_example']
        if not source.is_file():
            raise ValueError('missing example source: ' + str(source))
    return profiles


def generate(vmweave, output, root=ROOT):
    if output.resolve() == root.resolve():
        raise ValueError('example output must not overwrite the source catalog')
    profiles = load_profiles(root)

    # A private staging directory also keeps failed compilations from damaging
    # preexisting output. Source paths and tool invocations never use a shell.
    with tempfile.TemporaryDirectory(prefix='metavm-examples-') as temporary:
        stage = Path(temporary)
        declarations, entries, artifacts = [], [], []
        for profile in profiles:
            name, symbol = profile['name'], profile['symbol']
            relative = Path('VMLibrary') / name
            directory = stage / relative
            directory.mkdir(parents=True)
            binary = directory / (name + '.mvmb')
            source = directory / (name + '.c')
            factory = 'generated_' + symbol + '_example_module'
            subprocess.run([str(vmweave), 'compile',
                            str(root / relative / profile['portable_example']),
                            str(binary)], check=True)
            subprocess.run([str(vmweave), 'emit-c', str(binary), str(source), factory], check=True)
            declarations.append('extern "C" mvm_status ' + factory + '(mvm_module**,mvm_error*);')
            entries.append('    {' + json.dumps(name) + ', &' + factory + '}')
            artifacts.extend([relative / binary.name, relative / source.name])
        header = '\n'.join([
            '#pragma once', '#include "MetaVM.h"', *declarations,
            'struct GeneratedExample { const char* name; mvm_status (*factory)(mvm_module**,mvm_error*); };',
            'inline const GeneratedExample generated_examples[] = {',
            ',\n'.join(entries), '};', '',
        ])
        (stage / 'catalog_factories.hpp').write_bytes(header.encode('utf-8'))
        artifacts.append(Path('catalog_factories.hpp'))
        for relative in artifacts:
            destination = output / relative
            data = (stage / relative).read_bytes()
            if destination.is_file() and destination.read_bytes() == data:
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            fd, pending = tempfile.mkstemp(prefix='.' + destination.name + '.', dir=destination.parent)
            try:
                with os.fdopen(fd, 'wb') as stream:
                    stream.write(data)
                os.chmod(pending, 0o644)
                os.replace(pending, destination)
            finally:
                if os.path.exists(pending):
                    os.unlink(pending)
    print('Generated', len(profiles), 'portable modules and C factories in', output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vmweave', type=Path, help='Built VMWeave executable')
    parser.add_argument('--output-root', type=Path)
    parser.add_argument('--list-inputs', action='store_true', help='Print manifest-listed assembly paths for build systems')
    args = parser.parse_args()
    if args.list_inputs:
        for profile in load_profiles(ROOT):
            print((ROOT / 'VMLibrary' / profile['name'] / profile['portable_example']).as_posix())
        return
    if args.vmweave is None or args.output_root is None:
        parser.error('--vmweave and --output-root are required for generation')
    generate(args.vmweave.resolve(), args.output_root.resolve())


if __name__ == '__main__':
    main()
