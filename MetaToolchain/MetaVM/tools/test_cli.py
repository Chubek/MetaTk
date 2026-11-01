#!/usr/bin/env python3
"""Failure-path and in-place-output checks for the actual compiler executables."""
import pathlib,subprocess,sys,tempfile
b=pathlib.Path(sys.argv[1]).resolve()
runner=pathlib.Path(sys.argv[2]).resolve() if len(sys.argv)>2 else b/'metavm-run'
def run(*args,ok=True):
 executable=pathlib.Path(args[0])
 if executable.is_file():executable.chmod(executable.stat().st_mode|0o100)
 result=subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=15)
 if (result.returncode==0)!=ok:raise AssertionError(result.stdout+result.stderr)
 return result
with tempfile.TemporaryDirectory(prefix='cli-',dir=b) as tmp:
 d=pathlib.Path(tmp);source=d/'source.mvm';output=d/'result.c'
 source.write_text('.function main 0 1\nconst r0 42\nret r0\n.end\n')
 output.write_text('KEEP ME')
 run(b/'vmweave','emit-c',source,output,'int',ok=False)
 assert output.read_text()=='KEEP ME'
 source.write_text('this is invalid assembly')
 run(b/'vmweave','compile',source,output,ok=False)
 assert output.read_text()=='KEEP ME'
 source.write_text('.function main 0 1\nconst r0 42\nret r0\n.end\n')
 run(b/'vmweave','compile',source,source)
 assert run(runner,source).stdout.strip()=='42'
 run(b/'vmweave','emit-c',source,output,'valid_factory')
 assert 'valid_factory' in output.read_text()
 run(b/'vmweave','emit-c',source,d,'valid_factory',ok=False)
 assert sorted(d.glob('*.metavm-tmp-*'))==[]
 ssa=d/'argument.ssa';ssa.write_text('int_arg\nreturn (0)\n')
 assert run(b/'ssavm-run',ssa,42).stdout.strip()=='42'
 run(b/'ssavm-run',ssa,'invalid',ok=False)
 ssa.write_text('\nconst 1\nreturn (99)\n')
 assert 'line 3' in run(b/'ssavm-run',ssa,ok=False).stderr
 ssa.write_text('int_arg\nreturn (0)\n')
 assert run(b/'ssavm-run','--max-steps',2,ssa,-42).stdout.strip()=='-42'
 assert 'fuel exhausted' in run(b/'ssavm-run','--max-steps',1,ssa,42,ok=False).stderr
 for option in ['--max-steps','--max-array-elements','--max-total-elements','--max-arrays']:
  assert run(b/'ssavm-run',option,'-1',ssa,ok=False).returncode==2
 assert run(b/'ssavm-run','--unknown',1,ssa,ok=False).returncode==2
 ssa.write_text('const 0\narray (0)\ngoto [1] 0\n')
 assert 'allocation count' in run(b/'ssavm-run','--max-arrays',2,ssa,ok=False).stderr
 ssa.write_text('const 2\narray (0)\nreturn (1)\n')
 assert run(b/'ssavm-run','--max-array-elements',2,'--max-total-elements',2,ssa).stdout.strip()=='array[2]'
 assert 'allocation budget' in run(b/'ssavm-run','--max-total-elements',1,ssa,ok=False).stderr
 assert 'allocation limit' in run(b/'ssavm-run','--max-array-elements',1,ssa,ok=False).stderr
 ssa.write_text('int_arg\nreturn (0)\n')
 assert run(b/'ssavm-run','--check','--max-steps',0,ssa).stdout.strip()=='verified'
 assert run(b/'ssavm-run','--check',ssa,42,ok=False).returncode==2
 for option in ['--max-analysis-states','--max-analysis-cells','--max-analysis-work']:
  assert run(b/'ssavm-run',option,'-1','--check',ssa,ok=False).returncode==2
  failure=run(b/'ssavm-run','--check',option,0,ssa,ok=False)
  assert failure.returncode==1 and 'analysis' in failure.stderr and 'line 1' in failure.stderr
 ssa.write_text('# header\n\nconst 0\nlength (0)\nreturn (1)\n')
 failure=run(b/'ssavm-run','--check',ssa,ok=False)
 assert failure.returncode==1 and 'line 4' in failure.stderr and 'expected array' in failure.stderr
 ssa.write_text('goto [0] 0\n')
 assert run(b/'ssavm-run','--check',ssa).stdout.strip()=='verified'
print('Compiler CLI preservation, in-place output and SSA analysis diagnostics passed')
