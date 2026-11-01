#!/usr/bin/env python3
"""Check the catalog's present schema constraints without external packages."""
import json,re
from pathlib import Path
r=Path(__file__).resolve().parents[1]
schema=json.loads((r/'docs/machine.schema.json').read_text())
manifest=json.loads((r/'MANIFEST.yaml').read_text())
profiles=manifest['machines'];names=set();symbols=set()
for profile in profiles:
 name=profile['name'];d=r/'VMLibrary'/name
 stored=json.loads((d/(name+'.vmspc')).read_text())
 if stored!=profile:raise ValueError('manifest/profile mismatch: '+name)
 if set(stored)!=set(schema['required']):raise ValueError('profile fields: '+name)
 for key,rule in schema['properties'].items():
  value=stored[key]
  if 'const' in rule and value!=rule['const']:raise ValueError(key)
  if 'enum' in rule and value not in rule['enum']:raise ValueError(key)
  t=rule.get('type')
  if t=='string' and not isinstance(value,str):raise ValueError(key)
  if t=='boolean' and not isinstance(value,bool):raise ValueError(key)
  if t=='array' and (not isinstance(value,list) or not all(isinstance(v,str) for v in value)):raise ValueError(key)
  if 'minLength' in rule and len(value)<rule['minLength']:raise ValueError(key)
  if 'minItems' in rule and len(value)<rule['minItems']:raise ValueError(key)
  if 'pattern' in rule and not re.fullmatch(rule['pattern'],value):raise ValueError(key)
 if name in names or stored['symbol'] in symbols:raise ValueError('duplicate identity')
 names.add(name);symbols.add(stored['symbol'])
 for key in ['portable_example','vmweave','vmspc','vmdesc']:
  if not (d/stored[key]).is_file():raise ValueError('missing '+key)
 for suffix in ['.c','.h','.cpp','.hpp']:
  if not (d/(name+suffix)).is_file():raise ValueError('missing adapter: '+name+suffix)
expected={p.name for p in (r/'VMLibrary').iterdir() if p.is_dir()}
if expected!=names:raise ValueError('unindexed machine directory')
print(f'{len(profiles)} catalog specifications and file references validated')
