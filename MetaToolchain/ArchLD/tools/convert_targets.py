#!/usr/bin/env python3
"""Generate standalone JSON ArchBFD catalogues from the unmodified YAML data.
Requires PyYAML *only for regeneration*; linking/building has no Python deps.
Also adds modern GOTPCRELX howtos absent from the historical binutils snapshot.
"""
import pathlib,json
try: import yaml
except ImportError as e: raise SystemExit('pip install pyyaml to regenerate targets') from e
root=pathlib.Path(__file__).resolve().parents[1]/'targets'
for f in sorted(root.glob('*.bfd.yaml')):
 d=yaml.safe_load(f.read_text())
 if f.name=='ELF64.bfd.yaml':
  for n,name in [(41,'R_X86_64_GOTPCRELX'),(42,'R_X86_64_REX_GOTPCRELX')]:
   if not any(x['number']==n for x in d['relocations']):
    item=next(x for x in d['relocations'] if x['number']==9).copy()
    item.update(number=n,name=name,desc='Unrelaxed GOTPCRELX: preserve instruction, use GOT entry')
    d['relocations'].append(item)
 (f.with_suffix('.json')).write_text(json.dumps(d,indent=2)+'\n')
# The catalogue ships already converted; original catalogue is copied with the suite.
print('Generated',len(list(root.glob('*.bfd.json'))),'descriptions')
