#!/usr/bin/env python3
"""Regenerate declarative catalog files. Never overwrites native implementations.
The .mvm files are portable Machinery examples, not native guest images.
"""
from pathlib import Path
import json,re
ROOT=Path(__file__).resolve().parents[1]
ARCH={
'CHIP-8':('register','Classic CHIP-8; legacy, COSMAC VIP and CHIP-48 opcode presets; 4 KiB memory and 64x32 display','classic-opcode-interpreter'),
'SSAVM':('ssa','Bounded i64 ISSA subset; opt-in definite-assignment/type analysis; CEN selection; simultaneous PHI/PFE; persistent arrays','issa-subset'),
'DRMVM':('graphics','Machinery host imports into XRGB8888 surfaces; optional libdrm dumb buffers','graphics-subset'),
'FORTH':('stack','Data and return stacks, dictionary, colon definitions','integer-word-subset'),
'JVM':('stack','JVM integer Code arrays with local variables and bounded operand stack','integer-method-subset'),
'SECD':('closure','Version-1 unary SECD subset; lexical closures, i64 arithmetic, typed call/selection dump','secd-unary-subset'),
'CESK':('closure','Control, environment, store, continuation','catalog-only'),
'Krivine':('closure','Version-1 closed de Bruijn terms; bounded call-by-name weak-head closure evaluation','krivine-whnf-subset'),
'CAM':('closure','Categorical abstract machine','catalog-only'),
'WAM':('logic','Warren abstract machine for logic programs','catalog-only'),
'WASM':('stack','WebAssembly typed instructions and linear memory','catalog-only'),
'CLR':('stack','Common intermediate language runtime','catalog-only'),
'Dalvik':('register','DEX register instructions','catalog-only'),
'ART':('runtime','Android runtime consuming DEX; not a separate guest instruction set','catalog-only'),
'AVM2':('stack','ActionScript bytecode virtual machine','catalog-only'),
'DVI':('document','TeX device independent output command interpreter','catalog-only'),
'G-Code':('motion','Modal machine control command interpreter; dialect must be selected','catalog-only'),
'PostScript':('stack','PostScript operand, dictionary and execution stacks','catalog-only'),
'LuaVM':('register','Lua VM; bytecode version must be pinned','catalog-only'),
'BEAM':('register','Erlang instruction runtime with process scheduling','catalog-only'),
'CPython':('stack','Python bytecode; version must be pinned','catalog-only'),
'YARV':('stack','Ruby instruction sequences; version must be pinned','catalog-only'),
'V8':('runtime','JavaScript engine; no stable external bytecode ABI','catalog-only'),
'JSCore':('runtime','JavaScriptCore runtime; internal bytecode must be versioned','catalog-only'),
'Self':('object','Prototype-based object language runtime','catalog-only'),
'Smaltalk-80':('object','Original directory spelling retained; Smalltalk-80 object machine','catalog-only'),
'SOM':('object','Simple object machine','catalog-only'),
'ParrotVM':('register','Parrot virtual machine','catalog-only'),
'Dis':('register','Inferno Dis virtual machine','catalog-only'),
'P-Code':('stack','Pascal p-code; dialect must be selected','catalog-only'),
'BCPL':('stack','BCPL abstract machine; dialect must be selected','catalog-only'),
'Dart':('runtime','Dart runtime; guest ABI requires a version','catalog-only'),
'MoscowML':('closure','Moscow ML runtime','catalog-only'),
'ConcurrentML':('runtime','Concurrent ML language and concurrency runtime; guest ISA unspecified','catalog-only'),
'SCUMM':('script','Versioned game scripting machine','catalog-only'),
}
def ident(name):return re.sub('[^A-Za-z0-9_]','_',name)
items=[]
for d in sorted((ROOT/'VMLibrary').iterdir()):
 if not d.is_dir():continue
 name=d.name;symbol=ident(name);arch,summary,coverage=ARCH.get(name,('unspecified','No authoritative ISA or version supplied; preserve this identity pending specification','catalog-only'))
 native={'CHIP-8':'Chip8.hpp','SSAVM':'ISSA.hpp','DRMVM':'Graphics.hpp','JVM':'IntegerMethod.hpp','FORTH':'Forth.hpp','SECD':'SECDMachine.hpp','Krivine':'KrivineMachine.hpp'}.get(name)
 spec={'schema':'metavm.machine/1','name':name,'symbol':symbol,'architecture':arch,'description':summary,'native_coverage':coverage,'native_format_loader':False,'portable_example':name+'.mvm','vmweave':name+'.vmweave','vmspc':name+'.vmspc','vmdesc':name+'.vmdesc','limitations':(['No native execution implemented. Descriptor and portable Machinery example only.'] if coverage=='catalog-only' else ['See native API header for exact implemented subset. No full platform runtime or file loader.'])}
 if name=='SSAVM':spec['limitations']=['Textual i64/scalar-array subset with configurable runtime and static-analysis limits; see docs/SSAVM.md.','Opt-in analysis checks defined operands, types and PHI selections; no conventional SSA dominance certificate, value-range/termination proof, calls, floats or SafeTSA loader.']
 if name=='CHIP-8':spec['limitations']=['Selected classic-opcode presets, sampled key waits and host-driven vblank; see docs/CHIP8.md.','Pinned Corax+ and flags ROM tests, not full hardware compatibility; no SCHIP/XO-CHIP, audio synthesis or cycle-accurate scheduling.']
 if name=='SECD':spec['limitations']=['MetaVM in-memory unary/i64 SECD profile v1; see docs/CLOSURE_VMS.md.','No Lispkit binary loader, lists, DUM/RAP, mutable recursive environments, I/O or GC; cumulative environment budgets apply.']
 if name=='Krivine':spec['limitations']=['MetaVM in-memory closed-term Krivine profile v1; see docs/CLOSURE_VMS.md.','Weak-head call-by-name only; no binary loader, full normalization, call-by-need memoization, control operators or GC.']
 items.append(spec)
 asm='.function main 0 3\nconst r0 6\nconst r1 7\nmul r2 r0 r1\nret r2\n.end\n'
 if name=='DRMVM':asm='.import drm_clear 1\n.import drm_rect 5\n.function main 0 7\nconst r0 0\nhost r6 drm_clear r0\nconst r0 1\nconst r1 1\nconst r2 2\nconst r3 2\nconst r4 16711680\nhost r6 drm_rect r0\nconst r6 42\nret r6\n.end\n'
 (d/(name+'.mvm')).write_text(asm)
 # VMWeave is real MetaML host embedding. No alternate parser for this syntax.
 (d/(name+'.vmweave')).write_text('-- Portable Machinery example for '+name+'; not native guest bytecode.\n'+'\n'.join('val _ := vm_emit '+json.dumps(line) for line in asm.splitlines())+'\n')
 (d/(name+'.vmspc')).write_text(json.dumps(spec,indent=2)+'\n')
 (d/(name+'.vmdesc')).write_text('\\VMName{'+name+'}\n\\VMArchitecture{'+summary+'}\n\\VMStatus{'+coverage+'}\n\\VMInvariant{Portable example bytecode is Machinery bytecode; it does not establish native compatibility.}\n')
 guard='METAVM_LIBRARY_'+symbol.upper()+'_H'
 (d/(name+'.h')).write_text(f'#ifndef {guard}\n#define {guard}\n#include "MetaVM.h"\n#ifdef __cplusplus\nextern "C" {{\n#endif\n/* Portable Machinery example, not a native guest loader. */\nmvm_status {symbol}_example_module(mvm_module **,mvm_error *);\n#ifdef __cplusplus\n}}\n#endif\n#endif\n')
 (d/(name+'.c')).write_text(f'/* Generated portable Machinery example. Regenerate with tools/generate_catalog.py. */\n#include "{name}.h"\nstatic const char source[] = '+json.dumps(asm)+f';\nmvm_status {symbol}_example_module(mvm_module **out,mvm_error *error) {{\n return mvm_compile(source,sizeof(source)-1,0,out,error);\n}}\n')
 (d/(name+'.hpp')).write_text('#pragma once\n#include "'+name+'.h"\n#include "../Library.hpp"\n'+('#include "'+native+'"\n' if native else '')+f'namespace metavm {{ namespace library {{ const Profile& {symbol}_profile(); }} }}\n')
 (d/(name+'.cpp')).write_text(f'#include "{name}.hpp"\nnamespace metavm {{ namespace library {{\nconst Profile& {symbol}_profile() {{\n static const Profile profile = {{'+','.join(json.dumps(v) for v in [name,arch,coverage,summary])+f',&{symbol}_example_module}};\n return profile;\n}}\n}} }}\n')
 (d/'Doxyfile.in').write_text(f'PROJECT_NAME = "MetaVM {name}"\nOUTPUT_DIRECTORY = "@CMAKE_CURRENT_BINARY_DIR@/docs"\nINPUT = "@CMAKE_CURRENT_SOURCE_DIR@"\nRECURSIVE = NO\nGENERATE_HTML = YES\nGENERATE_LATEX = NO\nQUIET = YES\n')
 (d/'CMakeLists.txt').write_text('# Build within MetaVM root; machinery target and native support are shared.\nif(NOT TARGET metavm_catalog)\n  message(FATAL_ERROR "Configure the MetaVM root directory")\nendif()\n')
