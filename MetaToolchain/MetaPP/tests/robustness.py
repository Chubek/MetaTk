"""Binary CLI input, output preservation, and bounded malformed-input checks."""
import pathlib
import random
import subprocess
import sys
import tempfile

tool = sys.argv[1]

def run(data, *args):
    return subprocess.run([tool, *args], input=data, capture_output=True, timeout=10)

with tempfile.TemporaryDirectory(prefix='metapp-') as directory:
    root = pathlib.Path(directory)
    source = root / 'input.c'
    output = root / 'output.i'
    data = b'int before;\x00#error after\n'
    source.write_bytes(data)
    for args in [(), (str(source),)]:
        output.write_bytes(b'original output\n')
        result = run(data, *args, '-o', str(output))
        assert result.returncode == 1 and b'NUL byte' in result.stderr, result
        assert output.read_bytes() == b'original output\n'
    result = run(b'#define EMPTY\nEMPTY\n')
    assert result.returncode == 0, result

# Every byte exercises lexical handling; failures are ordinary diagnostics,
# never crashes, sanitizer reports, or timeouts. Fixed seed for reproducibility.
rng = random.Random(601)
inputs = [bytes([n]) for n in range(256)]
inputs += [bytes(rng.randrange(256) for _ in range(rng.randrange(1, 256))) for _ in range(128)]
parts = [b'x', b'(', b')', b',', b'#', b'##', b'%:', b'%:%:', b'...', b'1', b'"text"', b'__VA_ARGS__']
for _ in range(128):
    body = b' '.join(rng.choice(parts) for _ in range(rng.randrange(1, 20)))
    inputs.append(b'#define F(x, ...) ' + body + b'\nF(F(1,2),3)\n')
for data in inputs:
    result = run(data)
    assert result.returncode in (0, 1), (data, result)
    assert b'AddressSanitizer' not in result.stderr and b'runtime error:' not in result.stderr, (data, result)
print(f'{len(inputs)} byte-input robustness checks passed')
