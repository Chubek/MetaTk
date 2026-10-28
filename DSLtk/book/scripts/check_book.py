"""Check chapter structure, references, assets, and optionally PDF diagnostics."""
import argparse
import collections
import json
import re
import subprocess
from assemble import ROOT, SLUGS

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pdf', action='store_true')
    args = parser.parse_args()
    errors, stats, labels, references = [], [], [], []
    master = (ROOT / 'main.ltx').read_text()
    files = re.findall(r'\\input\{(chapters/[^}]+)\}', master)
    expected = [f'chapters/ch{i:02d}-{slug}.ltx' for i, slug in enumerate(SLUGS, 1)]
    if files != expected:
        errors.append('Master must include all 42 chapters once, in order, with .ltx extensions')
    for index, name in enumerate(expected, 1):
        path = ROOT / name
        if not path.exists():
            errors.append(f'Missing {name}')
            continue
        text = path.read_text()
        if len(re.findall(r'\\chapter\{', text)) != 1:
            errors.append(f'{name}: expected one numbered chapter')
        for label, pattern in [
            ('graph', r'\\begin\{(?:dslgraph|tikzpicture)\}'),
            ('table', r'\\begin\{(?:table|longtable)\}'),
            ('pseudocode', r'\\begin\{pseudocode\}')]:
            if not re.search(pattern, text):
                errors.append(f'{name}: missing {label}')
        prose = re.sub(r'\\begin\{lstlisting\}.*?\\end\{lstlisting\}', '', text, flags=re.S)
        labels.extend(re.findall(r'\\label\{([^}]+)\}', text))
        options = '\n'.join(re.findall(r'\\(?:begin\{lstlisting\}|lstinputlisting)\[(.*?)\]\s*\n', text, re.S))
        labels.extend(re.findall(r'\blabel=\{([^}]+)\}', options))
        labels.extend(re.findall(r'\blabel=([^\s{},\]]+)', options))
        references.extend(re.findall(r'\\(?:ref|chapref|secref|figref|tabref|lstref|cref|Cref)\{([^}]+)\}', prose))
        for asset in re.findall(r'\\lstinputlisting(?:\[[^\n]*\])?\{([^}]+)\}', text):
            if not (ROOT / asset).is_file():
                errors.append(f'{name}: missing listing {asset}')
        words = len(text.split())
        if words < 2500:
            errors.append(f'{name}: only {words} source words; expand the chapter')
        stats.append({'chapter': index, 'path': name, 'source_words': words})
    for label, count in collections.Counter(labels).items():
        if count != 1:
            errors.append(f'Duplicate label: {label}')
    known = set(labels)
    for group in references:
        for label in group.split(','):
            if label.strip() not in known:
                errors.append(f'Undefined reference: {label}')
    if args.pdf:
        log = ROOT / 'build/main.log'
        if not log.exists():
            errors.append('Missing PDF build log')
        else:
            text = log.read_text()
            for pattern in ['undefined references', 'undefined citations', 'multiply defined', 'Missing character:',
                            'LaTeX Error:', 'Overfull \\hbox', 'Overfull \\vbox']:
                if pattern in text:
                    errors.append('PDF diagnostic: ' + pattern)
        pdf = ROOT / 'build/main.pdf'
        if not pdf.exists():
            errors.append('Missing build/main.pdf')
        else:
            subprocess.run(['pdftotext', str(pdf), str(ROOT / 'build/main.txt')], check=True)
            rendered = (ROOT / 'build/main.txt').read_text()
            chapter_numbers = re.findall(r'(?m)^Chapter (\d+)\s*$', rendered.replace('\f', '\n'))
            if chapter_numbers != [str(n) for n in range(1, 43)]:
                errors.append('PDF text does not contain exactly the 42 ordered chapter headings')
    (ROOT / 'build').mkdir(exist_ok=True)
    (ROOT / 'build/chapter-stats.json').write_text(json.dumps(stats, indent=2) + '\n')
    if errors:
        raise SystemExit('\n'.join(sorted(set(errors))))
    print('42 chapters, five case studies, chapter graphs/tables/pseudocode, labels and assets: OK')
    print(f'{sum(x["source_words"] for x in stats):,} source words (includes code and TeX markup)')

if __name__ == '__main__':
    main()