# Fill old vmspecs with explicit compatibility adapters; no fake native loaders.
aliases={'Smalltalk-80':'Smaltalk-80','Postscript':'PostScript','Parrot':'ParrotVM'}
for d in sorted((ROOT/'vmspecs').iterdir()):
 if not d.is_dir():continue
 name=d.name;dest=aliases.get(name,name);symbol=ident(name);target=ident(dest)
 present=any(s['name']==dest for s in items)
 (d/(name+'.h')).write_text(f'#pragma once\n#include "MetaVM.h"\n#ifdef __cplusplus\nextern "C" {{\n#endif\nmvm_status legacy_{symbol}_example(mvm_module **,mvm_error *);\n#ifdef __cplusplus\n}}\n#endif\n')
 body=f'return {target}_example_module(out,error);' if present else '(void)error; if(out)*out=0; return MVM_NOT_FOUND;'
 (d/(name+'.c')).write_text(f'#include "{name}.h"\n'+(f'#include "../../VMLibrary/{dest}/{dest}.h"\n' if present else '')+f'mvm_status legacy_{symbol}_example(mvm_module **out,mvm_error *error) {{ {body} }}\n')
 (d/(name+'.vmwv')).write_text('# Legacy assembly companion; canonical MetaML is in VMLibrary.\n'+('.function main 0 1\nconst r0 42\nret r0\n.end\n' if present else '# No native specification supplied for this machine.\n'))
