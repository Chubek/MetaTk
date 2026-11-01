#!/usr/bin/env python3
"""Install, relocate, and compile consumers using only the installed SDK."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
for name in ['cmake', 'build', 'source', 'cc', 'cxx', 'config', 'libdir', 'bindir']:
    p.add_argument('--'+name, required=True)
a = p.parse_args()

def run(*args, cwd=None):
    subprocess.run([str(x) for x in args], check=True, cwd=cwd, timeout=90)

with tempfile.TemporaryDirectory(prefix='sdk-', dir=a.build) as temporary:
    tmp = Path(temporary)
    stage, prefix = tmp/'stage', tmp/'relocated'
    run(a.cmake, '--install', a.build, '--prefix', stage, '--config', a.config)
    stage.rename(prefix)
    for config in prefix.rglob('*.cmake'):
        text = config.read_text()
        if str(stage) in text or str(Path(a.build).resolve()) in text:
            raise AssertionError('Non-relocatable exported file: '+str(config))
    source = tmp/'consumer'
    shutil.copytree(a.source, source)
    asm = source/'example.mvm'
    asm.write_text('.function main 0 1\nconst r0 42\nret r0\n.end\n')
    suffix = '.exe' if os.name == 'nt' else ''
    tools = prefix/a.bindir
    run(tools/('closure-vm-demo'+suffix))
    run(tools/('vmweave'+suffix), 'emit-c', asm, source/'generated.c', 'installed_factory')
    run(tools/('vmweave'+suffix), 'compile', asm, source/'example.mvmb')
    if (tools/('metavm-run'+suffix)).exists():
        run(tools/('metavm-run'+suffix), source/'example.mvmb')
    ssa = source/'example.ssa'
    ssa.write_text('const 42\nreturn (0)\n')
    run(tools/('ssavm-run'+suffix), ssa)
    run(tools/('ssavm-run'+suffix), '--check', ssa)
    run(a.cmake, '-S', source, '-B', tmp/'consumer-build',
        '-DCMAKE_PREFIX_PATH='+str(prefix), '-DMetaVM_DIR='+str(prefix/a.libdir/'cmake/MetaVM'),
        '-DCMAKE_C_COMPILER='+a.cc, '-DCMAKE_CXX_COMPILER='+a.cxx,
        '-DCMAKE_BUILD_TYPE='+a.config, '-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF',
        '-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF')
    run(a.cmake, '--build', tmp/'consumer-build', '--config', a.config, '-j2')
    run(Path(a.cmake).with_name('ctest'+suffix), '--test-dir', tmp/'consumer-build',
        '-C', a.config, '--output-on-failure')
print('Installed SDK tools and C/C++ consumers pass after prefix relocation')
