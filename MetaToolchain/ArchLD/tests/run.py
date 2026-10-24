#!/usr/bin/env python3
"""End-to-end tests using real GNU assembler, C compiler, ar and Linux loader."""
import os, pathlib, shutil, subprocess, tempfile, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
LD=pathlib.Path(os.environ.get('ARCHLD_BIN', ROOT/'bin'))/'ArchLD'
def cmd(args,cwd=None,expect=0):
 p=subprocess.run([str(a) for a in args],cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 if p.returncode!=expect:
  raise AssertionError(f'command {args!r}, got {p.returncode}, wanted {expect}\nstdout:{p.stdout}\nstderr:{p.stderr}')
 return p

def source(path,body):path.write_text(body)
def run():
 with tempfile.TemporaryDirectory() as d:
  p=pathlib.Path(d)
  source(p/'start.s','''
.global _start
.text
_start:
 call run
 mov %eax,%edi
 mov $60,%eax
 syscall
''')
  source(p/'run.s','''.globl run
.text
run:
 mov $37,%eax
 ret
''')
  cmd(['as','--64','-o',p/'start.o',p/'start.s'])
  cmd(['as','--64','-o',p/'run.o',p/'run.s'])
  cmd([LD,'-o',p/'go',p/'start.o',p/'run.o']);cmd([p/'go'],expect=37)
  h=cmd(['readelf','-h',p/'go']).stdout
  assert 'EXEC' in h and 'Advanced Micro Devices X86-64' in h,h
  l=cmd(['readelf','-l',p/'go']).stdout
  assert 'R E' in l and 'RW' in l,l
  assert 'run' in cmd(['nm',p/'go']).stdout
  cmd(['ar','rcs',p/'librun.a',p/'run.o'])
  cmd([LD,'-Map='+str(p/'go.map'),'-L'+str(p),'-o',p/'goar',p/'start.o','-lrun'])
  cmd([p/'goar'],expect=37)
  assert 'librun.a(run.o)' in (p/'go.map').read_text()
  source(p/'foo.c','''
const char str[]="abcdefghijklmnopqrstuvwxyz";
int data=3;
int bss;
extern int external(int);
int run(void) {bss=7;return external(data + bss + str[0] - 97);}
''')
  source(p/'bar.c','int external(int x){return x+27;}')
  cmd(['cc','-c','-O0','-ffreestanding','-fno-pic','-fno-pie','-fno-stack-protector','-o',p/'foo.o',p/'foo.c'])
  cmd(['cc','-c','-O0','-ffreestanding','-fno-pic','-fno-pie','-fno-stack-protector','-o',p/'bar.o',p/'bar.c'])
  cmd([LD,'-o',p/'gc',p/'start.o',p/'foo.o',p/'bar.o'])
  cmd([p/'gc'],expect=37)
  assert '.rodata' in cmd(['readelf','-S',p/'gc']).stdout
  # weak definition replaced by strong
  source(p/'weak.s','.weak run\nrun:\n mov $12,%eax\n ret\n')
  cmd(['as','--64','-o',p/'weak.o',p/'weak.s'])
  cmd([LD,'-o',p/'goweak',p/'start.o',p/'weak.o',p/'run.o']);cmd([p/'goweak'],expect=37)
  # Reject duplicates, unresolved refs, bad input
  bad=cmd([LD,'-o',p/'bad',p/'start.o',p/'run.o',p/'run.o'],expect=1)
  assert 'duplicate strong' in bad.stderr,bad.stderr
  bad=cmd([LD,'-o',p/'bad',p/'start.o'],expect=1)
  assert 'undefined' in bad.stderr,bad.stderr
  (p/'malformed.o').write_bytes(b'NOT ELF')
  cmd([LD,'-o',p/'bad',p/'malformed.o'],expect=1)
  assert not (p/'bad').exists()
  # Validate that an archive which has no needed symbols stays unextracted
  source(p/'badmember.s','.globl run\nrun: ret\n.globl _start\n_start: ret\n')
  cmd(['as','--64','-o',p/'badmember.o',p/'badmember.s'])
  cmd(['ar','rcs',p/'libunneeded.a',p/'badmember.o'])
  cmd([LD,'-o',p/'gonone',p/'start.o',p/'run.o',p/'libunneeded.a'])
  cmd([p/'gonone'],expect=37)
  # COMDAT group: select exactly one definition and discard duplicate code
  source(p/'comdat1.s','.section .text.fn,"axG",@progbits,fn,comdat\n.globl fn\nfn: mov $12,%eax; ret\n')
  source(p/'comdat2.s','.section .text.fn,"axG",@progbits,fn,comdat\n.globl fn\nfn: mov $14,%eax; ret\n')
  source(p/'comdat_entry.s','.text\n.global _start\n_start: call fn; mov %eax,%edi; mov $60,%eax; syscall\n')
  for stem in ['comdat1','comdat2','comdat_entry']:
   cmd(['as','--64','-o',p/(stem+'.o'),p/(stem+'.s')])
  cmd([LD,'-o',p/'comdat',p/'comdat_entry.o',p/'comdat1.o',p/'comdat2.o'])
  cmd([p/'comdat'],expect=12)
  # PIC GOTPCRELX and _GLOBAL_OFFSET_TABLE_ linker symbol
  source(p/'pic.c','extern int external_data; int run(void) {return external_data+4;}')
  source(p/'pic_data.c','int external_data=33;')
  for stem in ['pic','pic_data']:
   cmd(['cc','-c','-O0','-fPIC','-ffreestanding','-fno-stack-protector',
        '-o',p/(stem+'.o'),p/(stem+'.c')])
  cmd([LD,'-o',p/'gopic',p/'start.o',p/'pic.o',p/'pic_data.o'])
  cmd([p/'gopic'],expect=37)
  # ELF32/i386 reader, relocator and genuine 32-bit ELF writer
  source(p/'entry32.s','.globl _start\n.text\n_start: call run; mov %eax,%ebx; mov $1,%eax; int $0x80\n')
  source(p/'run32.s','.globl run\n.text\nrun: mov $33,%eax; ret\n')
  cmd(['as','--32','-o',p/'entry32.o',p/'entry32.s'])
  cmd(['as','--32','-o',p/'run32.o',p/'run32.s'])
  cmd([LD,'--target','ELF32','-o',p/'go32',p/'entry32.o',p/'run32.o'])
  header=cmd(['readelf','-h',p/'go32']).stdout
  assert 'ELF32' in header and 'Intel 80386' in header
  # GNU-style mergeable string input is preserved, not rejected
  source(p/'string.c','const char *hello(void) {return "string-merge";}')
  cmd(['cc','-c','-O2','-fno-pic','-fno-pie','-o',p/'string.o',p/'string.c'])
  cmd([LD,'-o',p/'gostring',p/'start.o',p/'run.o',p/'string.o'])
  cmd([p/'gostring'],expect=37)
  # Basic deterministic generated files
  cmd([LD,'-o',p/'go2',p/'start.o',p/'run.o'])
  assert (p/'go').read_bytes()==(p/'go2').read_bytes()
 print('PASS: ELF64 execution, archives, strong/weak/common, COMDAT, PIC GOTPCRELX, ELF32 output, merge strings, diagnostics, reproducibility')
if __name__=='__main__':
 try:run()
 except Exception as ex:print('FAIL:',ex,file=sys.stderr);sys.exit(1)