manifest={'schema':'metavm.catalog/1','machines':items,'paths':{'Machinery':'Existing C11 execution engine, module verifier, serialization, heap and host bridge','vmweave':'C bootstrap compiler/emitter and optional MetaML C++20 evaluator','VMDesc':'MetaTeX embedding adapter','VMLibrary':'Catalog profiles, portable examples and selected native interpreters','vmspecs':'Legacy explicitly named example adapters','third_party':'Supplied MetaML header and dependency provenance','tests':'Integration and semantic regression tests','AGENTS.md':'Implementation/refinement contract'}}
manifest['paths'].update({'driver':'Command-line runners; ssavm runs the implemented textual ISSA subset','history':'Exact prior source archive retained; do not delete'})
manifest['framework_layers']={
 'authoring': ['vmweave/Builder.hpp','vmweave/MetaMLAdapter.hpp'],
  'definition': ['vmweave/Definition.hpp','vmweave/examples/ToyR16.hpp','vmweave/examples/ToyR16.vmweave'],
 'execution': ['Machinery/MetaVM.c','Machinery/CBridge.h'],
 'documentation': ['VMDesc/MetaTeXAdapter.hpp'],
  'native_subsets': ['VMLibrary/SSAVM/ISSA.hpp','VMLibrary/CHIP-8/Chip8.hpp','VMLibrary/DRMVM/Graphics.hpp','VMLibrary/JVM/IntegerMethod.hpp','VMLibrary/FORTH/Forth.hpp','VMLibrary/SECD/SECDMachine.hpp','VMLibrary/Krivine/KrivineMachine.hpp']}
(ROOT/'MANIFEST.yaml').write_text(json.dumps(manifest,indent=2)+'\n')
(ROOT/'VMLibrary/Library.hpp').write_text('''#pragma once
#include "MetaVM.h"
#include <string_view>
#include <vector>
namespace metavm { namespace library {
struct Profile {
 const char *name,*architecture,*coverage,*description;
 mvm_status (*example_module)(mvm_module**,mvm_error*);
};
const std::vector<Profile>& catalog();
const Profile* find(std::string_view name);
}}
''')
(ROOT/'VMLibrary/Library.cpp').write_text('#include "Library.hpp"\n'+''.join(f'#include "{s["name"]}/{s["name"]}.hpp"\n' for s in items)+'namespace metavm { namespace library {\nconst std::vector<Profile>& catalog(){\n static const std::vector<Profile> profiles={\n'+',\n'.join(ident(s['name'])+'_profile()' for s in items)+'};return profiles;\n}\nconst Profile* find(std::string_view name){for(const auto&p:catalog())if(name==p.name)return &p;return nullptr;}\n}}\n')
(ROOT/'docs/COVERAGE.md').write_text('# Machine coverage\n\nPortable examples run on Machinery. Native support is separate. No machine has a complete external runtime/image loader.\n\n'+str(sum(s['native_coverage']!='catalog-only' for s in items))+' executable native subsets; '+str(sum(s['native_coverage']=='catalog-only' for s in items))+' catalog-only profiles. SECD and Krivine use explicit in-memory closure-machine profiles; see [CLOSURE_VMS.md](CLOSURE_VMS.md).\n\nThe SSAVM i64 textual subset and its configurable limits are specified in [SSAVM.md](SSAVM.md). The validated MetaML Definition embedding is described in [ROUND3.md](ROUND3.md). CHIP-8 presets and pinned ROM coverage are specified in [CHIP8.md](CHIP8.md); SDK packaging is described in [PACKAGING.md](PACKAGING.md).\n\n| Machine | Architecture | Native coverage |\n|---|---|---|\n'+''.join(f'| {s["name"]} | {s["architecture"]} | {s["native_coverage"]} |\n' for s in items))
print('Generated',len(items),'profiles')
if any(p.stat().st_size==0 for p in ROOT.rglob('*') if p.is_file()):raise SystemExit('Empty file remains')
