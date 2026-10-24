#!/usr/bin/env python3
"""Real-object checks for the DSLtk tool suite, response files and scripts."""
import pathlib, tempfile, subprocess, os, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
BIN=pathlib.Path(os.environ.get('ARCHLD_BIN', ROOT/'bin'))
def run(args,expected=0):
    p=subprocess.run([str(x) for x in args],capture_output=True,text=True)
    assert p.returncode==expected, (args,p.returncode,p.stdout,p.stderr)
    return p.stdout,p.stderr
with tempfile.TemporaryDirectory() as tmp:
    d=pathlib.Path(tmp)
    (d/'entry.s').write_text('.global _start\n.text\n_start: call run\n mov %eax,%edi\n mov $60,%eax\n syscall\n')
    (d/'run.s').write_text('.global run\n.text\nrun: mov $37,%eax\n ret\n')
    for name in ['entry','run']:
        run(['as','--64','-o',d/(name+'.o'),d/(name+'.s')])
    run(['ar','rcs',d/'librun.a',d/'run.o'])
    targets,_=run([BIN/'ArchTargets','--verbose']);assert 'ELF64' in targets and 'ELF32' in targets
    nm,_=run([BIN/'ArchNm',d/'run.o']);assert 'run' in nm
    size,_=run([BIN/'ArchSize',d/'entry.o']);assert 'text=' in size and 'total=' in size
    sections,_=run([BIN/'ArchSections',d/'entry.o']);assert '.text' in sections
    spec,_=run([BIN/'ArchSpec','--target','ELF64','--all']);assert 'R_X86_64_PC32' in spec and 'record ehdr' in spec
    rel,_=run([BIN/'ArchRelocs',d/'entry.o']);assert 'run' in rel and ('PC32' in rel or 'PLT32' in rel)
    (d/'sample.txt').write_text('alpha 42 + beta\n')
    lex,_=run([BIN/'ArchLex',d/'sample.txt']);assert 'ident\t"alpha"' in lex and 'integer\t"42"' in lex,lex
    lex,_=run([BIN/'ArchLex','--rule','_skip=[ \t\n]+','--rule','word=[a-z]+','--rule','number=[0-9]+','--rule','punct=.',d/'sample.txt']);assert 'word\t"alpha"' in lex
    # Strings in scripts, nested scripts and library lookup.
    (d/'inner.ald').write_text('SEARCH_DIR(".")\n')
    (d/'link.ald').write_text('INCLUDE("inner.ald")\nINPUT("entry.o")\nLIB(run)\nENTRY(_start)\nOUTPUT("linked")\n')
    out,_=run([BIN/'ArchLD','-T',d/'link.ald']);assert 'linked' in out
    run([d/'linked'],expected=37)
    # A later SEARCH_DIR cannot retroactively satisfy an earlier LIB.
    (d/'late-search.ald').write_text('INPUT("entry.o")\nLIB(archld_test_unique)\nSEARCH_DIR(".")\n')
    run(['ar','rcs',d/'libarchld_test_unique.a',d/'run.o'])
    _,err=run([BIN/'ArchLD','-T',d/'late-search.ald'],expected=1)
    assert 'cannot find -larchld_test_unique' in err
    # Library order across INCLUDE boundaries chooses the first provider.
    (d/'alternate.s').write_text('.global run\n.text\nrun: mov $19,%eax; ret\n')
    run(['as','--64','-o',d/'alternate.o',d/'alternate.s'])
    run(['ar','rcs',d/'libalternate.a',d/'alternate.o'])
    (d/'providers.ald').write_text('LIB(run)\nINPUT("libalternate.a")\n')
    (d/'ordered.ald').write_text('SEARCH_DIR(".")\nINPUT("entry.o")\nINCLUDE("providers.ald")\nOUTPUT("ordered")\n')
    run([BIN/'ArchLD','-T',d/'ordered.ald']);run([d/'ordered'],expected=37)
    # quoted @response argument expansion and error detection
    (d/'opts.rsp').write_text('-o "'+str(d/'response linked')+'" "'+str(d/'entry.o')+'" "'+str(d/'run.o')+'"\n')
    run([BIN/'ArchLD','@'+str(d/'opts.rsp')]);run([d/'response linked'],expected=37)
    (d/'bad.rsp').write_text('@bad.rsp')
    _,err=run([BIN/'ArchLD','@'+str(d/'bad.rsp')],expected=1);assert 'recursive' in err
    (d/'bad.ald').write_text('BOGUS(foobar)')
    _,err=run([BIN/'ArchLD','-T',d/'bad.ald'],expected=1);assert 'unknown linker directive' in err
    # same target catalogue, actual static ELF link through multiplexer
    run([BIN/'ArchTool','ld','-o',d/'mux',d/'entry.o',d/'run.o']);run([d/'mux'],expected=37)
print('PASS: DSLtk lexing, descriptions, object inspectors, response files, link manifests and linked ELF execution')
