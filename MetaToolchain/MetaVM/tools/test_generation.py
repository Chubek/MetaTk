#!/usr/bin/env python3
"""Regeneration, failure preservation and generator ownership regressions."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from generate_examples import generate

ROOT = Path(__file__).resolve().parents[1]


def run(script, *args, success=True):
    result = subprocess.run([sys.executable, str(script), *map(str, args)],
                            capture_output=True, text=True)
    if (result.returncode == 0) != success:
        raise RuntimeError(result.stdout + result.stderr)
    return result


def snapshot(root):
    return {p.relative_to(root): (hashlib.sha256(p.read_bytes()).digest(), p.stat().st_mtime_ns)
            for p in root.rglob('*') if p.is_file()}


def main():
    script = ROOT / 'tools/generate_catalog.py'
    run(script, '--check')
    listed = run(ROOT / 'tools/generate_examples.py', '--list-inputs').stdout.splitlines()
    profiles = json.loads((ROOT / 'MANIFEST.yaml').read_text())['machines']
    assert listed == [(ROOT / 'VMLibrary' / p['name'] / p['portable_example']).as_posix() for p in profiles]
    with tempfile.TemporaryDirectory(prefix='metavm-generation-test-') as temporary:
        root = Path(temporary)
        output = root / 'catalog'
        native = output / 'VMLibrary/DVI/DviMachine.cpp'
        native.parent.mkdir(parents=True)
        native.write_text('native implementation must survive\n')
        (output / 'unrelated-empty-file').touch()
        run(script, '--output-root', output)
        before = snapshot(output)
        run(script, '--output-root', output, '--check')
        run(script, '--output-root', output)
        assert snapshot(output) == before, 'unchanged outputs were rewritten'
        stale = output / 'VMLibrary/JVM/JVM.c'
        expected = stale.read_bytes()
        stale.write_bytes(b'previous user artifact\n')
        changed = snapshot(output)
        result = run(script, '--output-root', output, '--check', success=False)
        assert 'VMLibrary/JVM/JVM.c' in result.stdout
        assert snapshot(output) == changed, '--check modified files'
        run(script, '--output-root', output)
        assert stale.read_bytes() == expected
        missing = output / 'VMLibrary/JVM/JVM.vmspc'
        missing.unlink()
        run(script, '--output-root', output, '--check', success=False)
        assert not missing.exists()
        run(script, '--output-root', output)
        assert native.read_text() == 'native implementation must survive\n'
        assert (output / 'unrelated-empty-file').stat().st_size == 0

        # A collision discovered after rendering the first profile must not
        # publish any partially generated catalog files.
        fixture = root / 'fixture'
        (fixture / 'tools').mkdir(parents=True)
        for name in ['generate_catalog.py', 'legacy_profiles.py']:
            shutil.copyfile(ROOT / 'tools' / name, fixture / 'tools' / name)
        (fixture / 'VMLibrary/A-B').mkdir(parents=True)
        (fixture / 'VMLibrary/A_B').mkdir()
        (fixture / 'vmspecs').mkdir()
        invalid = root / 'invalid-output'
        result = run(fixture / 'tools/generate_catalog.py', '--output-root', invalid, success=False)
        assert 'colliding machine symbol' in result.stderr
        assert not invalid.exists(), 'invalid catalog was partially published'

        # Use the real compiler: a bad second program must preserve an existing
        # first artifact and publish no header or generated C files.
        profiles = []
        for name in ['First', 'Second']:
            directory = fixture / 'VMLibrary' / name
            directory.mkdir()
            (directory / (name + '.mvm')).write_text('.function main 0 1\nconst r0 42\nret r0\n.end\n')
            profiles.append({'name': name, 'symbol': name, 'portable_example': name + '.mvm'})
        manifest = {'schema': 'metavm.catalog/1', 'machines': profiles}
        (fixture / 'MANIFEST.yaml').write_text(json.dumps(manifest))
        try:
            generate(Path(sys.argv[1]).resolve(), fixture, fixture)
        except ValueError as error:
            assert 'must not overwrite' in str(error)
        else:
            raise AssertionError('generator accepted the source catalog as output')
        bad = fixture / 'VMLibrary/Second/Second.mvm'
        good = bad.read_text()
        bad.write_text('unknown instruction\n')
        artifacts = root / 'artifacts'
        first = artifacts / 'VMLibrary/First/First.mvmb'
        first.parent.mkdir(parents=True)
        first.write_bytes(b'prior binary')
        before = snapshot(artifacts)
        try:
            generate(Path(sys.argv[1]).resolve(), artifacts, fixture)
        except subprocess.CalledProcessError:
            pass
        else:
            raise AssertionError('invalid assembly was accepted')
        assert snapshot(artifacts) == before, 'failed compilation damaged outputs'
        bad.write_text(good)
        generate(Path(sys.argv[1]).resolve(), artifacts, fixture)
        before = snapshot(artifacts)
        generate(Path(sys.argv[1]).resolve(), artifacts, fixture)
        assert snapshot(artifacts) == before, 'example generation is not stable'
    print('Catalog reproducibility, native preservation and failed-generation rollback passed')


if __name__ == '__main__':
    main()
