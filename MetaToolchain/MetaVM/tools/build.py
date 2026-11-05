#!/usr/bin/env python3
"""Dependency-free compiler fallback when CMake is not installed.
All sources are compiled on each invocation so header changes cannot go stale.
Use CMake for install/export, shared libraries and optional external backends.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse,os,subprocess,shutil,sys
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--test',action='store_true');p.add_argument('--sanitize',action='store_true');p.add_argument('--clean',action='store_true');a=p.parse_args()
b=r/('build-sanitize' if a.sanitize else 'build')
if a.clean:
 for name in ['build','build-sanitize']:
  path=r/name
  if path.exists():shutil.rmtree(path)
 raise SystemExit()
b.mkdir(exist_ok=True)
cc=os.environ.get('CC','cc');cxx=os.environ.get('CXX','c++')
common=['-g','-O1','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(r/'Machinery'),'-I'+str(r/'vmweave'),'-I'+str(r)]
if a.sanitize:common+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie']
link=['-fsanitize=address,undefined','-no-pie'] if a.sanitize else []
def run(cmd):
 # Workspace synchronization may restore created files without executable bits.
 # Restore owner execution only for binaries inside this selected build tree.
 executable=Path(cmd[0])
 if executable.is_absolute() and b in executable.parents and executable.is_file():
  executable.chmod(executable.stat().st_mode|0o100)
 subprocess.run(list(map(str,cmd)),check=True,cwd=b if executable.is_absolute() and b in executable.parents else r)
 if '-o' in cmd and '-c' not in cmd:
  output=Path(cmd[cmd.index('-o')+1]);output.chmod(output.stat().st_mode|0o100)
sources=list((r/'Machinery').glob('*.c'))+[r/'vmweave/vmweave.c']+list((r/'VMLibrary').rglob('*.c'))+list((r/'VMLibrary').rglob('*.cpp'))+list((r/'vmspecs').rglob('*.c'))
def compile_one(src):
 obj=b/(str(src.relative_to(r)).replace('/','_')+'.o')
 run([cxx if src.suffix=='.cpp' else cc,'-std=c++17' if src.suffix=='.cpp' else '-std=c11',*common,'-c',src,'-o',obj]);return obj
with ThreadPoolExecutor(max_workers=4) as pool:objs=list(pool.map(compile_one,sources))
archive=b/'libmetavm.a'
if archive.exists():archive.unlink()
run(['ar','rcs',archive,*objs])
run([cc,'-std=c11',*common,r/'vmweave/main.c',archive,*link,'-o',b/'vmweave'])
run([sys.executable,r/'tools/generate_examples.py','--vmweave',b/'vmweave','--output-root',b/'generated'])
run([cc,'-std=c11',*common,r/'Machinery/examples/run.c',archive,*link,'-o',b/'metavm-run'])
run([cxx,'-std=c++17',*common,r/'driver/ssavm.cpp',archive,*link,'-o',b/'ssavm-run'])
run([cxx,'-std=c++17',*common,r/'driver/dvi.cpp',archive,*link,'-o',b/'dvi-run'])
run([cxx,'-std=c++17',*common,r/'driver/closure_demo.cpp',archive,*link,'-o',b/'closure-vm-demo'])
run([cxx,'-std=c++17',*common,r/'vmweave/examples/definition.cpp',archive,*link,'-o',b/'vmweave-definition-demo'])
if a.test:
 run([sys.executable,r/'tools/validate_catalog.py'])
 run([sys.executable,r/'tools/test_generation.py',b/'vmweave'])
 run([sys.executable,r/'tools/generate_vmspec_manual.py','--check'])
 run([sys.executable,r/'tools/test_headers.py',cc,cxx])
 run([cc,'-std=c11',*common,r/'Machinery/tests/test_machinery.c',archive,*link,'-o',b/'test_machinery'])
 run([cc,'-std=c11',*common,r/'Machinery/tests/test_xvm.c',archive,*link,'-o',b/'test_xvm'])
 run([cxx,'-std=c++17',*common,r/'tests/integration.cpp',archive,*link,'-o',b/'integration'])
 run([cxx,'-std=c++17',*common,r/'tests/refinement.cpp',archive,*link,'-o',b/'refinement'])
 run([cxx,'-std=c++17',*common,r/'tests/chip8_roms.cpp',archive,*link,'-o',b/'chip8_roms'])
 run([cxx,'-std=c++11',*common,'-x','c++',r/'Machinery/tests/Cpp.hpp','-x','none',archive,*link,'-o',b/'test_cpp'])
 run([cxx,'-std=c++17',*common,r/'tests/closure_vms.cpp',archive,*link,'-o',b/'closure_vms'])
 run([cxx,'-std=c++17',*common,r/'tests/dvi.cpp',archive,*link,'-o',b/'dvi'])
 with ThreadPoolExecutor(max_workers=4) as pool:
  generated_objects=list(pool.map(compile_one,sorted((b/'generated/VMLibrary').rglob('*.c'))))
 run([cxx,'-std=c++17',*common,'-I'+str(b/'generated'),r/'tests/catalog_generation.cpp',*generated_objects,archive,*link,'-o',b/'catalog_generation'])
 run([b/'catalog_generation',r,b/'generated'])
 run([b/'dvi'])
 run([sys.executable,r/'tools/test_dvi.py',b/'dvi-run'])
 for executable in ['test_machinery','test_xvm','test_cpp','integration','refinement','closure_vms','closure-vm-demo','vmweave-definition-demo']:run([b/executable])
 run([b/'chip8_roms',r/'tests/fixtures/chip8'])
 run([b/'metavm-run',r/'Machinery/examples/factorial.mvm'])
 run([b/'metavm-run',r/'Machinery/examples/factorial.mvm','--threaded'])
 # Compile generated C with the real compiler and execute its module factory.
 generated=b/'generated.c';run([b/'vmweave','emit-c',r/'VMLibrary/JVM/JVM.mvm',generated,'generated_factory'])
 (b/'generated_test.c').write_text('#include "MetaVM.h"\nextern mvm_status generated_factory(mvm_module**,mvm_error*);\nint main(void){mvm_module*m=0;mvm_error e;mvm_status s=generated_factory(&m,&e);mvm_module_destroy(m);return s;}\n')
 run([cc,'-std=c11',*common,generated,b/'generated_test.c',archive,*link,'-o',b/'generated_test']);run([b/'generated_test'])
 run([b/'ssavm-run',r/'VMLibrary/SSAVM/fibonacci.ssa'])
 run([b/'ssavm-run','--check',r/'VMLibrary/SSAVM/fibonacci.ssa'])
 run([b/'vmweave-definition-demo',b/'generated_definition.c'])
 run([cc,'-std=c11',*common,b/'generated_definition.c',r/'tests/definition_factory.c',archive,*link,'-o',b/'definition_factory_test'])
 run([b/'definition_factory_test'])
 run([b/'vmweave','compile',r/'VMLibrary/JVM/JVM.mvm',b/'roundtrip.mvmb'])
 run([b/'metavm-run',b/'roundtrip.mvmb'])
 run([sys.executable,r/'tools/test_cli.py',b])
 print('All compiler, Machinery, catalog and native subset checks passed.')
print('Built',b)
