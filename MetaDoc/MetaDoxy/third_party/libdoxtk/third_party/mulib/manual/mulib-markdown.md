# MuLIB focused Markdown renderer

**Header:** `include/mulib-markdown.h` · **Language:** ISO C11, C++17 declarations

## Scope: deliberately not CommonMark

The renderer produces an **HTML fragment** using `mulib-html.h`'s shared escaping
and `mulib-strbuf.h` text builders. It does not maintain a second DOM or implement
an independent entity encoder. Input is XML-valid UTF-8 without embedded NUL.

Supported blocks:

- ATX headings: 1–6 `#` characters at column zero, followed by space/tab.
  Closing `#` characters are ordinary content.
- Paragraphs: consecutive ordinary lines joined with one space. Blank lines end
  paragraphs. Leading/trailing horizontal whitespace is trimmed.
- Flat unordered lists: `-`, `*` or `+` followed by space/tab at column zero.
- Flat ordered lists: digits, `.`, then space/tab at column zero. Source numbers
  are not retained; HTML starts at 1. Each list item occupies one source line.
- Thematic rules: exactly `---` or `***`, with optional trailing whitespace.
- Triple-backtick code fences at column zero. Optional language identifiers use
  letters/digits/`_`/`-` and become `class="language-ID"`. The closing line is
  three backticks with optional horizontal whitespace and no identifier. Code
  lines are escaped verbatim and terminated with LF.

CR, CRLF and LF are recognized as line endings. Blocks can interrupt paragraphs.
Blank lines end lists. Changing ordered/unordered marker kind starts a new list.
No indentation/nesting, lazy continuation or blockquote rules are implemented.

Supported inline syntax:

- `*emphasis*`, `_emphasis_`, `**strong**`, `__strong__`: simple matched runs,
  nonempty content without leading/trailing whitespace. Different run lengths
  can nest, with at most 32 recursive levels. This is not CommonMark delimiter
  flanking/intraword parsing; runs longer than two are literal.
- Single-backtick code spans; content is escaped without further interpretation.
  Multi-backtick delimiters are not implemented and remain literal.
- `[label](URL)` links; labels can contain the inline syntax above but not nested
  links. No link titles, escaped/nested URL parentheses, reference links or images.
- Backslash escapes for ASCII punctuation. Unmatched inline delimiters are text.

Code spans protect brackets within link labels. Complete link syntax (including
its URL) and code spans are skipped when finding an enclosing emphasis delimiter;
an underscore or asterisk in a link destination cannot close outer emphasis.

Raw HTML and entity spellings are always escaped as text. There are no tables,
Setext headings, indented code, autolinks, task lists, hard-break syntax or full
CommonMark compliance. Unsupported syntax generally remains paragraph text,
possibly with supported inline constructs inside it.

## Links and safety

URLs must be nonempty and cannot contain ASCII whitespace/controls, quotes,
backslashes, angle brackets, backticks or parentheses. Only `http`, `https` and
`mailto` schemes are accepted (case-insensitively); relative paths, fragment/query
references and protocol-relative URLs are allowed. Other explicit schemes retain
the entire link syntax as escaped text rather than generating `href`.

This restricts active link schemes but does not perform URL canonicalization,
host policy, phishing protection or application authorization. The renderer
never fetches links and never emits user-controlled raw HTML/CSS/script.

## Integration and API

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#define MULIB_IMPL_MARKDOWN
#include "mulib-markdown.h"

char *mulib_markdown_to_html(const char *source, size_t length,
                            size_t *output_length, mulib_xml_error_t *error);
```

Define all transitive implementation switches explicitly in exactly one TU.
Returned text is owned, NUL-terminated, and released with `free`. `output_length`
excludes NUL. NULL/zero input produces an allocated empty string. NULL with
nonzero length fails. Input may be released after return.

NULL output indicates invalid UTF-8/XML characters, allocation/size failure,
inline nesting overflow, or an **unclosed code fence**. This last case deliberately
fails rather than silently accepting an accidental truncated block. Failure
sets optional length to zero and provides a static error message/byte offset;
allocation/inline errors point to the current block and character-validation
errors to offset zero. Success clears the optional error. No partial output is
returned. This is an in-memory renderer with simple scans, not a streaming or
indexed parser.

## Runnable example

```c
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#define MULIB_IMPL_MARKDOWN
#include "mulib-markdown.h"

int main(void) {
    const char source[]="# Hello\n\n**Bold** and <escaped>.\n\n- `code`\n";
    char *html=mulib_markdown_to_html(source,sizeof(source)-1,NULL,NULL);
    assert(html&&strstr(html,"<h1>Hello</h1>"));
    assert(strstr(html,"<strong>Bold</strong> and &lt;escaped&gt;."));
    free(html);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/markdown.c -o ./mulib-markdown
./mulib-markdown
```

`tests/test_markup.c` checks block/inline rendering, raw-HTML escaping, unsafe
schemes, malformed encodings/fences, randomized inputs and allocation failures.
