assert(BOOK_ROOT, 'BOOK_ROOT must identify the book source directory')
BOOK_FORMAT = BOOK_FORMAT or 'pdf'
book = {chapter_number = 0, counts = {}, seen = {}}
book.titles = {
    'A language for building languages',
    'Building MetaML and working at the prompt',
    'Source text, tokens, and layout',
    'Values, arithmetic, and evaluation',
    'Bindings, scope, and program units',
    'Functions, closures, and application',
    'Tacit programming with numbered parameters',
    'Sections, pipelines, and composition',
    'Types, inference, and unification',
    'Local definitions and recursion',
    'Tuples, lists, and persistent data',
    'Comprehensions and collection algorithms',
    'Records and row polymorphism',
    'Patterns, guards, and total functions',
    'Algebraic data and domain models',
    'Structures and module initialization',
    'Signatures and abstraction',
    'Functors and reusable architecture',
    'Imports, library discovery, and project layout',
    'Diagnostics, recovery, and executable specifications',
    'Core and the numeric libraries',
    'Strings, bytes, and Unicode',
    'Option and Result pipelines',
    'IO, buffers, and mutable tables',
    'Structured data and interchange',
    'Graph algorithms and dependency analysis',
    'Abstract syntax trees and visitors',
    'Lexers and grammar bridges',
    'Parser combinators in MetaML',
    'Introspection and language tooling',
    'A complete expression-language toolchain',
    'Embedding MetaML in C++',
    'Embedding in D and through the C ABI',
    'Capabilities and runtime architecture',
    'DSLtk.hpp: composition and feature mixins',
    'DSLtk.hpp: sources, parsers, and diagnostics',
    'DSLtk.hpp: trees, visitors, and rewriting',
    'DSLtk.hpp: graphs, terms, and equality saturation',
    'DSLtk.hpp: codecs, streams, preprocessing, and grammars',
    'DSLtk.hpp: the reusable REPL',
    'Testing, parity, and performance engineering',
    'Reference, glossary, and publishing the book',
}
local common = dofile(BOOK_ROOT .. '/luaROFF/rofflets/common.lua')
for _, name in ipairs({'syntax-highlight', 'graph', 'pseudo-code', 'formal-grammar',
    'math-notation', 'memo', 'symbol', 'uml'}) do
    dofile(BOOK_ROOT .. '/luaROFF/rofflets/' .. name .. '.rofflet')
end
function book.chapter(number, title)
    assert(number == book.chapter_number + 1, 'chapters must appear exactly once in order')
    assert(title == book.titles[number], 'chapter title differs from contents')
    book.chapter_number, book.counts = number, {}
    book.seen[number] = true
    lroff.emitln('.bp\n.sh 1 "' .. common.escape(title) .. '"')
    lroff.emitln('.(x')
    common.line(number .. '. ' .. title)
    lroff.emitln('.)x\n.pp')
end
function book.contents()
    lroff.emitln('.bp\n.uh "Reading map"\n.pp')
    common.line('Chapters 1-20 teach the language; 21-30 explore its libraries; '
        .. '31 builds a complete toolchain; 32-34 cover embedding; 35-40 explain DSLtk.hpp; '
        .. '41-42 collect engineering practice and reference material. Page references are in '
        .. 'the detailed contents at the end of the volume.')
    for i, title in ipairs(book.titles) do
        lroff.emitln('.ip "' .. i .. '." 4')
        common.line(title)
    end
end
function book.finish()
    assert(book.chapter_number == 42, 'expected all 42 chapters')
    lroff.emitln('.bp\n.uh "Detailed contents"\n.xp')
end
