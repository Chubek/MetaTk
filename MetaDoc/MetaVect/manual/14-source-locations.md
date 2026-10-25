# Chapter 14 — Source Text and Locations

**Status:** implemented (`frontend/Input.d`).

## Ownership

`SourceText` owns immutable text plus a file name and owns nothing else:
no filesystem access, no evaluation. Reading a file is the driver's job;
everything below the driver takes text it is given. A null source is a
caller bug (`enforce` failure), never a source diagnostic.

## Locations and spans

`SourceLocation` holds a file name, a zero-based byte `offset`, and
one-based `line`/`column`. Columns count bytes. Two locations are equal
when file and offset match — line and column are derived views.

`SourceSpan` is a half-open byte range: `span(3, 6)` covers offsets 3, 4,
5. End of file has a zero-width span at `text.length`, which is how the
parser points at a missing construct:

```text
missing.mvct:2:1: error: expected expression, found end of file
```

## Line mapping

Line starts are recorded once at construction; `locationAt` binary-searches
them, so lookup is logarithmic. LF, CRLF, and lone CR all break lines.
Out-of-bounds offsets and reversed spans are rejected with exceptions,
keeping resource/API misuse distinct from source errors (chapter 20).
