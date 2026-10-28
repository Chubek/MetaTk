"""Reorganize attributed manual material and book-specific essays into chapters.

Chapters 2--36 are independently authored LaTeX and are preserved. Only the
seven Markdown-authored chapters are generated. No edits are made to DSLtk/.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MANUAL = ROOT.parent / 'DSLtk' / 'manual'
SLUGS = ['introduction','getting-started','architecture','crtp-foundation','fixed-string',
         'feature-tags','enum-reflection','pipeline','operators','pattern-match','pattern-regex',
         'astnode','leaf-and-node','traversal','rewrite','rewrite-sets','expression-templates',
         'custom-literals','memoization-and-lazy','maybe-and-result','parser-core',
         'primitive-parsers','parser-diagnostics','task-pipeline','peg-definition',
         'peg-combinators','peg-semantic-values','sexpressions','egraphs','codecs-and-io',
         'visitors','static-regex','cfg-earley','preprocessor','directed-graphs',
         'terms-and-reduction','d-toolkit','case-units','case-query','case-config',
         'case-saturation','case-binary']
TITLES = ['Introduction: The Problem of Language Construction','Getting Started: Build, Install, First Programs',
          'Architecture and the Feature Catalogue','The CRTP Foundation','FixedString: Compile-Time Strings',
          'Feature Tags, Mixins, and Concepts','Enum Reflection and the Embedded magic_enum',
          'Pipelines and Data Flow','Predicate Composition and the Operators Feature',
          'Compile-Time Dispatch Tables','The Small pattern Matcher','ASTNode: The Value-Type Tree',
          'Building Trees with leaf and node','Traversal, Dumping, and Tree Utilities',
          'Rewrite Rules and Predicates','Rewrite Sets and Fixed Points',
          'Expression Templates and Evaluation','Custom Literals and Suffix Resolution',
          'Memoization and Lazy Values','Maybe, Optional Helpers, and Result',
          'The Parser Core','Primitive Parsers and Combinators','Diagnostics and Semantic Actions',
          'Task Pipelines and Explicit State','PEG Definitions, Rules, and Channels',
          'PEG Combinators and Streaming Matching','PEG Semantic Values and AST Construction',
          'S-Expressions: Reading, Printing, and Rule Notation','E-Graphs and Equality Saturation',
          'User Codecs and Abstract Byte I/O','AST-Derived Visitors',
          'Static Byte Regular Expressions and Binary Streams','Context-Free Grammars and Earley Parsing',
          'The Preprocessor and Optional Lua','Directed Graphs, Traversal, and DOT',
          'First-Order Terms, Unification, and Reduction','The D Toolkit: Mixins, CTFE, and Reflection',
          'Case Study I: A Units and Physics DSL','Case Study II: A Query DSL',
          'Case Study III: A PEG Configuration Language','Case Study IV: An Equality-Saturation Optimizer',
          'Case Study V: A Binary Protocol Toolchain']
MAP = {1:[1],4:[3],5:[4],6:[5],7:[29],8:[6],9:[7],10:[8],11:[9],12:[10],13:[11],
       14:[12],15:[13],16:[14],17:[15,16],18:[17],19:[18,19],20:[20,21,22],21:[23],
       22:[24],23:[25],25:[27],26:[28],28:[32],29:[32],30:[33],31:[34],32:[33],
       33:[36],34:[35],35:[37],36:[37],37:[31]}
OLD_TO_NEW = {1:1,2:2,3:4,4:5,5:6,6:8,7:9,8:10,9:11,10:12,11:13,12:14,
              13:15,14:16,15:17,16:17,17:18,18:19,19:19,20:20,21:20,22:20,
              23:21,24:22,25:23,26:24,27:25,28:26,29:7,30:3,31:37,
              32:29,33:30,34:31,35:34,36:33,37:36}

def adapt(text, chapter):
    text = '\n'.join(text.splitlines()[1:]).strip()
    # Correct the type/value distinction in auto NTTP dispatch examples.
    text = re.sub(r'(when\s*<\s*dsl::pattern<"[^"\n]*">)(\s*>)',r'\1{}\2',text)
    text = text.replace('at worst quadratic in pathological backtracking cases involving `*`',
                        'potentially combinatorial when several overlapping stars retry failing suffixes')
    text = text.replace('a `==` returning `bool`', 'public structural subobjects')
    text = text.replace('there is no virtual call anywhere in the\ntoolkit',
                        'the CRTP base does not introduce virtual calls; other facilities use type-erased callables')
    text = text.replace('Every feature injects', 'A nonempty feature mixin injects')
    text = text.replace('no run-time cost to the dispatch, because the `if constexpr` discards the unused branch at compile time',
                        'no runtime branch for selecting the matching protocol; actual key tests and handler selection still occur at runtime')
    text = text.replace('This makes `Result` cheap to copy, move, and return by value',
                        'The cost of copying and moving a Result depends on its active payload')
    text = text.replace('It never throws, and it moves the fallback into the return value when the error branch is active.',
                        'It does not deliberately throw for an error state; payload copying or moving may still throw.')
    # Avoid publishing invalid guarantees about the snapshot's Result type.
    text = text.replace('This design keeps the common case ergonomic while preventing ambiguity in the degenerate case.',
                        'Construction alone can work for identical payload types, but querying such a result fails to instantiate: holds_alternative and get are type-based. Use distinct payload types in this snapshot.')
    text = text.replace('They are most useful in generic code and in assignments where the target type already fixes `T` and `E`.',
                        'In this C++ snapshot they do not convert to Result; they are independent wrappers. The D module supplies the corresponding tagged constructors.')
    text = re.sub(r'\[([^\]]+)\]\(ch(\d\d)-[^)]+\.md\)',
                  lambda m: m[1] + ' (book Chapter ' + str(OLD_TO_NEW.get(int(m[2]),int(m[2]))) + ')', text)
    text = re.sub(r'(?<!book )Chapter (\d+)',
                  lambda m: 'Chapter ' + str(OLD_TO_NEW.get(int(m[1]),int(m[1]))),text)
    # Old manual's internal numbering must not masquerade as book numbering.
    text = re.sub(r'^(#{2,6})\s+\d+(?:\.\d+)*\.?\s+',r'\1 ',text,flags=re.M)
    if chapter == 28: text = text.split('## Declare equality rules')[0] if '## Declare equality rules' in text else text.split('## 3. Declare equality rules')[0]
    if chapter == 29:
        at = text.find('## Declare equality rules')
        if at < 0: at = text.find('## 3. Declare equality rules')
        text = text[at:]
    if chapter == 30: text = text.split('## Static byte regular expressions')[0]
    if chapter == 32: text = text[text.index('## Static byte regular expressions'):]
    if chapter == 35: text = text.split('## First-order terms')[0]
    if chapter == 36: text = text[text.index('## First-order terms'):].split('## Verification')[0]
    if chapter == 37:
        text = text.replace('and the eight shared facility tags from Chapter 3',
                            'and the later shared facility tags catalogued in Chapter 3')
        text = text.replace('The shared facilities introduced in Chapter 3 have focused tutorials in Chapters\n32–36.',
                            'The shared facilities have focused tutorials in book Chapters 28–36.')
        text = text.replace('byte-NFA API is `StaticRegex!S` in Chapter 30',
                            'byte-NFA API is `StaticRegex!S` in book Chapter 32')
    # Explicitly classify inherited programs; chapter essays provide snapshot contracts.
    return text

def main():
    (ROOT/'manuscript').mkdir(exist_ok=True)
    for chapter, slug in enumerate(SLUGS,1):
        if 2 <= chapter <= 36: continue
        content = []
        essay = ROOT/'essays'/f'ch{chapter:02d}.md'
        if essay.exists(): content.append(essay.read_text())
        for source in ([31] if chapter == 37 else []):
            filename = next(MANUAL.glob(f'ch{source:02d}-*.md'))
            content.append(adapt(filename.read_text(),chapter))
        if not content: raise SystemExit(f'missing chapter {chapter}')
        source = '# '+TITLES[chapter-1]+'\n\n'+'\n\n'.join(content)
        source += '\n\n' + atlas(chapter)
        md = ROOT/'manuscript'/f'ch{chapter:02d}-{slug}.md'
        write_changed(md, source)
        result = subprocess.run(['pandoc','-f','markdown','-t','latex',
             '--top-level-division=chapter','--syntax-highlighting=none',
             '--lua-filter',str(ROOT/'scripts/listings.lua'),str(md)],
             check=True,capture_output=True,text=True).stdout
        # Namespace Pandoc-generated heading anchors, preserving explicit labels.
        result = re.sub(r'\\label\{([^}:]+)\}',lambda m: '\\label{c'+str(chapter)+'-'+m[1]+'}',result)
        result = re.sub(r'(\\chapter\{.*?\})',r'\1\n\\label{ch:'+slug+'}',result,count=1,flags=re.S)
        write_changed(ROOT/'chapters'/f'ch{chapter:02d}-{slug}.ltx', result)
    print('Assembled 7 chapters; preserved independently authored Chapters 2–36.')

def write_changed(path, text):
    if not path.exists() or path.read_text() != text:
        path.write_text(text)

def atlas(chapter):
    # Data-driven educational diagrams supplied in the separate source file.
    import json
    data = json.loads((ROOT/'scripts/atlas.json').read_text())[str(chapter)]
    nodes = data['nodes']
    drawing = []
    for i,name in enumerate(nodes):
        where = '' if i==0 else f', right=5mm of n{i-1}'
        drawing.append(f'\\node[dslsmall{where}] (n{i}) {{{name}}};')
        if i: drawing.append(f'\\draw[dsledge] (n{i-1}) -- (n{i});')
    rows = '\n'.join(a+' & '+b+r' \\' for a,b in data['rows'])
    steps = '\\\\\n'.join('\\textbf{'+str(i+1)+'.} '+s for i,s in enumerate(data['steps']))
    return r'''## Implementation map and review

The following diagram summarizes the chapter's central dependency or state
transition. Arrows denote logical dependence, not elapsed time or measured
performance. The contracts below separate what the library provides from the
conditions that the application must establish.

```{=latex}
\begin{figure}[htbp]
\centering
\begin{tikzpicture}
'''+'\n'.join(drawing)+r'''
\end{tikzpicture}
\caption{'''+data['caption']+r'''}
\label{fig:atlas-'''+str(chapter)+r'''}
\end{figure}
\begin{table}[htbp]
\centering\small
\begin{tabular}{L{0.28\linewidth}L{0.61\linewidth}}
\toprule Contract & Interpretation \\
\midrule
'''+rows+r'''
\bottomrule
\end{tabular}
\caption{Implementation contracts for this chapter.}
\label{tab:atlas-'''+str(chapter)+r'''}
\end{table}
\begin{pseudocode}[Operational summary]
'''+steps+r'''
\end{pseudocode}
```

### Review questions

'''+ '\n\n'.join(str(i+1)+'. '+s for i,s in enumerate(data['questions']))+'\n'

if __name__ == '__main__': main()
