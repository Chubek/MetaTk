# Case Study III: A PEG Configuration Language

## The problem: configuration loading should be all or nothing

A configuration file is a small external language whose failures are often
more operationally important than its successful syntax. A program should
not install half a new configuration after encountering an invalid final
assignment. It should distinguish an unknown key from a malformed number,
reject duplicate definitions according to a stated policy, and preserve the
previous settings if a reload fails. This case study makes those obligations
explicit with a temporary settings object.

The language has assignments of the form `key = value;`. It supports two
keys: `port`, whose value is a decimal integer from one through 65535, and
`mode`, whose value is `debug` or `release`. Missing keys use defaults.
Whitespace can appear between tokens. An optional preprocessing stage permits
named constants such as a macro defining the port number.

DSLtk supplies preprocessing and an ordered PEG tokenizing definition. The
application copies matched spans into tokens, checks assignment structure,
performs key-specific validation, and returns a completed `Settings` value.
The tokenizer's patterns are not recursive productions; the assignment
grammar is deliberately implemented as a small second-stage parser.

## The accepted language and its defaults

An empty source is accepted and yields port 8080 with release mode. This is
an intentional defaults policy, not evidence that the tokenizer silently
ignored input. A nonempty source must tokenize completely and contain complete
four-token assignments. Unknown characters cause lexical failure; missing
punctuation or a misplaced token causes grammatical failure; invalid key
values cause semantic failure.

| Input | Outcome | Responsible stage |
|---|---|---|
| `port=9000;` | Set port | Semantic assignment |
| `mode=debug;` | Set mode | Semantic assignment |
| `port=0;` | Reject range | Semantic validation |
| `port=80` | Reject missing semicolon | Assignment parser |
| `other=1;` | Reject unknown key | Semantic validation |
| `port=@;` | Reject unknown character | PEG tokenizer |
| Two `port` assignments | Reject duplicate | Definition tracking |

The lexical word rule accepts letters or underscore initially, followed by
letters, digits, or underscore. The number rule accepts one or more decimal
digits. Leading signs and fractional numbers are outside the language.
Although the tokenizer recognizes many possible words, the semantic layer
accepts only the declared keys and mode values. A broad lexical vocabulary
does not imply an open configuration schema.

Preprocessing preserves comments rather than erasing them as a lexical
whitespace rule would. The case study's tokenizer does not register comment
patterns, so ordinary comment text left in the expanded source is not accepted
as configuration syntax. If comments are desired, add and specify them at
the token layer instead of assuming preprocessing automatically removes them.

## Preprocessing before token ownership

The loader first creates a local `Preprocessor` and processes the complete
source. A macro such as `PORT` can therefore expand to `9000` before token
recognition. The returned expanded string becomes the tokenizer's input owner.
The local grammar and its callbacks remain alive only while that string is
available, and each emitted token copies its matched text.

Copying prevents the token vector from borrowing the preprocessor's temporary
output. The token's offset still refers to expanded text. If a production
configuration tool needs diagnostics in the original file, it should retain
source mapping information or explain that errors refer to expanded input.
The small match record alone cannot reconstruct which macro or include
produced every byte.

A fresh preprocessor per load also isolates macro state between reloads.
Definitions from one configuration do not silently leak into another.
An application that wants shared predefined constants can establish them
deliberately before processing. Object lifetime becomes part of reproducibility:
using a persistent mutable preprocessor and using a fresh one are different
configuration-language policies.

## Token actions append observations, not settings

The grammar registers whitespace with the ignore channel and no callback.
Word, number, equals, and semicolon rules append a `Token` containing its
kind, owned text, and beginning offset. The small `emit` factory captures the
token kind by value and the local token vector by reference. It produces
uniform callbacks without repeating the ownership logic for every rule.

No callback updates the settings object. That is a deliberate transaction
boundary. A later lexical error can leave several tokens in the local vector,
but the loader discards them when it throws. If token callbacks directly
changed global configuration, those changes would survive a later failure
unless the application implemented rollback. Local token collection avoids
that problem at little conceptual cost.

The C++ rule-vector lifetime issue is also avoided. The whitespace rule is
configured immediately after insertion, and no rule reference is kept across
later `add_rule` calls. The grammar's parse driver obtains its rule view after
construction is complete. This is an example of using the supplied API safely
without modifying its underlying vector storage.

## Parsing fixed-shape assignments

The application parser advances through the token vector in groups of four.
Each group must contain a word key, an equals sign, a value token, and a
semicolon. The count check precedes indexing, so a short trailing group is
rejected without out-of-range access. The parser then delegates the value's
meaning to the selected key.

This simple parser is appropriate because the grammar has no nesting or
operator precedence. A recursive parser would add machinery without adding
meaning. If the language later supports lists or nested sections, the grammar
should be extended explicitly and the parser structure should change
accordingly. The PEG tokenizer can remain the lexical front end while the
second stage becomes recursive or uses the CFG facility.

