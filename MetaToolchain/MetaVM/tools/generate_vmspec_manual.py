#!/usr/bin/env python3
"""Generate an honest, exhaustive manual of the legacy VM entry points."""
import argparse
import difflib
import json
from pathlib import Path
import re
from legacy_profiles import ALIASES, REQUIREMENTS

ROOT = Path(__file__).resolve().parents[1]
NATIVE = {
    'DVI': ('DviMachine::run(bytes, metrics[, limits])', 'DVI.md'),
    'FORTH': ('metavm::machines::forth(source, fuel, stack_limit)', 'VMS_NATIVE.md'),
    'JVM': ('metavm::machines::jvm_integer_method(code, args, locals, stack_limit, fuel)', 'VMS_NATIVE.md'),
    'Krivine': ('KrivineMachine::verify / KrivineMachine::run', 'CLOSURE_VMS.md'),
    'SECD': ('SECDMachine::verify / SECDMachine::run', 'CLOSURE_VMS.md'),
}

def render():
    names = sorted(p.name for p in (ROOT/'vmspecs').iterdir() if p.is_dir())
    if set(names) != set(REQUIREMENTS):
        raise ValueError('Legacy manual identity drift: '+repr(set(names)^set(REQUIREMENTS)))
    profiles = {s['name']: s for s in json.loads((ROOT/'MANIFEST.yaml').read_text())['machines']}
    out = ['# Manual for every legacy VM entry\n',
           'This manual covers all '+str(len(names))+' directories under `MetaVM/vmspecs` '
           '(there is no `vmspec` directory). It is generated from the catalog and '
           '`tools/legacy_profiles.py`. Run `python3 tools/generate_vmspec_manual.py --check` '
           'from MetaVM to detect drift.\n',
           'Five legacy identities have native execution APIs: DVI, FORTH, JVM, Krivine and SECD. '
           'The other entries remain unimplemented. Portable example factories are retained '
           'for compatibility and do not execute native guest programs. No full language/platform '
           'runtime is claimed.\n',
           '## Build and common usage\n',
           '```sh\ncmake -S MetaVM -B out-vm -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON\n'
           'cmake --build out-vm -j4\nctest --test-dir out-vm --output-on-failure -j4\n'
           'out-vm/dvi-run document.dvi\n'
           'out-vm/closure-vm-demo\n```\n',
           'CMake consumers link `MetaVM::Framework` (VMWeave plus catalog) or '
           '`MetaVM::Catalog` for native-only APIs. See [PACKAGING.md](PACKAGING.md) '
           'for installation. Legacy C headers work from C11 and C++; native APIs '
           'in this manual require C++17.\n',
           'For a mapped entry, `legacy_NAME_example(&module, &error)` returns an owned '
           'portable Machinery module on success. Destroy it with `mvm_module_destroy`. '
           'Its example returns 42. Run `.mvm` with `metavm-run`; build real MetaML '
           '`.vmweave` with the optional `vmweave-metaml` front end. `.vmwv` legacy '
           'companions are Machinery assembly, not native guest specifications. '
           'Unmapped factories return `MVM_NOT_FOUND`, clear the output and report a diagnostic. '
           'Always check the status before using a module.\n',
           'VMWeave Builder authors verified Machinery instructions. Definition IR lowers '
           'explicit fixed-width definitions with static targets; it cannot infer an ISA '
           'from a catalog name. DVI adds verified event replay through the host import '
           '`dvi_event`; the native decoder runs before replay. This preserves the '
           'difference between guest interpretation and authoring.\n',
           '## Coverage index\n',
           '| Legacy name | Catalog identity | Execution status |\n|---|---|---|\n']
    for name in names:
        target = ALIASES.get(name, name)
        profile = profiles.get(target)
        status = profile['native_coverage'] if profile else 'unmapped / unspecified'
        out.append(f'| {name} | {target if profile else "—"} | {status} |\n')
    for name in names:
        target = ALIASES.get(name, name)
        profile = profiles.get(target)
        symbol = re.sub('[^A-Za-z0-9_]', '_', name)
        out += [f'\n## {name}\n', f'Header: `vmspecs/{name}/{name}.h`. '
                f'Compatibility factory: `legacy_{symbol}_example`.\n']
        if name in NATIVE:
            api, manual = NATIVE[name]
            out.append(f'Native API: `{api}`. Read the [execution contract]({manual}) '
                       'for instructions, errors, ownership and resource limits.\n')
            out.append('Compatibility boundaries: '+' '.join(profile['limitations'])+'\n')
        elif profile:
            out.append(f'Catalog identity: `{target}`; status: **catalog-only**. '
                       f'`VMLibrary/{target}/{target}.vmweave` and `{target}.mvm` '
                       'are portable examples. There is no native execution API or guest loader.\n')
        else:
            out.append('Status: **unmapped / unspecified**. The compatibility factory '
                       'returns `MVM_NOT_FOUND`; the `.vmwv` companion has no guest definition. '
                       'No portable or native success is fabricated.\n')
        out.append('Compatibility requirement: '+REQUIREMENTS[name]+'\n')
    return '\n'.join(out)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    output = ROOT/'docs/VMS.md'
    expected = render()
    if args.check:
        actual = output.read_text() if output.exists() else ''
        if actual != expected:
            print(''.join(difflib.unified_diff(actual.splitlines(True), expected.splitlines(True),
                                            fromfile=str(output), tofile='generated VMS.md')))
            raise SystemExit(1)
        print('All 38 legacy VM identities, coverage and manual entries are in sync')
    else:
        output.write_text(expected)
