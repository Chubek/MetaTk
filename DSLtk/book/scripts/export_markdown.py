"""Export the typeset chapter sources as a readable Markdown edition.

The PDF is the illustrated edition. Diagram sources are retained in fenced
blocks; tables, listings, pseudocode, and chapter prose are converted by Pandoc.
"""
import re
import subprocess
from assemble import ROOT, SLUGS, write_changed

LINKS = {}
TOKENS = {}

def token(value):
    key = f'BOOKMARKUPTOKEN{len(TOKENS)}END'
    TOKENS[key] = value
    return key

def anchor(label):
    return token(f'<a id="{label}"></a>')

def reference(match):
    macro, label = match.groups()
    stem, number = LINKS.get(label, ('README', label))
    prefix = {'chapref':'Chapter ', 'secref':'Section ', 'figref':'Figure ',
              'tabref':'Table ', 'lstref':'Listing '}.get(macro, '')
    return token(f'[{prefix}{number}]({stem}.md#{label})')

def prepare(text):
    def tt(value):
        escapes = {'\\': r'\textbackslash{}', '_': r'\_', '&': r'\&',
                   '#': r'\#', '$': r'\$', '%': r'\%', '{': r'\{',
                   '}': r'\}', '^': r'\textasciicircum{}', '~': r'\textasciitilde{}'}
        return '\\texttt{' + ''.join(escapes.get(ch, ch) for ch in value) + '}'
    # Keep raw listings out of the prose transformations.
    protected = []
    def protect(match):
        protected.append(match[0])
        label = re.search(r'\blabel=(?:\{([^}]+)\}|([^\s,\]]+))', match[0].split(']\n', 1)[0])
        before = anchor(label[1] or label[2]) + '\n\n' if label else ''
        return before + f'BOOKLISTINGPLACEHOLDER{len(protected)-1}END'
    text = re.sub(r'\\begin\{lstlisting\}.*?\\end\{lstlisting\}', protect, text, flags=re.S)
    text = re.sub(r'\\label\{([^}]+)\}', lambda m: anchor(m[1]), text)
    text = re.sub(r'\\(ref|chapref|secref|figref|tabref|lstref|cref|Cref)\{([^}]+)\}', reference, text)
    def include(match):
        source = (ROOT / match[1]).read_text()
        language = 'D' if match[1].endswith('.d') else 'C++'
        return '\\begin{lstlisting}[language=' + language + ']\n' + source + '\\end{lstlisting}'
    text = re.sub(r'\\lstinputlisting(?:\[[^\n]*\])?\{([^}]+)\}', include, text)
    # Pandoc understands texttt but not the book's detokenizing helpers.
    text = re.sub(r'\\(?:code|api)\{((?:[^{}]|\{[^{}]*\})*)\}', lambda m: tt(m[1]), text)
    text = re.sub(r'\\(?:providecommand|newcommand)\{\\codel\}.*\n', '', text)
    text = re.sub(r'\\codel\{([^{}]*)\}\{([^{}]*)\}',
                  lambda m: tt(m[1] + m[2]), text)
    for macro, value in {'DSLtk':'DSLtk','cpp':'C++','dlang':'D','cstd':'C++20',
                         'dsltk':'DSLtk','dslns':'dsl::'}.items():
        text = re.sub(r'\\' + macro + r'\b(?:\{\})?', lambda _: value, text)
    text = re.sub(r'\\begin\{pseudocode\}(?:\[([^]]*)\])?(.*?)\\end\{pseudocode\}',
                  lambda m: '\\begin{quote}\\textbf{' + (m[1] or 'Pseudocode') + '}\\par\n' +
                  m[2] + '\\end{quote}', text, flags=re.S)
    for box in ['notebox', 'warnbox', 'tipbox', 'featurebox', 'csbox']:
        text = re.sub(r'\\begin\{' + box + r'\}(?:\[([^]]*)\])?',
                      lambda m: '\\begin{quote}\\textbf{' + (m[1] or 'Note') + '}\\par ', text)
        text = text.replace('\\end{' + box + '}', r'\end{quote}')
    text = re.sub(r'\\begin\{(?:dslgraph|tikzpicture)\}.*?\\end\{(?:dslgraph|tikzpicture)\}',
                  lambda m: '\\begin{lstlisting}[language={}]\n' + m[0] +
                  '\n\\end{lstlisting}', text, flags=re.S)
    for index, block in enumerate(protected):
        text = text.replace(f'BOOKLISTINGPLACEHOLDER{index}END', block)
    return text

def main():
    output = ROOT / 'markdown'
    output.mkdir(exist_ok=True)
    index = ['# DSLtk: The Definitive Guide', '',
             '42 chapters. [Illustrated PDF](../build/main.pdf). '
             'Diagrams below retain their TikZ source; the PDF renders them.', '']
    combined = ['# DSLtk: The Definitive Guide\n']
    aux = ROOT / 'build/main.aux'
    numbers = dict(re.findall(r'\\newlabel\{([^}]+)\}\{\{([^}]+)\}', aux.read_text())) if aux.exists() else {}
    for number, slug in enumerate(SLUGS, 1):
        stem = f'ch{number:02d}-{slug}'
        text = (ROOT / 'chapters' / (stem + '.ltx')).read_text()
        for label in re.findall(r'\\label\{([^}]+)\}', text):
            LINKS[label] = (stem, numbers.get(label, str(number) if label.startswith('ch:') else label))
        for match in re.finditer(r'\blabel=(?:\{([^}]+)\}|([^\s,\]]+))', text):
            label = match[1] or match[2]
            LINKS[label] = (stem, numbers.get(label, label))
    for number, slug in enumerate(SLUGS, 1):
        stem = f'ch{number:02d}-{slug}'
        text = prepare((ROOT / 'chapters' / (stem + '.ltx')).read_text())
        conversion = subprocess.run(['pandoc', '-f', 'latex', '-t', 'gfm', '--wrap=auto'],
                                    input=text, text=True, capture_output=True)
        if conversion.returncode:
            raise SystemExit(stem + ': ' + conversion.stderr)
        result = conversion.stdout
        for key, value in TOKENS.items():
            result = result.replace(key, value)
        # A top-level LaTeX chapter maps to a level-one Markdown heading.
        write_changed(output / (stem + '.md'), result)
        title = next((line.lstrip('# ') for line in result.splitlines() if line.startswith('#')), slug)
        index.append(f'{number}. [{title}]({stem}.md)')
        combined.append(result)
    write_changed(output / 'README.md', '\n'.join(index) + '\n')
    write_changed(output / 'DSLtkBook.md', '\n\n'.join(combined))
    print('Exported 42 Markdown chapters and markdown/DSLtkBook.md')

if __name__ == '__main__':
    main()
