#!/usr/bin/env python3
"""Individually compile changed public headers, without optional dependencies."""
from pathlib import Path
import os,shlex,subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
cc=shlex.split(sys.argv[1] if len(sys.argv)>1 else os.environ.get('CC','cc'))
cxx=shlex.split(sys.argv[2] if len(sys.argv)>2 else os.environ.get('CXX','c++'))
c_headers=['Machinery/CBridge.h','Machinery/OS.h','Machinery/Loader.h','vmweave/vmweave.h','MetaVM.h']
cpp_headers=['vmweave/Builder.hpp','vmweave/Definition.hpp','vmweave/examples/ToyR16.hpp','VMLibrary/SSAVM/ISSA.hpp','VMLibrary/CHIP-8/Chip8.hpp','MetaVM.hpp']
cpp_headers+=['VMLibrary/SECD/SECDMachine.hpp','VMLibrary/Krivine/KrivineMachine.hpp']
optional_headers=['vmweave/MetaMLAdapter.hpp'] if len(sys.argv)>3 else []
for header in c_headers+cpp_headers+optional_headers:
 modes=['c11','c++11'] if header in c_headers else ['c++20' if header in optional_headers else 'c++17']
 for mode in modes:
  source='#include "'+header+'"\nint main(void){return 0;}\n'
  cmd=(cc if mode=='c11' else cxx)+['-std='+mode,'-pedantic-errors','-Wall','-Wextra','-Werror','-I'+str(r),'-I'+str(r/'Machinery'),'-fsyntax-only','-x','c' if mode=='c11' else 'c++','-']
  if header in optional_headers:cmd+=['-I'+sys.argv[3]]
  subprocess.run(cmd,input=source,text=True,check=True,timeout=60)
print('Changed public C11/C++11 and C++17 headers compile independently')
