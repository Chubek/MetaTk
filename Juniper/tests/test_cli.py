import json
import os
from pathlib import Path
import subprocess
import sys

tool, source, work = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
work.mkdir(parents=True, exist_ok=True)


def run(*args, status=0, cwd=None, input=None):
    p = subprocess.run([str(tool), *map(str, args)], cwd=cwd, input=input,
                       capture_output=True, text=True, timeout=20)
    assert p.returncode == status, (args, p.returncode, p.stdout, p.stderr)
    return p.stdout


run('--help')
run('--version')
run('check', source / 'examples/Juniperfile')
run('check', cwd=source / 'examples')
canonical = run('emit', source / 'examples/Juniperfile')
assert canonical == run('emit', '-', '--no-preprocess', input=canonical)
assert canonical == run('emit', '-', '--no-preprocess', input=run('preprocess', source / 'examples/Juniperfile'))
assert run('metail', source / 'examples/Juniperfile').startswith('module affine;')
assert isinstance(json.loads(run('analyze', source / 'examples/Juniperfile', '--passes', 'ControlFlowGraph')), dict)
selected = json.loads(run('select', source / 'examples/Juniperfile'))
scheduled = json.loads(run('schedule', source / 'examples/Juniperfile'))
assert not selected['scheduled'] and scheduled['scheduled']
assert all(i['cycle'] is not None for f in scheduled['functions'] for i in f['instructions'])
assert any(i['opcode'] == 'ADDI' for f in selected['functions'] for i in f['instructions'])
run('schedule', source / 'examples/Juniperfile', '--optimize', '--passes', 'ConstantFolding')
# Imports survive real GPPX, canonical MetaIL, optimization, selection and scheduling.
foreign = source / 'examples/external.jun'
foreign_ir = run('emit', foreign)
assert foreign_ir == run('emit', '-', '--no-preprocess', input=foreign_ir)
linkage = json.loads(run('linkage', foreign))
assert linkage['arguments'] == ['-lc', './libcustom.so']
assert linkage['imports'][0]['symbol'] == 'labs'
assert linkage['imports'][1]['parameters'] == ['i64']
assert json.loads(run('linkage', '-', '--no-preprocess', input=run('metail', foreign))) == linkage
foreign_optimized = run('optimize', foreign)
assert json.loads(run('linkage', '-', '--no-preprocess', input=foreign_optimized)) == linkage
for command in ('select', 'schedule'):
    machine = json.loads(run(command, foreign, '--optimize'))
    assert machine['linkage'] == linkage
    calls = [i for f in machine['functions'] for i in f['instructions'] if i['opcode'] == 'CALL']
    assert [i['operands'][0]['value'] for i in calls] == ['labs', 'custom_function']
    assert [i['external'] for i in calls] == [0, 1]
    assert all(i['barrier'] for i in calls)
run('check', '-', '--no-preprocess', input='extern fn f(i64) -> i64 from "c"; fn main(){return f();}', status=1)
previous_home = os.environ.get('METAIL_HOME')
try:
    os.environ['METAIL_HOME'] = str(work / 'invalid-runtime')
    run('select', source / 'examples/Juniperfile', '--metail-home', source.parent / 'MetaToolchain/MetaIL')
finally:
    if previous_home is None:
        del os.environ['METAIL_HOME']
    else:
        os.environ['METAIL_HOME'] = previous_home
run('check', source / 'examples/control.jun')
run('optimize', source / 'examples/control.jun')
run('unknown', status=2)
run('check', source / 'examples/Juniperfile', '--bogus', status=2)
run('check', source / 'examples/Juniperfile', '--rounds', 4, status=2)
run('emit', source / 'examples/Juniperfile', '--depfile', work / 'bad.d', status=2)
run('emit', source / 'examples/Juniperfile', '--gppx', work / 'missing-tool', status=1)
run('emit', work / 'does-not-exist', status=3)
run('emit', source / 'examples/Juniperfile', '-o', source / 'examples/profile.xpp', status=3)

# Root-relative transitive/conditional includes and shell metacharacters in paths.
directory = work / 'space $ and ; metacharacters'
directory.mkdir(exist_ok=True)
(directory / 'parts').mkdir(exist_ok=True)
(directory / 'parts/inner.xpp').write_text('#define @twice mul(#1,2)\n')
(directory / 'outer.xpp').write_text('#include "parts/inner.xpp"\n')
main = directory / 'Juniperfile'
main.write_text('#if @FEATURE\n#include "outer.xpp"\n@fn(f, (x))\n@return(@twice(x))\n@end\n#else\n#include "missing-disabled.xpp"\n#error missing FEATURE\n#endif\n')
output, depfile = directory / 'artifact.junir', directory / 'artifact.d'
run('emit', main, '-DFEATURE=1', '-o', output, '--depfile', depfile)
assert 'inner.xpp' in depfile.read_text() and 'outer.xpp' in depfile.read_text()
assert 'missing-disabled.xpp' not in depfile.read_text()
assert '\\ ' in depfile.read_text() and '$$' in depfile.read_text()
saved = output.read_bytes()
main.write_text('fn invalid(){return add(1);}\n')
run('emit', main, '-o', output, status=1)
assert output.read_bytes() == saved
run('emit', main, '-o', main, status=3)
alias = directory / 'alias'
if alias.exists():
    alias.unlink()
os.link(main, alias)
run('emit', main, '-o', alias, status=3)

# #exec must not be enabled. Failure must never publish output.
sentinel = directory / 'unexpected-exec'
main.write_text(f'#exec touch "{sentinel}"\nfn f(){{return 0;}}\n')
run('emit', main, '-o', output, status=1)
assert not sentinel.exists() and output.read_bytes() == saved

# An explicitly selected external preprocessor is still bounded.
slow = directory / 'slow-gppx'
slow.write_text('#!/bin/sh\nsleep 5\n')
slow.chmod(0o755)
run('preprocess', main, '--gppx', slow, '--timeout', 1, status=1)
assert not list(directory.glob('*.tmp.*'))
print('CLI, dependency, timeout, and output-safety checks passed')