A semicolon is required even at end of input. This gives every assignment
the same shape and avoids a special trailing rule. It also makes a missing
delimiter an unambiguous grammatical error rather than a heuristic guess.
The syntax is narrow enough that users can predict acceptance from a short
description, an important quality for configuration languages.

## Semantic validation and duplicate policy

For `port`, the loader requires a number token, performs full `from_chars`
conversion, checks the error code and end pointer, and enforces the allowed
range. A huge decimal spelling can pass the token regex but fail integer
conversion. The lexical and semantic checks therefore cover different failure
classes, just as they did in the units case study.

For `mode`, the loader requires a word token and one of two exact spellings.
Case is significant. Supporting case-insensitive modes would be a deliberate
normalization policy, perhaps using a small enum mapping, rather than an
accidental consequence of a permissive lexer. Unknown keys are rejected
instead of ignored, which helps users discover misspellings.

The `defined` set rejects a second assignment to the same key. Other languages
choose last-definition-wins or first-definition-wins, but those policies should
be stated because they change reload behavior and user expectations. Rejecting
duplicates makes this small configuration language easy to audit: each key
has either its default or one explicit assignment.

## The executable implementation

The source file `projects/config/main.cpp` is both the compiled companion
and the listing below. Build it with `make build/config`. Its assertions
exercise one successful macro-containing configuration and six rejection
cases spanning the lexical, grammatical, and semantic boundaries.

```{=latex}
\lstinputlisting[style=dslcpp,caption={Case study III: preprocessing, PEG tokenization, and transactional settings validation.},label={lst:case-config}]{projects/config/main.cpp}
```

The successful source defines `PORT` as 9000, assigns it to `port`, and sets
debug mode. The observed output is:

```text
port=9000 mode=debug
```

The program checks the returned fields rather than relying on the printed
string as its only evidence. The rejection loop verifies that every invalid
source throws. These cases are selected from the language contract: zero is
outside the allowed port range, an unknown mode is outside the enum-like
vocabulary, duplicate keys violate definition policy, and incomplete or
unrecognized syntax fails earlier stages.

## Publishing a reload safely

The loader builds a local `candidate` and returns it only after the complete
token sequence has been validated. A caller can therefore write an assignment
to its active settings only after `load` returns. If loading throws, the caller
never receives a replacement value and can retain the existing configuration.
This is a transaction at the object-publication boundary, not a rollback
mechanism inside every token action.

The same pattern scales to richer settings. Construct a candidate containing
all validated fields, perform cross-field checks, and publish it as one value.
Cross-field constraints might require a secure mode to use a particular port
range or prohibit two mutually exclusive options. Such checks belong after
individual assignments have been parsed, because they depend on the complete
candidate state.

For multithreaded applications, publication itself needs an appropriate
ownership strategy, such as swapping an immutable shared settings object.
The example does not provide concurrency control. Its contribution is a
clear point at which a complete validated value exists, making that later
application-level publication decision possible.

## Improving diagnostics without changing acceptance

The current exception messages are intentionally short. Each token already
stores an offset, so the next extension is to report the unexpected token
position, key name, and expected value class. A structured error record could
distinguish lexical, syntactic, and semantic categories. Formatting that
record for a terminal or editor should remain separate from deciding whether
the input is accepted.

The tokenizer's parse result supplies a location for an unrecognized byte.
The assignment parser can use the first token of an incomplete group or the
end of the source for a missing delimiter. The semantic validator can use
the value token's location for a range error. These are different sources
of diagnostic evidence; preserving them yields more useful messages than
wrapping every exception in a generic configuration failure.

## Lessons for a larger external DSL

The case study succeeds because it keeps four boundaries visible: source
transformation, token recognition, grammatical arrangement, and domain
validation. The code is small enough to read as one program, but the phases
are not collapsed into one undifferentiated callback. Each can be tested
independently and replaced when the language grows.

DSLtk's facilities are especially helpful when their contracts are respected.
The preprocessor supplies expanded text; the PEG definition supplies ordered
span recognition; the application supplies owned semantic tokens and settings.
No step silently provides the responsibilities of the next. This is the
central reusable pattern for external DSL construction in a compact toolkit.

## A staged failure trace

Consider loading `port=9000; mode=unknown;` while the running application
still uses port 8080 and release mode. Preprocessing returns ordinary
assignment text. Tokenization succeeds for the complete input, because every
word and punctuation mark belongs to the lexical vocabulary. The assignment
parser recognizes both four-token groups. The first assignment updates the
local candidate, and the second fails the permitted-mode check.

At that moment the candidate contains a newly assigned port, but it has
never been published. Stack unwinding destroys the candidate and token
vector. The active settings are untouched because they are owned by the
caller, which has not received a successful result. This trace explains why
the transaction is robust even though the local implementation mutates a
settings object during validation. Local mutation and externally visible
partial publication are different operations.

