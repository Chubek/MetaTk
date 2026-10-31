from pathlib import Path
import re, subprocess, sys
root=Path(__file__).resolve().parents[1]
pdf=root/'build/main.pdf'
if not pdf.exists(): pdf=root/'DSLtk-and-MetaML.pdf'
info=subprocess.check_output(['pdfinfo',str(pdf)],text=True)
pages=int(re.search(r'^Pages:\s+(\d+)',info,re.M)[1])
assert pages==300, f'Expected 300 physical pages, found {pages}'
log=root/'build/main.log'
if log.exists():
 text=log.read_text(errors='replace')
 assert 'Overfull \\hbox' not in text and 'Overfull \\vbox' not in text, 'Layout overflow'
 assert 'There were undefined references' not in text, 'Undefined references'
 assert 'Rerun to get' not in text, 'Unresolved references; rerun TeX'
chapters=list((root/'chapters').glob('ch*.tex'))
assert len(chapters)==24
for language,extension in [('cpp','cpp'),('d','d')]:
 assert len(list((root/'examples'/language).glob('*.'+extension)))==120
print('PASS: 300 pages; 24 chapters; 240 workshop programs; no reported layout overflow')
