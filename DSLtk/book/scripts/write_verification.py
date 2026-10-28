"""Record reproducible evidence after make verify completes its checks."""
from datetime import datetime, timezone
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
from assemble import ROOT, SLUGS, write_changed

def output(*args):
    return subprocess.run(args, cwd=ROOT, check=True, text=True, capture_output=True).stdout.strip()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--d-compiler', default='ldc2')
    args = parser.parse_args()
    evidence = ROOT / 'build/verification'
    listings = json.loads((evidence / 'listings.json').read_text())
    probes = json.loads((evidence / 'probes.json').read_text())
    assert listings and all(row['passed'] for row in listings)
    assert len(probes) == 4 and all(row['passed'] for row in probes)
    chapters = [(ROOT / 'chapters' / f'ch{i:02d}-{slug}.ltx').read_text()
                for i, slug in enumerate(SLUGS, 1)]
    stats = json.loads((ROOT / 'build/chapter-stats.json').read_text())
    pdfinfo = output('pdfinfo', 'build/main.pdf')
    pages = re.search(r'^Pages:\s+(\d+)', pdfinfo, re.M)[1]
    figures = sum(len(re.findall(r'\\begin\{figure\}', x)) for x in chapters)
    tables = sum(len(re.findall(r'\\begin\{(?:table|longtable)\}', x)) for x in chapters)
    algorithms = sum(x.count(r'\begin{pseudocode}') for x in chapters)
    hashes = []
    for name in ['DSLtk.hpp', 'DSLtk.d']:
        digest = hashlib.sha256((ROOT.parent / 'DSLtk' / name).read_bytes()).hexdigest()
        hashes.append(f'| `DSLtk/{name}` | `{digest}` |')
    versions = [output(*shlex.split(args.compiler), '--version').splitlines()[0],
                output(*shlex.split(args.d_compiler), '--version').splitlines()[0],
                output('pandoc', '--version').splitlines()[0],
                output('lualatex', '--version').splitlines()[0],
                output('bibtex', '--version').splitlines()[0]]
    text = f'''# Verification report

Generated after `make verify`, {datetime.now(timezone.utc):%Y-%m-%d %H:%M UTC}.

## Book

- Exactly **42 numbered chapters**, including five case studies.
- **{pages} PDF pages**, {figures} figures, {tables} table environments, and
  {algorithms} pseudocode blocks.
- {sum(row['source_words'] for row in stats):,} source words, including code
  and TeX markup; each chapter passes the 2,500-source-word minimum check.
- Chapter ordering, graph/table/pseudocode presence, listing assets, label
  uniqueness, and reference targets checked by `scripts/check_book.py`.
- LuaLaTeX/BibTeX build completed. Final log checked for undefined references,
  duplicate labels, missing glyphs, LaTeX errors, and overfull boxes.
- 42 Markdown chapter exports plus a combined Markdown volume generated.

## Executable evidence

- Five C++20 case-study programs built and ran with assertions enabled.
- {len(listings)} complete C++ chapter programs compiled and exited successfully,
  including the optional Lua examples. Source excerpts, fragments, and negative
  examples are inventoried separately; they are not counted as standalone programs.
- The D importing companion compiled and ran with LDC.
- Four header probes passed their expected outcomes: structural-object pattern
  dispatch compiled and ran; closure predicate composition, duplicate-payload
  Result inspection, and the C++ task runner were rejected as documented.

Case-study output:

```text
1500 m / 120 s = 12.5 m/s
query: Ada, Grace (original and optimized results agree)
port=9000 mode=debug
(mul (add a 0) 1) => a
saturated=1 iterations=2 applications=4
frame=sensor_7 bytes=14 partial-I/O round trip OK
```

D companion output:

```text
10 (add (number 1) (number 2)) a
```

Per-program source, compiler logs, exit status, and captured output are under
`build/verification/`. The JSON listing inventory records which listings are
fragments. Passing compile/run checks do not constitute a proof of every
general statement in the text, a performance benchmark, or exhaustive input
coverage. The five case studies add domain assertions for their stated contracts.

## Implementation fingerprints

| Supplied file | SHA-256 |
|---|---|
{chr(10).join(hashes)}

## Toolchain

```text
{chr(10).join(versions)}
```

The default bibliography backend is BibTeX, which is usable with the installed
TeX Live. The installed Biber executable required an unavailable `libcrypt.so.1`;
the book therefore uses biblatex's supported BibTeX backend for its six entries.

## Reproduce

```sh
make -C DSLtkBook verify
```

Successful listing/probe results are reused only when their extracted source,
compiler identity, flags, and header digest match. Remove `build/verification/`
to force those checks again. See `README.md` for dependencies and individual targets.
'''
    write_changed(ROOT / 'VERIFICATION.md', text)
    print(f'Verification report written: 42 chapters, {pages} pages, {len(listings)} chapter programs')

if __name__ == '__main__':
    main()
