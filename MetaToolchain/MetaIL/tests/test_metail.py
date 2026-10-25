#!/usr/bin/env python3
"""Integration/semantic tests using the genuine Lua 5.4-backed MetaIL binary."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
exe = Path(sys.argv[1]).resolve()
env = {**os.environ, 'METAIL_HOME': str(root)}

def run(*args, ok=True):
    p = subprocess.run([str(exe), *map(str,args)], capture_output=True, text=True, env=env)
    if ok:
        assert p.returncode == 0, (args, p.stdout, p.stderr)
    else:
        assert p.returncode != 0, (args, p.stdout, p.stderr)
    return p.stdout

assert '321 Optikern kernels' in run('kernels')
all_kernels = json.loads(run('kernels','--json'))
assert all_kernels['count'] == 321
assert len({k['name'] for k in all_kernels['kernels']}) == 321
assert sum(k['kind'] == 'analysis' for k in all_kernels['kernels']) == 84
assert sum(k['kind'] == 'rewrite' for k in all_kernels['kernels']) == 230
assert sum(k['kind'] == 'builtin' for k in all_kernels['kernels']) == 7
names=[k['name'] for k in all_kernels['kernels']]
assert names == sorted(names)
for kernel in all_kernels['kernels']:
    assert Path(kernel['path']).is_file(),kernel
    assert kernel['kind'] in kernel['tags'],kernel
    assert kernel['uses'] and kernel['keywords'],kernel
numeric=json.loads(run('kernels','--tag','NUMERIC','--tag','statistics','--kind','analysis','--json'))
assert numeric['count']==24
strength=json.loads(run('kernels','--search','multiply power','--tag','strength-reduction',
                        '--use','shifts','--kind','rewrite','--json'))
assert strength['count']>0
for kernel in strength['kernels']:
    assert 'strength-reduction' in kernel['tags']
assert not run('kernels','--tag','no-such-tag','--paths')
assert json.loads(run('kernels','--tag','no-such-tag','--json'))=={'count':0,'kernels':[]}
exact=run('kernels','--search','AddZeroRight','--source')
assert 'add($x,0)' in exact and 'AddZeroRight.optk' in exact
assert len(run('kernels','--search','AddZeroRight','--paths').splitlines())==1
for args in [('kernels','--json','--paths'),('kernels','--search'),('kernels','--kind','unknown'),
             ('kernels','--out','unused'),('verify','extra')]:
    run(*args,ok=False)
# Numeric kernels live alongside the base declarations in the committed layout.
assert len(list((root/'optimize-kernels').glob('Numeric*.optk'))) == 120
assert 'selftest ok' in run('selftest')
assert 'verified 230 rewrites' in run('verify')
assert 'valid MetaIL' in run('check',root/'examples/arithmetic.mi')
emitted = run('emit',root/'examples/arithmetic.mi')
scratch = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
with tempfile.TemporaryDirectory(prefix='metail-',dir=scratch) as d:
    custom=Path(d)/'custom kernels';custom.mkdir()
    (custom/'custom.optk').write_text('''local K = lkopt.optimizer.new "ProjectIdentity"
K:rewrite { pattern = "add($x,0)", replace = "$x" }
K:metadata { keywords = {"widgets"}, tags = {"project"}, uses = {"reduce widget cost"} }
''')
    selected=json.loads(run('kernels','--kernel-dir',custom,'--search','widgets','--tag','project',
                            '--use','reduce cost','--json'))
    assert selected['count']==1 and selected['kernels'][0]['name']=='ProjectIdentity'
    custom_input=Path(d)/'custom.mi';custom_input.write_text('fn f(x) { return x+0; }')
    assert 'return x;' in run('optimize',custom_input,'--kernel-dir',custom,'--passes','ProjectIdentity')
    for passes in ('',',','AddZeroRight,',',AddZeroRight','AddZeroRight,,ConstantFolding'):
        run('optimize',custom_input,'--passes',passes,ok=False)
    fn=Path(d)/'roundtrip.mi';fn.write_text(emitted)
    assert 'valid MetaIL' in run('check',fn)
    optimized = run('optimize',fn)
    assert '<< 3' in optimized, optimized
    assert 'unused' not in optimized
    assert ' * 1' not in optimized
    assert ' + 0' not in optimized
    output=Path(d)/'result.mi'
    changes=Path(d)/'changes.json'
    run('optimize',fn,'--out',output,'--stats',changes)
    assert output.exists() and changes.exists()
    counts=json.loads(changes.read_text())['kernel_changes']
    assert any(value > 0 for value in counts.values()), counts

    fn.write_text('fn f() { let z = mul(log_event(),0); return 3; }')
    optimized=run('optimize',fn)
    assert 'eval ' in optimized and 'log_event()' in optimized, optimized
    assert 'valid MetaIL' in run('check',output)

    fn.write_text('fn f() { let x = add(neg(before()),after()); return x; }')
    rewritten=run('optimize',fn,'--passes','AddNegativeLeft')
    assert 'before()' in rewritten and 'after()' in rewritten and ' + ' in rewritten, rewritten

    fn.write_text('fn empty() {}')
    assert 'fn empty()' in run('optimize',fn)

    fn.write_text('fn nope() { goto nonexistent; }')
    run('check',fn,ok=False)
    fn.write_text('fn bad() { return @; }')
    run('check',fn,ok=False)

    # Traps must survive both guarded rewrites and procedural dead-code cleanup.
    for expr in ('div(1,0)', 'div(-9223372036854775808,-1)', 'shr(1,64)', 'shl(1,-1)'):
        fn.write_text(f'fn trap() {{ let unused = mul({expr},0); return 3; }}')
        optimized=run('optimize',fn)
        assert 'eval ' in optimized and ' * 0' in optimized, optimized

    fn.write_text('fn f(x) { let x = add(x,1); let y = add(x,1); return y; }')
    optimized=run('optimize',fn,'--passes','CommonSubexpressionElimination')
    assert 'let y = (x + 1)' in optimized, optimized

    for source in ('fn f() { return $x; }', 'fn f(x$y) { return 1; }',
                   'fn f() { return 0x10000000000000000; }',
                   'fn f() { return '+('('*300)+'1'+(')'*300)+'; }',
                   'fn f(x) { return '+('x+'*300)+'1; }'):
        fn.write_text(source)
        run('check',fn,ok=False)

    fn.write_text('fn f() { entry: goto loop; loop: br 1, loop, exit; exit: return 0; }')
    loops=json.loads(run('analyze',fn,'--passes','NaturalLoops'))['analyses']['NaturalLoops']['f']
    assert loops==[{'header':'loop','latch':'loop','blocks':['loop']}],loops

    fn.write_text('fn f() { entry: let x=1; goto join; dead: let x=2; goto join; join: return x; }')
    data=json.loads(run('analyze',fn,'--passes','Dominators,ReachingDefinitions'))['analyses']
    assert data['Dominators']['f']['join']==['entry','join'],data
    assert data['ReachingDefinitions']['f']['join']['entry']==['x@2'],data
    fn.write_text('fn f(x) { entry: br x, loop, exit; loop: goto loop; exit: return 0; }')
    data=json.loads(run('analyze',fn,'--passes','PostDominators'))['analyses']['PostDominators']['f']
    assert data['entry']==['entry'] and data['loop']==['loop'],data

    # Exercise all 24 numeric analyses, with independently computed answers.
    import math
    import statistics
    fn.write_text('fn numeric(x) { eval add(x,1); eval mul(x,2); eval neg(3); return 4; } fn empty() {}')
    report=json.loads(run('analyze',fn))['analyses']
    for source,values in [('Literals',[1,2,3,4]),('ExpressionSizes',[3,3,2,1])]:
        expected={'Mean':statistics.mean(values),'SampleVariance':statistics.variance(values),
                  'PopulationVariance':statistics.pvariance(values),'SampleStdDev':statistics.stdev(values),
                  'PopulationStdDev':statistics.pstdev(values),'Median':statistics.median(values),
                  'Minimum':min(values),'Maximum':max(values)}
        ordered=sorted(values)
        for p in (10,25,75,90):
            pos=(len(values)-1)*p/100
            lo=math.floor(pos)
            expected[f'Quantile{p}']=ordered[lo]+(ordered[min(lo+1,len(values)-1)]-ordered[lo])*(pos-lo)
        for statistic,value in expected.items():
            result=report[f'Numeric{source}{statistic}']
            assert result['numeric']['status']=='ok',result
            assert result['numeric']['count']==4,result
            assert math.isclose(result['numeric']['value'],value,rel_tol=1e-14),result
            assert result['empty']['status']=='insufficient-data',result

report=json.loads(run('analyze',root/'examples/dataflow.mi', '--passes',
                     'ControlFlowGraph,LiveVariables,Dominators,ReachingDefinitions,NaturalLoops,UnreachableBlocks'))
a=report['analyses']
assert a['UnreachableBlocks']['main']==['dead'], a
assert a['LiveVariables']['main']['join']['live_in']==['x'], a
assert sorted(a['Dominators']['main']['join'])==['entry','join'], a
assert sorted(a['ReachingDefinitions']['main']['join']['entry'])==['x@5','x@8'], a
assert a['NaturalLoops']['main']==[], a
op_counts=json.loads(run('analyze', root/'examples/arithmetic.mi', '--passes', 'CountAddOperations,CountMulOperations'))
assert op_counts['analyses']['CountAddOperations']['count']>=2
assert op_counts['analyses']['CountMulOperations']['count']>=2
print('OK: 321 kernels, 230 rewrite equivalence tests, 24 numeric analyses, parser/IR, CFG/dataflow, effects/traps, CLI')
