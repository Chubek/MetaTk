# Sources and attribution

This book studies the **supplied source snapshot**, principally
`../DSLtk/DSLtk.hpp`, with `../DSLtk/DSLtk.d` as the secondary implementation.
Line references describe that snapshot and may change in later versions.
The verification report records SHA-256 digests of both implementation files.

The project's existing `../DSLtk/manual/`, examples, and tests supplied
background, API examples, and an initial topic map. Chapters 2–36 were
independently authored against the source; their implementation excerpts
identify the originating header and, where useful, line ranges. Chapter 37
combines the original comparative essay in `essays/ch37.md` with an adapted
version of `../DSLtk/manual/ch31-d-toolkit.md`. The assembly script remaps
chapter references. The introduction and five application narratives are in
`essays/ch01.md` and `essays/ch38.md` through `ch42.md`.

The complete case-study implementations are in `projects/`. Their source is
included directly in the PDF, avoiding a second typeset copy that can drift
from the compiled program. Additional complete chapter examples are extracted
and compiled by the listing verification target. Implementation excerpts,
fragments, pseudocode, and explicitly rejected examples are distinct from
standalone applications.

`DSLtk.hpp` embeds **magic_enum v0.9.8**, copyright 2019–2024 Daniil
Goncharov, under the MIT license. The upstream license notices and amalgamation
provenance remain in the supplied header. The book's enum-reflection chapter
attributes this implementation rather than presenting it as original DSLtk
code. The bibliography also identifies the D language specification, Earley's
parsing paper, Thompson's regular-expression paper, and the *egg* paper as
background references. Those works explain related algorithms; they do not
establish that DSLtk implements every facility of the cited systems.

The supplied tree has no top-level license file for the complete DSLtk project
or its manual. This document records provenance and does not assign a new
license to those materials. Bibliographic entries and the title page identify
this volume as a source-based study, without asserting upstream endorsement.

The book workflow writes only inside `DSLtkBook/`. It does not apply fixes to
the implementation being studied. Confirmed limitations are documented and
exercised by small probes; working application alternatives are identified as
application code.
