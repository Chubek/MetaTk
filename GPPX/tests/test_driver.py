"""Exercise CLI argument handling, config paths, persistent REPL and profiles."""
import pathlib
import subprocess
import sys
import tempfile

cli = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()


def run(*args, stdin=None, ok=True):
    result = subprocess.run([str(cli), *map(str, args)], input=stdin,
                            text=True, capture_output=True)
    assert (result.returncode == 0) == ok, (args, result.stdout, result.stderr)
    return result


assert run('-DA=3', '-e', '#eval A+2').stdout == '5'
assert run('-DA=3', '-UA', '-e', 'A').stdout == 'A'
assert run('--check', '-e', 'hidden').stdout == ''
assert run('--no-strict', '-e', '#unknown').stdout == '#unknown'
assert run('-', stdin='stdin\n').stdout == 'stdin\n'
for args in [('--max-output', '-1'), ('--max-depth', '12x'),
             ('--max-includes', '0'), ('-e', 'x', 'file'),
             ('--check', '-o', 'file'), ('--missing',), ('-o',)]:
    run(*args, ok=False)

with tempfile.TemporaryDirectory(prefix='gppx-driver-') as directory:
    folder = pathlib.Path(directory)
    (folder / 'header.gppx').write_text('VALUE\n')
    config = folder / 'config.yaml'
    config.write_text('include_paths:\n  - .\ndefines:\n  VALUE: configured\n'
                      'line_comment: ""\n')
    assert run('--config', config, '-e', '#include "header.gppx"').stdout == 'configured\n'
    assert run('--config', config, '-DVALUE=override', '-e', 'VALUE // VALUE').stdout == 'override // override'
    out = folder / 'output'
    run('-e', 'saved', '-o', out)
    assert out.read_text() == 'saved'
    source = folder / 'source.jcks'
    source.write_bytes(b'\xef\xbb\xbf#!/usr/bin/env jsc\n#define A 7\nA $(A)\n')
    assert run('--profile', 'jackscript', source).stdout == '#!/usr/bin/env jsc\nA 7\n'
    source.write_bytes(b'\xff')
    run('--profile', 'jackscript', source, ok=False)

result = run('--repl', stdin=':define A 7\nA\n:begin\n#repeat 2\nA\n#endrepeat\n:end\n:quit\n')
assert result.stdout == '7\n7\n7\n'
run('--repl', stdin=':begin\nunclosed\n', ok=False)
sample = run('--profile', 'jackscript', root / 'examples/jackscript/ReadCSV.jcks').stdout
assert 'const firstField := String:Split(firstLine, ",")[0];' in sample
print('PASS driver integration')