Now change the source to `port=9000; mode=@;`. The failure occurs earlier:
the tokenizer reaches the unrecognized byte after having appended several
tokens. No candidate needs to be interpreted. Those temporary tokens are
discarded, and the same publication guarantee holds. The two failures should
produce different diagnostics, but they share the same external state
contract. A carefully chosen boundary can unify recovery without flattening
all error information into one category.

Duplicate keys illustrate another timing question. The loader records that
a key has been defined as it processes assignments. A duplicate is rejected
even when both values are identical. This preserves the simple rule that a
source contains at most one assignment per key. If a future include system
wants overrides, it should state an ordering and provenance policy rather
than weakening duplicate detection silently. Configuration merging is a
language feature in its own right.

## Resource limits and source provenance

The executable uses short, trusted example strings. A loader for externally
supplied files should also define a source-size bound and the maximum amount
of preprocessing expansion it accepts. Token count is another useful bound:
a long source of tiny assignments can create many owned token strings even
when no individual value is large. These limits belong to the application's
resource contract, alongside the already explicit numeric port bound.

Preprocessing can change how locations are interpreted. If a macro expands
from four source characters into a longer decimal token, an offset in the
expanded buffer no longer identifies the same column in the original file.
An include resolver introduces a second dimension: which file supplied the
text? A production diagnostic model can carry source identifiers and expansion
spans, or it can deliberately report locations in an expanded-source view.
Either policy is clearer than displaying an original filename with an
unrelated expanded offset.

The optional include and scripting hooks also introduce state beyond the
candidate settings. If such a hook writes a file, makes a network request,
or mutates shared data, destroying the candidate cannot undo that effect.
The configuration example's all-or-nothing claim concerns publication of the
returned settings value. Applications that add effectful hooks must define
their own lifecycle for those effects. Keeping source transformation as a
pure or controlled input-producing phase makes the stronger overall behavior
easier to reason about.

## Schema evolution without accidental acceptance

Suppose a third key, `timeout`, is added. First decide its unit, range, and
default. Then decide whether its token is a bare number or a unit-bearing
spelling. A bare number is concise but needs a documented unit convention;
a value such as `250ms` is more explicit but requires a lexical extension
and a conversion policy. The units case study provides a useful model for
keeping whole-spelling validation separate from magnitude conversion.

Backward compatibility should be tested with complete old configurations.
Adding a defaulted key can preserve their meaning, whereas changing the
default of an existing key changes an empty or partial configuration even
when the grammar remains identical. This is why grammar compatibility is
not the whole compatibility story. Defaults, duplicate handling, macro
scope, and unknown-key behavior are all observable language semantics.

For verification, retain the small failure matrix and extend it with
cross-field cases. A configuration containing individually valid values may
still violate a relationship between fields. Validate those relationships
after all assignments have been read, so the result is independent of key
order. Then test that a failed reload leaves the previous active value
available. The combination of staged diagnostics and one publication point
provides a maintainable path from this two-key example to a substantially
larger configuration language.


## Implementation map and review

The following diagram summarizes the chapter's central dependency or state
transition. Arrows denote logical dependence, not elapsed time or measured
performance. The contracts below separate what the library provides from the
conditions that the application must establish.

```{=latex}
\begin{figure}[htbp]
\centering
\begin{tikzpicture}
\node[dslsmall] (n0) {Preprocess};
\node[dslsmall, right=5mm of n0] (n1) {PEG tokens};
\draw[dsledge] (n0) -- (n1);
\node[dslsmall, right=5mm of n1] (n2) {Validate settings};
\draw[dsledge] (n1) -- (n2);
\node[dslsmall, right=5mm of n2] (n3) {Commit};
\draw[dsledge] (n2) -- (n3);
\end{tikzpicture}
\caption{Case study III publishes a configuration only after complete validation.}
\label{fig:atlas-40}
\end{figure}
\begin{table}[htbp]
\centering\small
\begin{tabular}{L{0.28\linewidth}L{0.61\linewidth}}
\toprule Contract & Interpretation \\
\midrule
Tokens & Matched text is copied into owned token storage. \\
Duplicates & Repeated keys are rejected. \\
Commit & A temporary settings object is returned only on success. \\
\bottomrule
\end{tabular}
\caption{Implementation contracts for this chapter.}
\label{tab:atlas-40}
\end{table}
\begin{pseudocode}[Operational summary]
\textbf{1.} Expand configuration macros.\\
\textbf{2.} Tokenize the entire expanded source.\\
\textbf{3.} Require key, equals, value, semicolon groups.\\
\textbf{4.} Validate key-specific types and ranges.\\
\textbf{5.} Publish the completed settings object.
\end{pseudocode}
```

### Review questions

1. Why do token offsets refer to expanded rather than original source?

2. How would you preserve previous settings after a failed reload?
