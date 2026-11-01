#!/usr/bin/env python3
"""Exercise the real MetaML CLI and emit a C factory for the native C test."""
from pathlib import Path
import subprocess
import sys
import tempfile

metaml, compiler, root, artifact = map(lambda x: Path(x).resolve(), sys.argv[1:])

def run(*args, ok=True):
    result = subprocess.run([str(a) for a in args], capture_output=True, text=True, timeout=30)
    if (result.returncode == 0) != ok:
        raise AssertionError(result.stdout + result.stderr)
    return result

with tempfile.TemporaryDirectory(prefix='metaml-', dir=artifact.parent) as directory:
    d = Path(directory)
    source, guest, output = d/'source.vmweave', d/'guest.bin', d/'output.mvmb'
    output.write_bytes(b'KEEP ME')
    source.write_text('\nval x := missing\n')
    result = run(metaml, source, output, ok=False)
    assert 'M0204' in result.stderr and ':2:' in result.stderr
    assert output.read_bytes() == b'KEEP ME'
    source.write_text('val _ := vm_function "main" 0 1\n')
    run(metaml, source, output, ok=False)
    assert output.read_bytes() == b'KEEP ME'
    definition = root/'vmweave/examples/ToyR16.vmweave'
    guest.write_bytes(b'\xff\xff')
    run(metaml, '--definition', definition, guest, output, ok=False)
    assert output.read_bytes() == b'KEEP ME'
    guest.write_bytes(bytes([0x10, 40, 0x11, 2, 0x20, 0x10, 0x30, 3, 0x50, 0]))
    run(metaml, '--definition', definition, guest, output)
    run(compiler, 'emit-c', output, artifact, 'toy_r16_factory')
    source.write_text((root/'vmweave/factorial.vmweave').read_text())
    run(metaml, source, source)
    companion = d/'factorial.mvmb'
    run(compiler, 'compile', root/'vmweave/factorial.mvm', companion)
    assert source.read_bytes() == companion.read_bytes()
    assert not list(d.glob('*.metavm-tmp-*'))
print('MetaML CLI diagnostics, output preservation and Definition C emission passed')
