#!/usr/bin/env python3
"""Loaded dialect plugins, shebang/fd capture, all MetaML module imports, REPL state."""
import json
import os
import pathlib
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1]).resolve()
sources = pathlib.Path(sys.argv[2]).resolve()


def run(args, **kwargs):
    return subprocess.run([str(arg) for arg in args], capture_output=True, text=True,
                          timeout=30, **kwargs)


with tempfile.TemporaryDirectory() as td:
    root = pathlib.Path(td)

    def script(name, body):
        path = root / name
        path.write_text(body)
        path.chmod(0o700)
        return path

    emit = script('emit.py', '#!/usr/bin/env python3\nimport os\nos.write(3, b"\\\\textbf{FD three}")\n')
    # Every new primitive is checked through the loaded CLI in IR, HTML and PDF.
    source = root / 'commands.tex'
    source.write_text(r'\mmlcommand{\shout}[1]{String.upper arg1}\shout{hello}\par'
                      r'\Extern[fd=3]{emit.py}')
    for target in ('json', 'html', 'pdf'):
        dest = root / ('out.' + target)
        p = run([binary, '-f', 'plain', '-T', target, '-o', dest, source])
        assert p.returncode == 0, p.stderr
    data = json.loads((root / 'out.json').read_text())
    assert data['diagnostics'] == []
    assert data['blocks'][0]['spans'] == [{'type': 'text', 'text': 'HELLO', 'extra': ''}]
    assert data['blocks'][1]['spans'] == [{'type': 'bold', 'text': 'FD three', 'extra': ''}]
    assert '<strong>FD three</strong>' in (root / 'out.html').read_text()
    assert (root / 'out.pdf').read_bytes().startswith(b'%PDF-1.4')

    modules = ['http', 'uri', 'ffi', 'memory', 'parsec', 'shell', 'sequence', 'array',
               'stream', 'getopt', 'db', 'nosqldb', 'transpile', 'weblets', 'jsonrpc',
               'msgpack', 'sysipc', 'sqlite', 'sqldb', 'nng', 'webserver']
    (root / 'libraries.mml').write_text(
        ''.join('import "' + name + '"\n' for name in modules) +
        'val db := match SQL.connect ":memory:" with | Ok db -> db | Err e -> Core.error e\n'
        'val rows := SQL.query db "SELECT 42 AS value" []\n'
        'SQL.close db\n'
        'if rows == Ok [[("value", SqlInt 42)]] then "All libraries ready" else Core.error "SQL failed"\n')
    source.write_text(r'\mmlcommandfile{\libraries}{libraries.mml}\libraries')
    p = run([binary, '-f', 'plain', '-T', 'text', '-o', root / 'libs.txt', source])
    assert p.returncode == 0, p.stderr
    assert (root / 'libs.txt').read_text() == 'All libraries ready\n\n'

    slow = script('slow.py', '#!/usr/bin/env python3\nimport time\ntime.sleep(10)\n')
    noisy = script('noisy.py', '#!/usr/bin/env python3\nimport os\nos.write(1, b"x" * (4*1024*1024+1))\n')
    badutf = script('badutf.py', '#!/usr/bin/env python3\nimport os\nos.write(1, b"\\xff")\n')
    noshebang = script('bad.sh', 'printf wrong\n')
    for text, message in [(r'\Extern[timeout=50]{slow.py}', 'timed out'),
                          (r'\Extern{noisy.py}', 'output limit'),
                          (r'\Extern{badutf.py}', 'UTF-8'),
                          (r'\Extern{bad.sh}', 'shebang')]:
        source.write_text(text)
        destination = root / 'preserved.txt'
        destination.write_text('previous')
        p = run([binary, '-f', 'plain', '-T', 'text', '-o', destination, source])
        assert p.returncode == 1 and message in p.stderr, p.stderr
        assert destination.read_text() == 'previous'

    transcript = (sources / 'tests/repl/definitions.input').read_text()
    expected = (sources / 'tests/repl/definitions.expected').read_text()
    env = dict(os.environ, METATEX_BINARY=str(binary))
    for launcher in ('mtex', 'mlatex', 'nlatex'):
        for flag in ([], ['-i'], ['--interactive']):
            p = run([sources / 'bin' / launcher, *flag], input=transcript, cwd=root, env=env)
            assert p.returncode == 0 and p.stdout == expected, (p.stdout, p.stderr)
        # Build-tree launchers find their neighboring executable directly.
        p = run([binary.parent / launcher], input='Hello\n:quit\n', cwd=root)
        assert p.returncode == 0 and p.stdout == 'Hello\n\n', p.stderr
    p = run([binary], input=(sources / 'tests/repl/directives.input').read_text())
    assert p.returncode == 0 and p.stdout == (sources / 'tests/repl/directives.expected').read_text(), (p.stdout, p.stderr)

    count = root / 'count'
    script('once.sh', '#!/bin/sh\nprintf x >> "' + str(count) + '"\nprintf once\n')
    p = run([binary, '-f', 'plain', '-i', '-T', 'json', '-o', root / 'repl.json'], cwd=root,
            input='\\Extern{once.sh}\nSecond\n:show\n:quit\n')
    assert p.returncode == 0 and count.read_text() == 'x', p.stderr
    assert [b['spans'][0]['text'] for b in json.loads((root / 'repl.json').read_text())['blocks']] == ['once', 'Second']
    source.write_text(r'\def\seed{Retained}')
    p = run([binary, '-i', source], input='\\seed\n:quit\n')
    assert p.returncode == 0 and p.stdout == 'Retained\n\n', p.stderr
    p = run([binary, '-f', 'plain'], input='\\def\\a{old}\n:reset\n\\a\nNew\n:quit\n')
    assert p.returncode == 1 and 'undefined control sequence' in p.stderr and p.stdout == 'New\n\n'
    p = run([binary], input='\\textbf{unfinished\n')
    assert p.returncode == 1 and 'incomplete REPL input' in p.stderr
    p = run([binary], input='$$x\n+ y$$\n:quit\n')
    assert p.returncode == 0 and 'x' in p.stdout and 'y' in p.stdout, p.stderr
print('Scripting and REPL integration passed')
