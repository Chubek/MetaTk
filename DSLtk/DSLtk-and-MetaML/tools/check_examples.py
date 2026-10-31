from pathlib import Path
import subprocess, sys, shutil, random
R=Path(__file__).resolve().parents[1]; build=R/'build'; build.mkdir(exist_ok=True)
mode=sys.argv[1] if len(sys.argv)>1 else 'cpp'
def run(args,**kw):return subprocess.run([str(x) for x in args],check=True,**kw)
def cpp(source,target):
 compiler=shutil.which('g++') or shutil.which('clang++')
 if not compiler: raise SystemExit('A C++20 compiler is required')
 run([compiler,'-std=c++20','-O0','-pthread','-I'+str(R/'vendor'),source,'-o',target])
if mode=='cpp':
 target=build/'examples-cpp';cpp(R/'examples/all_cpp.cpp',target);run([target])
elif mode=='d':
 compiler=shutil.which('ldc2') or shutil.which('dmd')
 if not compiler:raise SystemExit('D tests NOT RUN: install ldc2 or dmd')
 for source in sorted((R/'examples/d').glob('*.d')):
  target=build/source.stem
  run([compiler,'-I'+str(R/'vendor'),R/'vendor/DSLtk.d',R/'vendor/MetaML.d',source,'-of='+str(target)])
  run([target])
 for case in ['formula','policy']:
  target=build/(case+'-d')
  run([compiler,'-I'+str(R/'vendor'),R/'vendor/DSLtk.d',R/'vendor/MetaML.d',R/f'case-studies/{case}/main.d','-of='+str(target)])
  if case=='policy':run([target])
  else:run([target],input='(mul (add 42 0) 1)\n',text=True)
 print('PASS: 120 D workshops and two D case programs')
elif mode=='cases':
 formula=build/'formula';policy=build/'policy'
 cpp(R/'case-studies/formula/main.cpp',formula);cpp(R/'case-studies/policy/main.cpp',policy)
 def check(text,status,expected=None,budget=None):
  args=[str(formula)]+([] if budget is None else [str(budget)])
  p=subprocess.run(args,input=text,text=True,capture_output=True)
  assert p.returncode==status,(text,p.returncode,p.stderr)
  if expected is not None:assert p.stdout.strip()==expected,(text,p.stdout)
 check('(mul (add 42 0) 1)',0,'42')
 check('(add 42 0)',2,'(add 42 0)',0)
 for invalid in ['(add 42)','(unknown 1 2)','"42"','42junk','(add 1 2 3)','-1','18446744073709551616']:
  check(invalid,1)
 rng=random.Random(42)
 def expr(depth):
  if depth==0 or rng.randrange(3)==0:return str(rng.choice([0,1,42,2**64-1,rng.randrange(10000)]))
  return '('+rng.choice(['add','mul'])+' '+expr(depth-1)+' '+expr(depth-1)+')'
 for _ in range(100):check(expr(4),0)
 run([policy]);print('PASS: formula valid/invalid/budget cases, 100 fixed-seed semantic checks, policy reload checks')
else:raise SystemExit('Usage: check_examples.py cpp|d|cases')
