# perlpod: strict focused POD rendering

`mulib-perlpod.h` renders a documented POD subset into a safe HTML fragment or
plain text. It does not parse Perl or claim full `perlpod` compatibility. Unsupported
directives and formatting codes return errors rather than disappearing.

## Integration

Define `MULIB_IMPL_PERLPOD` once, and separately provide `MULIB_IMPL_STRBUF` and
`MULIB_IMPL_UTF8`. No dependency switch is silently defined. Other units need
only declarations; C++ consumers use the provided linkage guards.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/perlpod.c -o perlpod
./perlpod
```

```c
const char source[] = "=head1 Title\n\nB<Bold> and C<x E<lt> y>.\n=cut\n";
mulib_perlpod_error_t error;
size_t length;
char *html = mulib_perlpod_render(source, sizeof source-1,
    MULIB_PERLPOD_HTML, NULL, &length, &error);
/* On success html is malloc-owned, length excludes its trailing NUL. */
free(html);
```

## Line-oriented subset

Input is length-delimited UTF-8. NUL and controls other than tab/CR/LF are errors.
LF, CRLF, and bare CR delimit lines and output uses LF. Outside POD, ordinary
lines are ignored, allowing a POD section inside a Perl-like source file.
A recognized `=pod`, `=head1`..`=head4`, or `=over` begins POD. `=cut` ends it.
Directives must begin in column one and consume their entire line; unlike full
POD, a preceding blank line is not required.

* `=pod` and `=cut` take no arguments and cannot occur with open lists.
* `=head1 text` through `=head4 text` require nonempty formatted text and cannot
  occur in a list. HTML uses the corresponding heading element.
* `=over` or `=over N` opens an unordered list. Optional decimal N must be 1..16;
  it is validated but does not alter presentation. Nested lists must be inside
  an already opened item.
* `=item label` opens an item or closes the previous item at that level. An empty
  label or exactly `*` yields an unlabeled bullet; other labels are formatted
  item text, not numeric list semantics. Subsequent paragraphs belong to it.
* `=back` closes a list and takes no arguments. Text before a list's first item,
  unmatched back, unclosed over, or cutting an open list is an error.
* Blank lines end paragraphs/verbatim runs. Consecutive ordinary text lines in
  a paragraph are joined by one space. Formatting codes must close on the same
  line; cross-line codes are deliberately unsupported.
* Lines beginning with space/tab form verbatim runs, preserving their indentation
  and line breaks without interpreting formatting. A change between ordinary
  and indented lines ends the prior run, even without a blank line.

HTML list structure is `ul/li`; paragraphs use `p`, verbatim runs `pre`.
Plain text keeps headings as text, blank paragraph separators, and `* ` item
markers; it is intentionally simple, not a width-aware terminal typesetter.

Any other column-one `=` directive is an unsupported error **even outside an
active POD section**. Thus `=encoding`, `=begin`, `=end`, `=for`, and heading
levels above four are never silently skipped. Ordinary code lines not starting
with `=` remain ignored outside POD.

## Formatting and HTML safety

The single-angle forms `B<...>`, `I<...>`, `C<...>`, `F<...>`, and `S<...>` nest.
HTML maps these to `strong`, `em`, `code`, `em`, and a fixed
`span style="white-space:nowrap"` respectively. Plain text removes those wrappers.
`E<lt>`, `E<gt>`, `E<sol>`, `E<verbar>`, `E<amp>`, `E<quot>`, and `E<apos>`
produce literal characters, escaped again when necessary for HTML.

Unknown capital-letter formatting codes immediately followed by `<` are errors,
including `L`, `X`, and `Z`. Numeric and other named entities, multi-angle
formatting such as `C<< ... >>`, and unclosed codes are errors. There are no
links, generated IDs, HTML injection directives, or raw HTML support. Ordinary
text and verbatim bytes are escaped for `&`, `<`, and `>` in HTML output; no
caller-controlled byte is interpolated into a tag or attribute. The result is a
fragment, not a complete HTML document. Unicode bytes remain UTF-8 unchanged.

## Ownership and resources

`mulib_perlpod_render` borrows source only during the call and returns an
independent malloc-owned NUL-terminated allocation. Even empty input (NULL/0)
returns an allocated empty string. Free with `free`. On failure it returns NULL,
sets optional length to zero, and fills an optional error with a typed code,
static message, byte offset, and 1-based line/column. Some structural/output
errors identify the directive/line or end-of-input, rather than a single byte.
Success clears the error. CR-only line breaks are accepted; diagnostic line
counting follows LF bytes.

NULL limits select 1 MiB input, 4 MiB output, nesting 32. Supplied fields are
exact budgets, including zero. Depth above 64 is invalid. List depth and inline
format depth independently use the depth budget. Every output append checks the
byte cap before invoking the shared strbuf, including HTML escape expansion and
closing markup. A strbuf may reserve geometric spare capacity beyond logical
output bytes. There is no hidden unbounded document tree, external resource
loading, global state, or third-party parser. Allocation/limit failure discards
the partial output. Time is linear in source and produced output, with recursion
bounded by the format-depth limit.

`tests/test_language_tools.c` covers both formats, nested lists/formatting,
verbatim escaping, malformed/unknown directives, UTF-8 and NUL errors, exact
output limits, nesting limits, empty input, allocation failure, and mutations.
