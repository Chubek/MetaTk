#!/usr/bin/env perl
use strict;
use warnings;
use 5.010;

=pod

=head1 NAME

aurocks.pl - generate a scannerless GLR-style C99 parser from an Aurocks grammar

=head1 SYNOPSIS

  perl scripts/aurocks.pl grammar.g > parser.c
  perl scripts/aurocks.pl --entrypoint parse_expr grammar.g > parser.c
  perl scripts/aurocks.pl --help
  perl scripts/aurocks.pl --version

The generated output is complete, self-contained C99 source code.

=head1 DESCRIPTION

C<aurocks.pl> is a self-contained parser generator for the grammar language
used by the Aurocks examples.  It reads a grammar file, extracts its C
prologue, directives, productions, semantic actions and C epilogue, and
writes a C99 parser implementation to standard output.

The generated parser is B<scannerless>: literal terminals and
regular-expression terminals are compiled into matcher code that runs
directly against the input buffer.  No separate lexer is ever required, and
no token table is built.  A C<%skip> regular expression may be used to
consume layout such as spaces and newlines between grammar symbols.

The implementation uses recursive GLR-style alternatives.  Each production
alternative is attempted from the same input position, and the parser state
is restored when an alternative fails.  Direct left recursion is supported
through a seed/tail iteration, so a production such as

  expr: expr '+' /[0-9]+/
      | /[0-9]+/
      ;

parses iteratively instead of overflowing the stack.  Successful
alternatives produce a value through the production's semantic action.

Everything the generated parser needs is emitted into a single translation
unit: a C<aurocks_> runtime library of exactly 35 helper functions (see
L</"THE AUROCKS RUNTIME LIBRARY">), a scannerless regular-expression
engine, the generated productions, and the public entry points.

=head1 OPTIONS

=over 2

=item C<--entrypoint I<NAME>>, C<-e I<NAME>>

Use I<NAME> as the name of the generated entry point instead of
C<parse_I<start production>>.  The default name is always generated as well,
so both C<NAME> and C<parse_I<start>> are callable.

=item C<--help>, C<-h>

Print a usage summary and exit.

=item C<--version>, C<-V>

Print the generator version and exit.

=back

=head1 INPUT GRAMMAR

Grammar files are divided into three regions:

=over 2

=item * C<%{ ... %}>

The C prologue.  It is copied into the generated source after the standard
headers and before the runtime implementations.  Place type declarations,
constructor prototypes, C<#define> overrides and other application-specific
declarations here.  The prologue is optional.

The prototypes of the 35 runtime helpers and the parser types are emitted
before the prologue, so the prologue may call C<aurocks_*> and may use
C<GLRParser>.

The prologue may override the tunables documented below, for example:

  #define GLR_MAX_DEPTH 4096

=item * The production section

This contains directives and grammar rules.  It is delimited by the C<%%>
markers:

  %{ ... %}       <- optional prologue
  %%              <- start of the production section
  ... rules ...
  %%              <- end of the production section
  ... epilogue ...

If the grammar contains a single C<%%> marker, the generator uses it to
decide which side of the marker carries the productions.  If the grammar
contains no C<%%> marker at all, the whole file is the production section.

=item * Text after the production section

The C epilogue.  It is copied verbatim to the end of the generated source.
This is normally where constructor and helper function definitions live.

=back

The generator requires a C<%start> directive and at least one production.
Production names are C identifiers.  A production has one or more
alternatives separated by C<|> and terminated by C<;>:

  expression:
        /[0-9]+/               { $$ = make_number($1); }
      | expression '+' /[0-9]+/  { $$ = add($1, $3); }
      ;

Symbols may be nonterminal names, quoted literals, regular expressions
written between slashes, or the special C<%empty> symbol.  A symbol that is
neither a production nor a literal nor a regular expression is rejected with
a diagnostic.

The production reader is brace- and quote-aware.  Actions may contain nested
braces, string literals, character literals and C comments; C<//> and
C</* */> comments are permitted anywhere in the production section.


=head1 DIRECTIVES

Directives are written on a line of their own in the production section.
The following directives are recognized:

=over 2

=item C<%start I<name>>

Selects the start production.  The generated API is C<parse_I<name>()>, with
C<yyparse()> as an alias.

=item C<%skip I</regexp/>>

Defines insignificant input to skip before terminal matches and at the end of
input.  The expression is compiled as a regular expression (see
L</"REGULAR EXPRESSIONS">) and must match at the current input position.  A
C<%skip> expression that can match the empty string is ignored, because it
would otherwise loop forever.

=item C<%dprec I<number>>

Attaches a dynamic precedence value to a production alternative.  The
directive may be written on the alternative itself:

  expr: expr '+' term %dprec 3 { $$ = add($1, $3); }

Alternatives of one production are attempted in order of decreasing dynamic
precedence; alternatives with equal precedence keep their written order.

=item C<%left>, C<%right>, C<%nonassoc>

Declare associativity conventions for operator-style productions.  They are
accepted and recorded as grammar metadata.

=item C<%prefer>, C<%avoid>, C<%longest>

Declare lexical or forest-selection preferences.  They are accepted as
grammar metadata and are emitted as SPPF policy flags in the generated C.

=item C<%merge>, C<%reject>, C<%cut>, C<%expect>

Declare merge, rejection, cut and expected-conflict policies for ambiguous
forests.  They are accepted as grammar metadata.

=item C<%glr>, C<%sppf>, C<%ambiguity>, C<%resolve>, C<%priority>

Declare GLR execution, shared-packed parse forest, ambiguity resolution and
priority behaviour.  They are accepted as grammar metadata.

=item C<%inline>, C<%layout>, C<%lexer>, C<%error>

Declare production inlining, layout handling, lexer integration and error
handling conventions.  They are accepted as grammar metadata.

=back

Unknown directives are retained as metadata rather than being rejected, so
grammar authors can annotate policies without making the generator fail.
A directive written inside a production (that is, while a production body is
open) is treated as part of that production, not as a file-level directive.

=head1 REGULAR EXPRESSIONS

Terminals written between slashes are compiled to regular expressions.  The
accepted syntax is a rich superset of POSIX ERE.

=head2 Operators

=over 2

=item * C<|>

Alternation.

=item * C<xy>

Concatenation.

=item * C<x*>, C<x+>, C<x?>

Zero or more, one or more, optional.

=item * C<x{m}>, C<x{m,}>, C<x{m,n}>

Bounded repetition.  C<x{2,5}> matches between two and five occurrences.

=item * C<(...)>

Grouping.

=item * C<.>

Any character except a newline.

=back

=head2 Bracket expressions

C<[...]> is a bracket expression.  It supports:

=over 2

=item * Literal characters, for example C<[abc]>.

=item * Ranges, for example C<[a-z]>.  A C<-> that does not sit between two
characters is literal.

=item * Negation with a leading C<^>, for example C<[^abc]>.

=item * POSIX character classes, for example C<[[:alpha:]]>.  The supported
class names are C<alnum>, C<alpha>, C<blank>, C<cntrl>, C<digit>, C<graph>,
C<lower>, C<print>, C<punct>, C<space>, C<upper>, C<xdigit> and C<word>.

=item * Backslash escapes, including the shorthand classes C<\d>, C<\D>,
C<\s>, C<\S>, C<\w> and C<\W>, which may also be used inside a bracket
expression.

=back

=head2 Escapes

C<\n>, C<\r>, C<\t>, C<\f>, C<\v>, C<\a>, C<\e> and C<\0> denote the
corresponding control characters.  C<\xHH> is a hexadecimal escape and
C<\NNN> is an octal escape.  Any other character after a backslash stands
for itself, so C<\.>, C<\*>, C<\+>, C<\?>, C<\(>, C<\)>, C<\[>, C<\]>,
C<\{>, C<\}>, C<\|>, C<\^>, C<\$> and C<\\> are all literal.

=head2 Anchors and boundaries

=over 2

=item * C<^>

Start of the current terminal match attempt.  Because terminals are
anchored at the current input position, a leading C<^> is effectively a
no-op; C<^> is an anchor only at the start of the pattern or at the start of
a parenthesized subexpression, and is a literal character anywhere else.

=item * C<$>

End of the input buffer.  C<$> is an anchor only at the end of the pattern
or immediately before a closing parenthesis, and is a literal character
anywhere else.

=item * C<\b> and C<\B>

Word boundary and non-word boundary.  A word character is C<[A-Za-z0-9_]>.
Inside a bracket expression C<\b> means backspace instead.

=back

=head2 Matching semantics

Terminals are matched B<anchored>: a regular expression must match at the
current input position and may not skip forward.  Within that constraint the
match is B<leftmost-longest>, following POSIX ERE: the longest possible
match is taken.  A regular expression that matches the empty string does not
match a terminal; use C<%empty> for a production that consumes nothing.

=head1 SEMANTIC ACTIONS

Actions are C fragments enclosed in braces.  The following substitutions are
performed before the action is emitted:

=over 2

=item C<$$>

Becomes the local C<result> variable.  Its type is always C<void *>, so a
production assigns its semantic value to it.  A production that never
assigns C<$$> yields C<NULL>.

  FOO: /a+/ { $$ = $TOKEN; }

=item C<$1>, C<$2>, ...

Select the value of the first, second, ... right-hand side symbol of the
alternative.  The type of C<$I> depends on the symbol:

=over 2

=item * For a nonterminal, C<$I> is the C<void *> semantic value produced
by that nonterminal.

=item * For a literal or regular-expression terminal, C<$I> is a C<char *>
holding the matched text, NUL-terminated and heap allocated.

=back

=item C<$TOKEN>

The C<char *> text of the most recently matched terminal of the current
alternative.  For a production whose only symbol is a regular expression,
C<$TOKEN> is exactly the text that the regular expression matched:

  FOO: /a+/ { $$ = $TOKEN; }

C<$TOKEN> is a C<char *>, so it can be assigned directly to the C<void *>
C<$$>.

=back

=head2 Ownership of terminal text

Terminal text is copied out of the input buffer into the heap when the
terminal is matched.  The generated code releases a copy that the semantic
action never looked at, so a production that matches a terminal without
inspecting its text does not leak.

A copy that the action B<does> use belongs to the action.  The action may
keep it, for example by assigning it to C<$$>, and is then responsible for
releasing it with C<aurocks_free> when it is no longer needed.  An action
that copies the text, for example with C<aurocks_strdup> or C<aurocks_strndup>,
should release the original:

  FOO: /a+/ { $$ = aurocks_strdup($TOKEN); aurocks_free((void *)$TOKEN); }

Use C<aurocks_strdup> or C<aurocks_strndup> when a private copy of a string
that the action does not own is required.

=head2 Automatic boxing

Actions that assign a C floating point conversion directly to C<$$> are
boxed automatically, so that the C<void *> value carries a C<double>:

  number: /[0-9]+(\.[0-9]+)?/ { $$ = strtod($TOKEN, NULL); }

is rewritten to use C<aurocks_box_double> and C<aurocks_strtod>.  The same
applies to C<atof>, C<strtof> and C<strtold>.

=head2 Compatibility rewrites

Two rewrites exist for grammars that were written against earlier revisions
of this generator:

=over 2

=item * C<build_number(vI)> becomes C<build_number(*(double *)vI)>, because
numeric semantic values are boxed as C<double *>.

=item * C<vI-E<gt>field> becomes C<((SList *)vI)-E<gt>field>.

=back

=head1 THE AUROCKS RUNTIME LIBRARY

Every generated parser contains a runtime library of exactly B<35> helper
functions, all prefixed with C<aurocks_>.  They are emitted with external
linkage so that semantic actions and the C epilogue can call them.  The
generated source also contains a manifest array, C<aurocks_runtime_functions>,
listing the 35 names, together with a compile-time assertion that the
manifest really holds 35 entries.

=head2 Memory management (7)

=over 2

=item * C<void *aurocks_alloc(size_t size)>

Allocate C<size> zeroed bytes.

=item * C<void *aurocks_calloc(size_t count, size_t size)>

Allocate C<count * size> zeroed bytes, with overflow checking.

=item * C<void *aurocks_realloc(void *ptr, size_t size)>

Resize an allocation.  A size of zero frees the block and returns C<NULL>.

=item * C<void aurocks_free(void *ptr)>

Release a block obtained from this library.

=item * C<char *aurocks_strdup(const char *s)>

Copy a NUL-terminated string.

=item * C<char *aurocks_strndup(const char *s, size_t n)>

Copy at most C<n> bytes of a string and NUL-terminate the copy.

=item * C<void *aurocks_memdup(const void *src, size_t n)>

Copy C<n> bytes of arbitrary data.

=back

=head2 String handling (8)

=over 2

=item * C<size_t aurocks_strlen(const char *s)>

=item * C<int aurocks_strcmp(const char *a, const char *b)>

=item * C<int aurocks_strncmp(const char *a, const char *b, size_t n)>

=item * C<int aurocks_strcasecmp(const char *a, const char *b)>

Case-insensitive comparison.  Only ASCII letters are folded, so the
function is portable C99.

=item * C<const char *aurocks_strchr(const char *s, int c)>

=item * C<const char *aurocks_strrchr(const char *s, int c)>

=item * C<const char *aurocks_strstr(const char *haystack, const char *needle)>

=item * C<size_t aurocks_strlcpy(char *dst, const char *src, size_t size)>

Copy with truncation.  Returns the length of C<src>, so truncation is
detectable.

=back

=head2 Numeric conversion (11)

=over 2

=item * C<long aurocks_strtol(const char *s, char **end, int base)>

=item * C<unsigned long aurocks_strtoul(const char *s, char **end, int base)>

=item * C<long long aurocks_strtoll(const char *s, char **end, int base)>

=item * C<unsigned long long aurocks_strtoull(const char *s, char **end, int base)>

=item * C<double aurocks_strtod(const char *s, char **end)>

=item * C<float aurocks_strtof(const char *s, char **end)>

=item * C<long double aurocks_strtold(const char *s, char **end)>

=item * C<int aurocks_atoi(const char *s)>

=item * C<long aurocks_atol(const char *s)>

=item * C<long long aurocks_atoll(const char *s)>

=item * C<double aurocks_atof(const char *s)>

=back

=head2 Formatted output (4)

=over 2

=item * C<int aurocks_snprintf(char *buf, size_t size, const char *fmt, ...)>

=item * C<int aurocks_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)>

=item * C<char *aurocks_asprintf(const char *fmt, ...)>

Format into a freshly allocated string.  The caller owns the result and
releases it with C<aurocks_free>.

=item * C<char *aurocks_vasprintf(const char *fmt, va_list ap)>

=back

=head2 Semantic value boxing (5)

=over 2

=item * C<void *aurocks_box_double(double value)>

=item * C<double aurocks_unbox_double(const void *box)>

=item * C<void *aurocks_box_long(long value)>

=item * C<long aurocks_unbox_long(const void *box)>

=item * C<void *aurocks_box_str(const char *text)>

Box a copy of a string as a C<void *> semantic value.

=back

=head1 GENERATED C RUNTIME

The generated source is laid out like this:

=over 2

=item * The standard headers.

=item * The prototypes of the 35 runtime helpers, the runtime manifest and
the compile-time count assertion.

=item * The parser types: C<GLRParser>, the regular-expression state
records and the C<GLR_MAX_DEPTH> tunable.

=item * The C prologue.

=item * The 35 runtime helper implementations.

=item * The scannerless matching engine: one state table and one matcher
function per regular-expression terminal, one character class function per
terminal that uses bracket expressions, the layout skipper and the terminal
dispatcher.

=item * One generated function per nonterminal.

=item * The public entry points.

=item * The C epilogue.

=back

Besides the 35 helper functions, the generated source contains:

=over 2

=item * A parser state with the buffer start, the current pointer, the end
pointer and the error position.

=item * A whitespace/layout skipper driven by C<%skip>.

=item * Literal matching for quoted terminals.

=item * Alternative restoration so a failed production does not consume input
needed by another branch, and a progress guard so that a left-recursive
production cannot loop forever without consuming input.

=item * A recursion depth guard, C<GLR_MAX_DEPTH>, which defaults to 512 and
may be overridden with a C<#define> in the prologue.

=item * C<parse_I<start>()>, C<yyparse()> and C<parse_I<start>_error()> entry
points.

=back

On success, C<parse_I<start>()> returns the semantic value produced by the
start production.  On failure, or when trailing non-layout input remains, it
returns C<NULL>.  C<parse_I<start>_error()> returns the input position at
which parsing failed, or C<NULL> if the last parse succeeded.

The runtime helpers have external linkage, so the generated translation unit
must not be linked together with a second generated parser.  Generate one
parser per translation unit.

=head1 SCANNERLESS MATCHING

Quoted terminals are matched literally at the current input position.
Regular-expression terminals are anchored automatically, so a terminal cannot
skip forward looking for a later match.  The current input pointer advances
only after a complete terminal match.  C<%skip> is applied before terminals
and once after the start production.

Because matching happens inside production evaluation, a grammar can express
token-like rules and structural rules in one specification.  This is useful
for languages where lexical interpretation depends on grammatical context.

=head1 LIMITATIONS

This generator intentionally favours a compact implementation over a full
canonical LR-table engine.  It does not currently construct a persistent,
graph-structured SPPF data structure or perform generalized chart sharing
across every equivalent state.  SPPF-related directives therefore describe
and annotate the intended disambiguation policy; production order, dynamic
precedence and successful alternative selection remain the operational
resolution mechanism.

Left recursion must be direct, that is, the recursive occurrence must be the
first symbol of the alternative, and the production must have at least one
non-left-recursive seed alternative.  Indirect left recursion and recursion
through a nullable prefix are rejected with a diagnostic.

A regular expression must match at least one character in order to match a
terminal.  Use C<%empty> for a production that consumes nothing.

Because the parser backtracks, the semantic value of a sub-production may be
discarded when the enclosing alternative fails.  Such a value is not
released, because the parser does not know how to release a C<void *>.  A
grammar that parses untrusted input should therefore collect its semantic
values in an arena that is reset after every parse.

Actions must be valid C after the documented substitutions.

=head1 OUTPUT

The generated C99 source is written to standard output.  It contains the
standard headers, the C prologue, the Aurocks runtime library, the
scannerless matching engine, the generated parsing functions, the public
entry points, and the C epilogue in one translation unit.  Diagnostics and
malformed-input errors are written by the Perl generator to standard error
and terminate with a non-zero exit status.

=head1 EXAMPLES

Generate and compile a parser:

  perl scripts/aurocks.pl grammar.g > parser.c
  cc -std=c99 -Wall -Wextra -c parser.c

The resulting parser exposes:

  void *parse_<start>(const char *input);
  void *yyparse(const char *input);
  const char *parse_<start>_error(void);

A grammar that exercises terminals, captures and the runtime library:

  %{
  #include <stdio.h>
  %}

  %start items
  %skip /[ \t\r\n]+/

  items:
        item                    { $$ = $1; }
      | items item             { $$ = $1; }
      ;

  item:
        /[A-Za-z_][A-Za-z0-9_]*/   { $$ = $TOKEN; }
      | /[0-9]+(\.[0-9]+)?/        { $$ = aurocks_box_double(aurocks_strtod($TOKEN, NULL)); }
      ;

  %%

  int main(void) {
      void *v = parse_items("hello 42");
      printf("%p\n", v);
      return 0;
  }

=head1 AUTHOR

The Aurocks project.

=cut

# ---------------------------------------------------------------------------
# Small utilities
# ---------------------------------------------------------------------------

sub trim {
    my ($s) = @_;
    return '' unless defined $s;
    $s =~ s/\A\s+//;
    $s =~ s/\s+\z//;
    return $s;
}

sub slurp {
    my ($path) = @_;
    open my $fh, '<', $path or die "cannot open $path: $!\n";
    local $/;
    return <$fh>;
}

sub c_byte_literal {
    my ($n) = @_;
    return "'\\\\'" if $n == 92;
    return "'\\''" if $n == 39;
    return sprintf("0x%02X", $n) if $n < 32 || $n > 126;
    return sprintf("'%s'", chr($n));
}

# ---------------------------------------------------------------------------
# Grammar scanning
# ---------------------------------------------------------------------------

# Scan a quoted string or character literal starting at $i (which points at
# the opening quote).  Returns the offset just past the closing quote.
sub scan_quoted_literal {
    my ($text, $q, $i) = @_;
    my $len = length $text;
    my $j   = $i + 1;
    while ($j < $len) {
        my $d = substr($text, $j, 1);
        if ($d eq '\\') { $j += 2; next; }
        return $j + 1 if $d eq $q;
        return $j     if $d eq "\n";    # unterminated literal
        $j++;
    }
    return $len;
}

# Scan a /regex/ literal starting at $i (which points at the opening slash).
# Returns the offset just past the closing slash.
sub scan_regex_literal {
    my ($text, $i) = @_;
    my $len = length $text;
    my $j   = $i + 1;
    my $in_class = 0;
    my $first_in_class = 0;
    while ($j < $len) {
        my $d = substr($text, $j, 1);
        if ($d eq '\\') { $first_in_class = 0 if $in_class; $j += 2; next; }
        if ($d eq '[' && !$in_class) {
            $in_class = 1;
            $first_in_class = 1;
            $j++;
            next;
        }
        if ($in_class) {
            if ($d eq '[' && substr($text, $j + 1, 1) eq ':') {
                my $end = index($text, ':]', $j + 2);
                if ($end >= 0) { $j = $end + 2; $first_in_class = 0; next; }
            }
            if ($d eq ']' && !$first_in_class) { $in_class = 0; $j++; next; }
            $first_in_class = 0 unless $d eq '^' && $first_in_class;
        }
        return $j + 1 if $d eq '/' && !$in_class;
        return $j     if $d eq "\n";    # unterminated regex
        $j++;
    }
    return $len;
}

# Split a production body into alternatives.  '|' is only a separator at
# brace depth zero and outside quotes and regular expressions.
sub split_alternatives {
    my ($text) = @_;
    my @out;
    my $len   = length $text;
    my $start = 0;
    my $depth = 0;
    my $i     = 0;
    while ($i < $len) {
        my $c = substr($text, $i, 1);
        if ($c eq '/' && substr($text, $i + 1, 1) eq '/') {
            my $e = index($text, "\n", $i);
            $i = $e < 0 ? $len : $e;
            next;
        }
        if ($c eq '/' && substr($text, $i + 1, 1) eq '*') {
            my $e = index($text, '*/', $i + 2);
            $i = $e < 0 ? $len : $e + 2;
            next;
        }
        if ($c eq '"' || $c eq "'") { $i = scan_quoted_literal($text, $c, $i); next; }
        if ($c eq '/' && $depth == 0) { $i = scan_regex_literal($text, $i); next; }
        if ($c eq '{') { $depth++; $i++; next; }
        if ($c eq '}') { $depth-- if $depth; $i++; next; }
        if ($c eq '|' && $depth == 0) {
            push @out, substr($text, $start, $i - $start);
            $start = $i + 1;
            $i++;
            next;
        }
        $i++;
    }
    push @out, substr($text, $start);
    return @out;
}

# Peel a trailing { ... } action off an alternative.  Returns the action
# body and the remaining right-hand side text.  Brace matching is aware of
# strings, character literals and comments.
sub extract_action {
    my ($text) = @_;
    my $t = $text;
    $t =~ s/\s+\z//;
    my $len   = length $t;
    my $depth = 0;
    my $open  = -1;
    my $i     = 0;
    while ($i < $len) {
        my $c = substr($t, $i, 1);
        # C comments are comments at any depth.
        if ($c eq '/' && substr($t, $i + 1, 1) eq '/') {
            my $e = index($t, "\n", $i);
            $i = $e < 0 ? $len : $e;
            next;
        }
        if ($c eq '/' && substr($t, $i + 1, 1) eq '*') {
            my $e = index($t, '*/', $i + 2);
            $i = $e < 0 ? $len : $e + 2;
            next;
        }
        # Outside an action a slash introduces a regular expression, which
        # may itself contain quotes and braces.
        if ($depth == 0 && $c eq '/') {
            $i = scan_regex_literal($t, $i);
            next;
        }
        if ($c eq '"' || $c eq "'") { $i = scan_quoted_literal($t, $c, $i); next; }
        if ($c eq '{') { $open = $i if $depth == 0; $depth++; $i++; next; }
        if ($c eq '}') {
            $depth--;
            if ($depth == 0 && $open >= 0 && $i == $len - 1) {
                my $action = substr($t, $open + 1, $i - $open - 1);
                my $rest   = substr($t, 0, $open);
                return ($action, $rest);
            }
            $i++;
            next;
        }
        $i++;
    }
    return ('', $text);
}

# Parse the symbol sequence of a right-hand side.
sub parse_rhs_symbols {
    my ($text) = @_;
    my @syms;
    my $t   = $text;
    my $len = length $t;
    my $pos = 0;
    while ($pos < $len) {
        $pos++ while $pos < $len && substr($t, $pos, 1) =~ /\s/;
        last if $pos == $len;
        my $c = substr($t, $pos, 1);
        if ($c eq '/') {
            my $end = scan_regex_literal($t, $pos);
            die "unterminated regular expression near [" . substr($t, $pos) . "]\n"
                if $end <= $pos + 1 || $end > $len || substr($t, $end - 1, 1) ne '/';
            push @syms, substr($t, $pos, $end - $pos);
            $pos = $end;
            next;
        }
        pos($t) = $pos;
        last unless $t =~ /\G(%empty\b|'(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*"|[A-Za-z_]\w*)/gc;
        push @syms, $1;
        $pos = pos($t);
    }
    my $rest = substr($t, $pos);
    $rest =~ s/\s+//g;
    if (length $rest) {
        die "cannot parse right-hand side near [$rest]\n";
    }
    return @syms;
}

# Walk the production section and return directives and productions.
sub scan_grammar_body {
    my ($text) = @_;
    my @items;
    my $len = length $text;
    my $i   = 0;
    my $depth = 0;
    my $at_line_start = 1;
    my $lhs;
    my @chunk;

    my $flush = sub {
        return unless defined $lhs;
        my $body = join('', @chunk);
        $body =~ s/\s+\z//;
        push @items, { type => 'rule', lhs => $lhs, body => $body };
        @chunk = ();
        $lhs = undef;
    };

    while ($i < $len) {
        my $c = substr($text, $i, 1);

        if ($c eq "\n") {
            push @chunk, $c if defined $lhs;
            $i++;
            $at_line_start = 1;
            next;
        }

        if ($depth == 0 && $at_line_start) {
            pos($text) = $i;
            if ($text =~ /\G[ \t]*([A-Za-z_]\w*)[ \t]*(?:%[A-Za-z_]\w*[ \t]*)*:/gc) {
                $flush->();
                $lhs = $1;
                $i   = pos($text);
                $at_line_start = 0;
                next;
            }
            if (!defined $lhs) {
                pos($text) = $i;
                if ($text =~ /\G%([A-Za-z_]\w*)([^\n]*)/gc) {
                    push @items, { type => 'directive', name => lc($1), value => trim($2) };
                    $i = pos($text);
                    $at_line_start = 0;
                    next;
                }
            }
        }

        if ($c eq '/' && substr($text, $i + 1, 1) eq '/') {
            my $e = index($text, "\n", $i);
            die "unterminated block comment\n" if $e < 0;
            push @chunk, ' ' x ($e - $i) if defined $lhs;
            $i = $e;
            next;
        }
        if ($c eq '/' && substr($text, $i + 1, 1) eq '*') {
            my $e = index($text, '*/', $i + 2);
            $e = $len if $e < 0;
            if (defined $lhs) {
                my $t = substr($text, $i, $e + 2 - $i);
                $t =~ s/[^\n]/ /g;
                push @chunk, $t;
            }
            $i = $e + 2;
            next;
        }
        if ($c eq '"' || $c eq "'") {
            my $e = scan_quoted_literal($text, $c, $i);
            push @chunk, substr($text, $i, $e - $i) if defined $lhs;
            $i = $e;
            $at_line_start = 0;
            next;
        }
        if ($c eq '/' && $depth == 0) {
            my $e = scan_regex_literal($text, $i);
            push @chunk, substr($text, $i, $e - $i) if defined $lhs;
            $i = $e;
            $at_line_start = 0;
            next;
        }
        if ($c eq '{') { $depth++; push @chunk, $c if defined $lhs; $i++; next; }
        if ($c eq '}') { $depth-- if $depth; push @chunk, $c if defined $lhs; $i++; next; }
        if ($c eq ';' && $depth == 0) {
            push @chunk, $c if defined $lhs;
            $i++;
            $flush->();
            $at_line_start = 0;
            next;
        }

        push @chunk, $c if defined $lhs;
        $i++;
        $at_line_start = 0;
    }
    die "unterminated action\n" if $depth;
    $flush->();
    return @items;
}

sub section_has_rules {
    my ($text) = @_;
    for my $item (scan_grammar_body($text)) {
        return 1 if $item->{type} eq 'rule';
    }
    return 0;
}

sub parse_grammar {
    my ($g) = @_;

    # --- prologue -----------------------------------------------------
    my $prologue = '';
    my $pstart   = index($g, '%{');
    if ($pstart >= 0) {
        my $pend = index($g, '%}', $pstart + 2);
        die "unterminated %{ prologue %}\n" if $pend < 0;
        $prologue = substr($g, $pstart + 2, $pend - $pstart - 2);
        # Blank out the prologue so that %% markers and directives are only
        # looked for in the production section.
        substr($g, $pstart, $pend + 2 - $pstart, ' ' x ($pend + 2 - $pstart));
    }

    # --- production section and epilogue ------------------------------
    my @marks;
    while ($g =~ /%%/g) { push @marks, $-[0]; }
    my ($body, $epilogue);
    if (@marks >= 2) {
        my $body_start = $marks[0] + 2;
        my $body_end   = $marks[1];
        $body     = substr($g, $body_start, $body_end - $body_start);
        $epilogue = substr($g, $marks[1] + 2);
    }
    elsif (@marks == 1) {
        # A single %% marker is ambiguous: it may introduce the production
        # section or the epilogue.  Use whichever side carries the
        # productions.
        my $before = substr($g, 0, $marks[0]);
        my $after  = substr($g, $marks[0] + 2);
        if (section_has_rules($before)) {
            $body     = $before;
            $epilogue = $after;
        }
        else {
            $body     = $after;
            $epilogue = '';
        }
    }
    else {
        $body     = $g;
        $epilogue = '';
    }

    # --- directives and productions -----------------------------------
    my %directives;
    my @rules;
    for my $item (scan_grammar_body($body)) {
        if ($item->{type} eq 'directive') {
            push @{ $directives{ $item->{name} } }, $item->{value};
            next;
        }
        my $lhs = $item->{lhs};
        for my $alt (split_alternatives($item->{body})) {
            $alt =~ s/\s*;\s*\z//s;
            $alt = trim($alt);
            next unless length $alt;
            my ($action, $rhs_text) = extract_action($alt);
            my $prec = 0;
            if ($rhs_text =~ /(^|\s)%dprec\s+(-?\d+)/) {
                $prec = $2;
                $rhs_text =~ s/(^|\s)%dprec\s+(-?\d+)/$1/;
            }
            $rhs_text =~ s/(^|\s)%empty\b/$1/g;
            my @syms = parse_rhs_symbols($rhs_text);
            @syms = () if @syms == 1 && $syms[0] eq '%empty';
            push @rules, {
                lhs    => $lhs,
                rhs    => \@syms,
                action => $action,
                dprec  => $prec,
            };
        }
    }
    die "no productions found\n" unless @rules;

    # Directives written in the prologue act as a fallback for directives
    # that were not given in the production section.
    for my $item (scan_grammar_body($prologue)) {
        next unless $item->{type} eq 'directive';
        next if exists $directives{ $item->{name} };
        push @{ $directives{ $item->{name} } }, $item->{value};
    }

    my $start = undef;
    $start = $directives{start}[0]
        if $directives{start} && $directives{start}[0] =~ /\A[A-Za-z_]\w*\z/;
    die "missing %start directive\n" unless defined $start;

    my %nonterm = map { $_->{lhs} => 1 } @rules;
    die "start production [$start] is not defined\n" unless $nonterm{$start};

    return {
        prologue   => $prologue,
        epilogue   => $epilogue,
        start      => $start,
        directives => \%directives,
        rules      => \@rules,
        nonterm    => \%nonterm,
    };
}

# A nonterminal is nullable when one of its alternatives can match the
# empty string.  Terminals always consume at least one character.
sub compute_nullable {
    my ($rules, $nonterm) = @_;
    my %nullable;
    my $changed = 1;
    while ($changed) {
        $changed = 0;
        for my $r (@$rules) {
            next if $nullable{ $r->{lhs} };
            my $all = 1;
            for my $s (@{ $r->{rhs} }) {
                if (!$nonterm->{$s} || !$nullable{$s}) { $all = 0; last; }
            }
            if ($all) { $nullable{ $r->{lhs} } = 1; $changed = 1; }
        }
    }
    return \%nullable;
}

# Reject cycles reachable before consuming input.  Only a direct first-symbol
# self-reference can be implemented by the seed/tail loop in emit_nonterminal.
sub check_recursion {
    my ($rules, $nonterm, $nullable) = @_;
    my %edges;
    for my $r (@$rules) {
        my $lhs = $r->{lhs};
        my $rh  = $r->{rhs};
        for my $k (0 .. $#$rh) {
            my $s = $rh->[$k];
            last unless $nonterm->{$s};
            if ($s eq $lhs) {
                die "production [$lhs] has unsupported nullable-prefix left recursion\n"
                    if $k != 0;
                last; # The direct self-reference is handled by seed/tail iteration.
            } else {
                $edges{$lhs}{$s} = 1;
            }
            last unless $nullable->{$s};
        }
    }
    my (%visiting, %visited);
    my $visit;
    $visit = sub {
        my ($nt) = @_;
        die "indirect left recursion involving [$nt]\n" if $visiting{$nt};
        return if $visited{$nt};
        $visiting{$nt} = 1;
        $visit->($_) for sort keys %{ $edges{$nt} || {} };
        delete $visiting{$nt};
        $visited{$nt} = 1;
    };
    $visit->($_) for sort keys %$nonterm;
}

# ---------------------------------------------------------------------------
# Regular expressions
# ---------------------------------------------------------------------------

sub regex_escape_char {
    my ($c) = @_;
    return "\n" if $c eq 'n';
    return "\r" if $c eq 'r';
    return "\t" if $c eq 't';
    return "\f" if $c eq 'f';
    return chr(11) if $c eq 'v';
    return "\a" if $c eq 'a';
    return "\e" if $c eq 'e';
    return "\0" if $c eq '0';
    return chr(8)  if $c eq 'b';
    return $c;
}

sub regex_posix_class_expr {
    my ($name, $var) = @_;
    my %map = (
        alnum => "((($var) >= '0' && ($var) <= '9') || ((($var) >= 'A' && ($var) <= 'Z') || (($var) >= 'a' && ($var) <= 'z')))",
        alpha => "((($var) >= 'A' && ($var) <= 'Z') || (($var) >= 'a' && ($var) <= 'z'))",
        blank => "(($var) == ' ' || ($var) == '\\t')",
        cntrl => "((($var) <= 0x1F) || (($var) == 0x7F))",
        digit => "((($var) >= '0' && ($var) <= '9'))",
        graph => "((($var) >= 0x21 && ($var) <= 0x7E))",
        lower => "((($var) >= 'a' && ($var) <= 'z'))",
        print => "((($var) >= 0x20 && ($var) <= 0x7E))",
        punct => "((($var) >= 0x21 && ($var) <= 0x2F) || (($var) >= 0x3A && ($var) <= 0x40) || (($var) >= 0x5B && ($var) <= 0x60) || (($var) >= 0x7B && ($var) <= 0x7E))",
        space => "(($var) == ' ' || ($var) == '\\t' || ($var) == '\\n' || ($var) == '\\r' || ($var) == '\\f' || ($var) == '\\x0b')",
        upper => "((($var) >= 'A' && ($var) <= 'Z'))",
        xdigit => "((($var) >= '0' && ($var) <= '9') || ((($var) >= 'A' && ($var) <= 'F') || (($var) >= 'a' && ($var) <= 'f')))",
        word => "((($var) >= '0' && ($var) <= '9') || ((($var) >= 'A' && ($var) <= 'Z') || (($var) >= 'a' && ($var) <= 'z')) || ($var) == '_')",
    );
    my $k = lc $name;
    die "unsupported POSIX character class [$name]\n" unless exists $map{$k};
    return $map{$k};
}

sub regex_class_item_expr {
    my ($item, $var) = @_;
    my $type = $item->{type};
    if ($type eq 'char') {
        return "($var == " . c_byte_literal($item->{ch}) . ")";
    }
    if ($type eq 'range') {
        return "(($var >= " . c_byte_literal($item->{from}) . ") && ($var <= " . c_byte_literal($item->{to}) . "))";
    }
    if ($type eq 'posix') {
        return regex_posix_class_expr($item->{name}, $var);
    }
    if ($type eq 'set') {
        return regex_class_expr($item->{node}, $var);
    }
    die "unknown character class item\n";
}

sub regex_class_expr {
    my ($node, $var) = @_;
    my @parts = map { regex_class_item_expr($_, $var) } @{ $node->{items} || [] };
    die "empty character class\n" unless @parts;
    my $expr = join(' || ', @parts);
    return $node->{neg} ? "(!($expr))" : "($expr)";
}

# Parse a regular expression into an abstract syntax tree.
#
# The accepted syntax is a rich superset of POSIX ERE: alternation,
# concatenation, the three repetition operators, interval expressions,
# grouping, bracket expressions with ranges, POSIX classes and negation,
# the GNU-style shorthand classes, word boundary assertions, hexadecimal and
# octal escapes, and the two anchors.
sub regex_parse {
    my ($pat) = @_;
    my $len = length $pat;
    my $i   = 0;
    my $err = sub { die "bad regular expression [$pat]\n"; };

    my $peek = sub { $i < $len ? substr($pat, $i, 1) : undef };
    my $eat  = sub { $i < $len ? substr($pat, $i++, 1) : undef };

    my $shorthand = sub {
        my ($c) = @_;
        my %map = (
            d => [ 'digit', 0 ], D => [ 'digit', 1 ],
            s => [ 'space', 0 ], S => [ 'space', 1 ],
            w => [ 'word',  0 ], W => [ 'word',  1 ],
        );
        my ($cls, $neg) = @{ $map{$c} };
        return { type => 'class', neg => $neg, items => [ { type => 'posix', name => $cls } ] };
    };

    # Escape sequence outside a bracket expression.
    my $escape_node = sub {
        my ($c) = @_;
        return $shorthand->($c) if $c =~ /[dDsSwW]/;
        return { type => 'boundary', neg => 0 } if $c eq 'b';
        return { type => 'boundary', neg => 1 } if $c eq 'B';
        if ($c eq 'x' || $c eq 'X') {
            my $hex = '';
            while (length($hex) < 2 && $i < $len && substr($pat, $i, 1) =~ /[0-9A-Fa-f]/) {
                $hex .= substr($pat, $i++, 1);
            }
            $err->() unless length $hex;
            return { type => 'char', ch => hex($hex), escaped => 1 };
        }
        if ($c =~ /[0-7]/) {
            my $oct = $c;
            $oct .= substr($pat, $i++, 1)
                while length($oct) < 3 && $i < $len && substr($pat, $i, 1) =~ /[0-7]/;
            return { type => 'char', ch => oct($oct) & 0xFF, escaped => 1 };
        }
        return { type => 'char', ch => ord(regex_escape_char($c)), escaped => 1 };
    };

    # Escape sequence inside a bracket expression.
    my $escape_class_item = sub {
        my ($c) = @_;
        return { type => 'set', node => $shorthand->($c) } if $c =~ /[dDsSwW]/;
        if ($c eq 'x' || $c eq 'X') {
            my $hex = '';
            while (length($hex) < 2 && $i < $len && substr($pat, $i, 1) =~ /[0-9A-Fa-f]/) {
                $hex .= substr($pat, $i++, 1);
            }
            $err->() unless length $hex;
            return { type => 'char', ch => hex($hex), escaped => 1 };
        }
        if ($c =~ /[0-7]/) {
            my $oct = $c;
            $oct .= substr($pat, $i++, 1)
                while length($oct) < 3 && $i < $len && substr($pat, $i, 1) =~ /[0-7]/;
            return { type => 'char', ch => oct($oct) & 0xFF, escaped => 1 };
        }
        return { type => 'char', ch => ord(regex_escape_char($c)), escaped => 1 };
    };

    my $parse_class_atom;
    my $parse_class;
    my $parse_atom;
    my $parse_repeat;
    my $parse_concat;
    my $parse_expr;

    $parse_class_atom = sub {
        my $c = $peek->();
        $err->() unless defined $c;
        if ($c eq '\\') {
            $eat->();
            my $e = $eat->();
            $err->() unless defined $e;
            return $escape_class_item->($e);
        }
        if ($c eq '[' && substr($pat, $i + 1, 1) eq ':') {
            my $end = index($pat, ':]', $i + 2);
            $err->() if $end < 0;
            my $name = substr($pat, $i + 2, $end - ($i + 2));
            $i = $end + 2;
            return { type => 'posix', name => $name };
        }
        $eat->();
        return { type => 'char', ch => ord($c), escaped => 0 };
    };

    $parse_class = sub {
        my $neg = 0;
        my @items;
        $eat->();                      # consume '['
        my $c = $peek->();
        if (defined $c && $c eq '^') { $neg = 1; $eat->(); }
        my $first = 1;
        while (1) {
            $c = $peek->();
            $err->() unless defined $c;
            last if $c eq ']' && !$first;
            my $atom = $parse_class_atom->();
            # A '-' starts a range only when it follows a character and is
            # itself followed by another character; otherwise it is literal.
            if ($atom->{type} eq 'char'
                && $atom->{ch} != 45
                && defined($peek->())
                && $peek->() eq '-'
                && substr($pat, $i + 1, 1) ne ']'
                && substr($pat, $i + 1, 1) ne '') {
                $eat->();
                my $rhs = $parse_class_atom->();
                $err->() unless $rhs->{type} eq 'char';
                push @items, { type => 'range', from => $atom->{ch}, to => $rhs->{ch} };
            } else {
                push @items, $atom;
            }
            $first = 0;
        }
        $eat->();                      # consume ']'
        return { type => 'class', neg => $neg, items => \@items };
    };

    # $allow_anchor is true when the atom starts a fresh expression, that is
    # at the very start of the pattern or right after '('.  Only then may '^'
    # act as an anchor.
    $parse_atom = sub {
        my ($allow_anchor) = @_;
        my $c = $peek->();
        $err->() unless defined $c;

        if ($c eq '(') {
            $eat->();
            my $node = $parse_expr->(1);
            $err->() unless defined($peek->()) && $peek->() eq ')';
            $eat->();
            return $node;
        }
        if ($c eq '[') { return $parse_class->(); }
        if ($c eq '.') { $eat->(); return { type => 'any' }; }
        if ($c eq '^') {
            if ($allow_anchor) { $eat->(); return { type => 'bol' }; }
            $eat->();
            return { type => 'char', ch => 94, escaped => 0 };
        }
        if ($c eq '$') {
            my $next = ($i + 1 < $len) ? substr($pat, $i + 1, 1) : undef;
            if (!defined $next || $next eq ')') { $eat->(); return { type => 'eol' }; }
            $eat->();
            return { type => 'char', ch => 36, escaped => 0 };
        }
        if ($c eq '\\') {
            $eat->();
            my $e = $eat->();
            $err->() unless defined $e;
            return $escape_node->($e);
        }
        if ($c eq '*' || $c eq '+' || $c eq '?') { $err->(); }
        if ($c eq '{' || $c eq '}') {
            # A brace that does not introduce a valid interval is literal.
            $eat->();
            return { type => 'char', ch => ord($c), escaped => 0 };
        }
        $eat->();
        return { type => 'char', ch => ord($c), escaped => 0 };
    };

    $parse_repeat = sub {
        my ($allow_anchor) = @_;
        my $node = $parse_atom->($allow_anchor);
        while (defined(my $c = $peek->())) {
            if ($c eq '*') {
                $eat->();
                $node = { type => 'repeat', min => 0, max => undef, child => $node };
            }
            elsif ($c eq '+') {
                $eat->();
                $node = { type => 'repeat', min => 1, max => undef, child => $node };
            }
            elsif ($c eq '?') {
                $eat->();
                $node = { type => 'repeat', min => 0, max => 1, child => $node };
            }
            elsif ($c eq '{') {
                my $save = $i;
                $eat->();
                my $read_num = sub {
                    my $n = '';
                    while (defined($peek->()) && $peek->() =~ /[0-9]/) { $n .= $eat->(); }
                    return $n;
                };
                my $m = $read_num->();
                if ($m eq '') { $i = $save; last; }
                my $n = $m;
                if (defined($peek->()) && $peek->() eq ',') {
                    $eat->();
                    $n = $read_num->();
                    $n = undef if $n eq '';
                }
                if (!defined($peek->()) || $peek->() ne '}') { $i = $save; last; }
                $eat->();
                $node = {
                    type => 'repeat',
                    min  => int($m),
                    max  => defined($n) ? int($n) : undef,
                    child => $node,
                };
            }
            else { last; }
            # A trailing '?' requests a lazy match.  Matching is
            # leftmost-longest, so laziness cannot be observed; it is
            # accepted and ignored.
            if (defined($peek->()) && $peek->() eq '?') { $eat->(); }
        }
        return $node;
    };

    $parse_concat = sub {
        my ($allow_anchor) = @_;
        my @parts;
        my $first = 1;
        while (1) {
            my $c = $peek->();
            last unless defined $c && $c ne '|' && $c ne ')';
            push @parts, $parse_repeat->($allow_anchor && $first);
            $first = 0;
        }
        return { type => 'empty' } unless @parts;
        return $parts[0] if @parts == 1;
        return { type => 'concat', parts => \@parts };
    };

    $parse_expr = sub {
        my ($allow_anchor) = @_;
        my @alts = ($parse_concat->($allow_anchor));
        while (defined($peek->()) && $peek->() eq '|') {
            $eat->();
            push @alts, $parse_concat->(0);
        }
        return $alts[0] if @alts == 1;
        return { type => 'alt', parts => \@alts };
    };

    my $ast = $parse_expr->(1);
    $err->() if $i != $len;
    return $ast;
}

# Compile a regular expression AST into a Thompson NFA.
sub regex_compile_nfa {
    my ($ast, $desc) = @_;
    my @states;
    my @classes;

    my $new_state = sub {
        my ($kind, $out1, $out2, $arg) = @_;
        push @states, {
            kind => $kind,
            out1 => defined($out1) ? $out1 : -1,
            out2 => defined($out2) ? $out2 : -1,
            arg  => defined($arg)  ? $arg  : 0,
        };
        return $#states;
    };

    my $patch = sub {
        my ($outs, $target) = @_;
        for my $o (@$outs) { $states[$o->[0]]{$o->[1]} = $target; }
    };

    my $concat = sub {
        my ($a, $b) = @_;
        $patch->($a->{outs}, $b->{start});
        return { start => $a->{start}, outs => $b->{outs} };
    };

    my $compile;
    my $class_id = sub {
        my ($node) = @_;
        return $node->{class_id} if defined $node->{class_id};
        $node->{class_id} = scalar @classes;
        push @classes, $node;
        return $node->{class_id};
    };

    my $repeat_frag;
    $repeat_frag = sub {
        my ($node, $count) = @_;
        my $frag;
        for (1 .. $count) {
            my $next = $compile->($node);
            $frag = defined $frag ? $concat->($frag, $next) : $next;
        }
        return $frag || do {
            my $s = $new_state->('jmp', undef, undef, undef);
            { start => $s, outs => [ [ $s, 'out1' ] ] };
        };
    };

    my $empty_frag = sub {
        my $s = $new_state->('jmp', undef, undef, undef);
        return { start => $s, outs => [ [ $s, 'out1' ] ] };
    };

    $compile = sub {
        my ($node) = @_;
        my $type = $node->{type};

        if ($type eq 'literal') {
            my @bytes = unpack('C*', $node->{text});
            return $empty_frag->() unless @bytes;
            my $frag;
            for my $b (@bytes) {
                my $s = $new_state->('char', undef, undef, $b);
                my $next = { start => $s, outs => [ [ $s, 'out1' ] ] };
                $frag = defined $frag ? $concat->($frag, $next) : $next;
            }
            return $frag;
        }
        if ($type eq 'char') {
            my $s = $new_state->('char', undef, undef, $node->{ch});
            return { start => $s, outs => [ [ $s, 'out1' ] ] };
        }
        if ($type eq 'any') {
            my $s = $new_state->('any', undef, undef, 0);
            return { start => $s, outs => [ [ $s, 'out1' ] ] };
        }
        if ($type eq 'class') {
            my $id = $class_id->($node);
            my $s = $new_state->('class', undef, undef, $id);
            return { start => $s, outs => [ [ $s, 'out1' ] ] };
        }
        if ($type eq 'bol') {
            my $s = $new_state->('bol', undef, undef, 0);
            return { start => $s, outs => [ [ $s, 'out1' ] ] };
        }
        if ($type eq 'eol') {
            my $s = $new_state->('eol', undef, undef, 0);
            return { start => $s, outs => [ [ $s, 'out1' ] ] };
        }
        if ($type eq 'boundary') {
            my $s = $new_state->('wordb', undef, undef, $node->{neg} ? 0 : 1);
            return { start => $s, outs => [ [ $s, 'out1' ] ] };
        }
        if ($type eq 'empty') { return $empty_frag->(); }
        if ($type eq 'concat') {
            my @parts = @{ $node->{parts} || [] };
            my $frag = $compile->(shift @parts);
            for my $part (@parts) { $frag = $concat->($frag, $compile->($part)); }
            return $frag;
        }
        if ($type eq 'alt') {
            my @parts = @{ $node->{parts} || [] };
            my $frag = $compile->(shift @parts);
            for my $part (@parts) {
                my $rhs = $compile->($part);
                my $s = $new_state->('split', $frag->{start}, $rhs->{start}, undef);
                $frag = { start => $s, outs => [ @{ $frag->{outs} }, @{ $rhs->{outs} } ] };
            }
            return $frag;
        }
        if ($type eq 'repeat') {
            my $min = $node->{min};
            my $max = $node->{max};
            my $frag;
            if (defined $max) {
                die "bad regular expression [$desc->{text}]\n" if $max < $min;
                $frag = $min ? $repeat_frag->($node->{child}, $min) : $empty_frag->();
                my $extra = $max - $min;
                for (1 .. $extra) {
                    my $opt = $compile->($node->{child});
                    my $s = $new_state->('split', $opt->{start}, undef, undef);
                    my $maybe = { start => $s, outs => [ @{ $opt->{outs} }, [ $s, 'out2' ] ] };
                    $frag = $concat->($frag, $maybe);
                }
                return $frag;
            }
            $frag = $min ? $repeat_frag->($node->{child}, $min) : $empty_frag->();
            my $star = $compile->($node->{child});
            my $s = $new_state->('split', $star->{start}, undef, undef);
            $patch->($star->{outs}, $s);
            my $tail = { start => $s, outs => [ [ $s, 'out2' ] ] };
            return defined $frag ? $concat->($frag, $tail) : $tail;
        }
        die "bad regular expression [" . ($desc->{text} // '') . "]\n";
    };

    my $frag = $compile->($ast);
    my $match = $new_state->('match', undef, undef, undef);
    $patch->($frag->{outs}, $match);
    return {
        states  => \@states,
        classes => \@classes,
        start   => $frag->{start},
    };
}

# ---------------------------------------------------------------------------
# Terminals
# ---------------------------------------------------------------------------

sub unquote_literal {
    my ($s) = @_;
    return undef unless $s =~ /\A\s*(['"])(.*?)\1\s*\z/s;
    my $v = $2;
    $v =~ s/\\(['"\\])/$1/g;
    return $v;
}

sub parse_terminal_symbol {
    my ($s) = @_;
    my $lit = unquote_literal($s);
    return {
        kind => 'literal',
        text => $lit,
        key  => 'lit:' . unpack('H*', $lit),
        ast  => { type => 'literal', text => $lit },
    } if defined $lit;
    if ($s =~ m!\A/(.*)/\z!s) {
        my $pat = $1;
        return {
            kind => 'regex',
            text => $pat,
            key  => 're:' . unpack('H*', $pat),
            ast  => regex_parse($pat),
        };
    }
    return undef;
}

sub collect_terminals {
    my ($rules, $skip) = @_;
    my %by_key;
    my %by_symbol;
    my @terms;
    my $add = sub {
        my ($desc) = @_;
        return $by_key{ $desc->{key} } if exists $by_key{ $desc->{key} };
        $desc->{id} = scalar @terms;
        $by_key{ $desc->{key} } = $desc->{id};
        push @terms, $desc;
        return $desc->{id};
    };
    for my $r (@$rules) {
        for my $s (@{ $r->{rhs} || [] }) {
            my $desc = parse_terminal_symbol($s) or next;
            my $id = $add->($desc);
            $by_symbol{$s} = $id unless exists $by_symbol{$s};
        }
    }
    my $skip_desc = undef;
    if (defined $skip && $skip ne '') {
        $skip_desc = {
            kind => 'regex',
            text => $skip,
            key  => 'skip:' . unpack('H*', $skip),
            ast  => regex_parse($skip),
            id   => 'skip',
        };
    }
    return (\@terms, $skip_desc, \%by_symbol);
}

# ---------------------------------------------------------------------------
# Semantic actions
# ---------------------------------------------------------------------------

sub action_uses_token {
    my ($a) = @_;
    return 0 unless defined $a && length $a;
    return 1 if $a =~ /\$\{TOKEN\}/;
    return 1 if $a =~ /\$TOKEN\b/;
    return 0;
}

# The positions of the right-hand side whose terminal text the action does
# not use.  The generated code releases those copies after the action has
# run, which keeps a grammar that matches a terminal without inspecting
# its text free of leaks.
sub unused_token_positions {
    my ($rule) = @_;
    my $action = $rule->{action};
    my $rhs    = $rule->{rhs};
    my @terms;
    for my $i (0 .. $#{$rhs}) {
        my $s = $rhs->[$i];
        next if $s eq '%empty';
        next if $rule->{nonterm}{$s};
        push @terms, $i;
    }
    return [] unless @terms;
    my $last = $terms[-1];
    my $want_token = action_uses_token($action);
    my @unused;
    for my $i (@terms) {
        my $n = $i + 1;
        my $used = 0;
        $used = 1 if defined $action && $action =~ /\$\Q$n\E(?!\d)/;
        $used = 1 if $want_token && $i == $last;
        push @unused, $i unless $used;
    }
    return \@unused;
}

sub emit_release_unused {
    my ($rule, $indent) = @_;
    my $out = '';
    for my $i (@{ unused_token_positions($rule) }) {
        $out .= "${indent}aurocks_free(v" . ($i + 1) . ");\n";
    }
    return $out;
}

sub action_c {
    my ($a, $tokvar, $prefix) = @_;
    $tokvar = 'token' unless defined $tokvar;
    $prefix = '' unless defined $prefix;
    return '' unless defined $a && length $a;
    $a =~ s/\$\$/${prefix}result/g;
    $a =~ s/\$\{TOKEN\}/$tokvar/g;
    $a =~ s/\$TOKEN\b/$tokvar/g;
    $a =~ s/\$(\d+)/${prefix}v$1/g;
    # Numeric conversions assigned straight to $$ are boxed so that the
    # void* result carries a double.
    $a =~ s/\b${prefix}result\s*=\s*strtod\s*\(([^;]*)\)/${prefix}result = aurocks_box_double(aurocks_strtod($1))/g;
    $a =~ s/\b${prefix}result\s*=\s*strtold\s*\(([^;]*)\)/${prefix}result = aurocks_box_double((double)aurocks_strtold($1))/g;
    $a =~ s/\b${prefix}result\s*=\s*strtof\s*\(([^;]*)\)/${prefix}result = aurocks_box_double((double)aurocks_strtof($1))/g;
    $a =~ s/\b${prefix}result\s*=\s*atof\s*\(([^;]*)\)/${prefix}result = aurocks_box_double(aurocks_atof($1))/g;
    # Compatibility rewrites for grammars written against older revisions.
    $a =~ s/\bbuild_number\s*\(\s*(${prefix}v\d+)\s*\)/build_number(*(double*)$1)/g;
    $a =~ s/\b(${prefix}v\d+)\s*->/((SList*)$1)->/g;
    return $a;
}

# ---------------------------------------------------------------------------
# The Aurocks runtime library: exactly 35 helper functions
# ---------------------------------------------------------------------------

my @AUROCKS_RUNTIME_API = qw(
    aurocks_alloc
    aurocks_calloc
    aurocks_realloc
    aurocks_free
    aurocks_strdup
    aurocks_strndup
    aurocks_memdup
    aurocks_strlen
    aurocks_strcmp
    aurocks_strncmp
    aurocks_strcasecmp
    aurocks_strchr
    aurocks_strrchr
    aurocks_strstr
    aurocks_strlcpy
    aurocks_strtol
    aurocks_strtoul
    aurocks_strtoll
    aurocks_strtoull
    aurocks_strtod
    aurocks_strtof
    aurocks_strtold
    aurocks_atoi
    aurocks_atol
    aurocks_atoll
    aurocks_atof
    aurocks_snprintf
    aurocks_vsnprintf
    aurocks_asprintf
    aurocks_vasprintf
    aurocks_box_double
    aurocks_unbox_double
    aurocks_box_long
    aurocks_unbox_long
    aurocks_box_str
);

sub emit_runtime_api {
    my $out = <<'C_CODE';
/* ------------------------------------------------------------------------ *
 * The Aurocks runtime library.
 *
 * Exactly 35 helper functions are provided, all prefixed with "aurocks_".
 * The manifest below lists them and a compile-time assertion checks the
 * count, so an accidental change to this list is caught by the compiler.
 *
 * The prototypes are emitted before the C prologue so that the prologue may
 * call any of these helpers.
 * ------------------------------------------------------------------------ */

void *aurocks_alloc(size_t size);
void *aurocks_calloc(size_t count, size_t size);
void *aurocks_realloc(void *ptr, size_t size);
void aurocks_free(void *ptr);
char *aurocks_strdup(const char *s);
char *aurocks_strndup(const char *s, size_t n);
void *aurocks_memdup(const void *src, size_t n);
size_t aurocks_strlen(const char *s);
int aurocks_strcmp(const char *a, const char *b);
int aurocks_strncmp(const char *a, const char *b, size_t n);
int aurocks_strcasecmp(const char *a, const char *b);
const char *aurocks_strchr(const char *s, int c);
const char *aurocks_strrchr(const char *s, int c);
const char *aurocks_strstr(const char *haystack, const char *needle);
size_t aurocks_strlcpy(char *dst, const char *src, size_t size);
long aurocks_strtol(const char *s, char **end, int base);
unsigned long aurocks_strtoul(const char *s, char **end, int base);
long long aurocks_strtoll(const char *s, char **end, int base);
unsigned long long aurocks_strtoull(const char *s, char **end, int base);
double aurocks_strtod(const char *s, char **end);
float aurocks_strtof(const char *s, char **end);
long double aurocks_strtold(const char *s, char **end);
int aurocks_atoi(const char *s);
long aurocks_atol(const char *s);
long long aurocks_atoll(const char *s);
double aurocks_atof(const char *s);
int aurocks_snprintf(char *buf, size_t size, const char *fmt, ...);
int aurocks_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
char *aurocks_asprintf(const char *fmt, ...);
char *aurocks_vasprintf(const char *fmt, va_list ap);
void *aurocks_box_double(double value);
double aurocks_unbox_double(const void *box);
void *aurocks_box_long(long value);
long aurocks_unbox_long(const void *box);
void *aurocks_box_str(const char *text);

enum { AUROCKS_RUNTIME_FUNCTION_COUNT = 35 };

const char *const aurocks_runtime_functions[AUROCKS_RUNTIME_FUNCTION_COUNT] = {
C_CODE
    $out .= "    \"" . $_ . "\",\n" for @AUROCKS_RUNTIME_API;
    $out .= <<'C_CODE';
};

typedef char aurocks_runtime_function_count_check[
    (sizeof(aurocks_runtime_functions) / sizeof(aurocks_runtime_functions[0]))
        == AUROCKS_RUNTIME_FUNCTION_COUNT ? 1 : -1];
C_CODE
    return $out;
}

sub emit_runtime_impl {
    my $out = <<'C_CODE';

/* --- memory management (7) -------------------------------------------- */
C_CODE
    $out .= <<'C_CODE';

void *aurocks_alloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (p && size) memset(p, 0, size);
    return p;
}

void *aurocks_calloc(size_t count, size_t size)
{
    if (count && size > (size_t)-1 / count) return NULL;
    return calloc(count ? count : 1, size ? size : 1);
}

void *aurocks_realloc(void *ptr, size_t size)
{
    if (!size) { free(ptr); return NULL; }
    return realloc(ptr, size);
}

void aurocks_free(void *ptr)
{
    free(ptr);
}

char *aurocks_strdup(const char *s)
{
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

char *aurocks_strndup(const char *s, size_t n)
{
    char *p;
    size_t len = 0;
    if (!s) return NULL;
    while (len < n && s[len]) len++;
    p = (char *)malloc(len + 1);
    if (!p) return NULL;
    if (len) memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

void *aurocks_memdup(const void *src, size_t n)
{
    void *p;
    if (!src && n) return NULL;
    p = malloc(n ? n : 1);
    if (!p) return NULL;
    if (n) memcpy(p, src, n);
    return p;
}

/* --- string handling (8) ---------------------------------------------- */

size_t aurocks_strlen(const char *s)
{
    return s ? strlen(s) : (size_t)0;
}

int aurocks_strcmp(const char *a, const char *b)
{
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}

int aurocks_strncmp(const char *a, const char *b, size_t n)
{
    if (n == 0) return 0;
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strncmp(a, b, n);
}

int aurocks_strcasecmp(const char *a, const char *b)
{
    unsigned char ca, cb;
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    do {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) return (int)ca - (int)cb;
    } while (ca != 0);
    return 0;
}

const char *aurocks_strchr(const char *s, int c)
{
    return s ? strchr(s, c) : NULL;
}

const char *aurocks_strrchr(const char *s, int c)
{
    return s ? strrchr(s, c) : NULL;
}

const char *aurocks_strstr(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return NULL;
    return strstr(haystack, needle);
}

size_t aurocks_strlcpy(char *dst, const char *src, size_t size)
{
    size_t srclen = src ? strlen(src) : (size_t)0;
    if (dst && size) {
        size_t n = (srclen < size - 1) ? srclen : size - 1;
        if (n && src) memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return srclen;
}

/* --- numeric conversion (11) ------------------------------------------ */

long aurocks_strtol(const char *s, char **end, int base)
{
    return s ? strtol(s, end, base) : 0L;
}

unsigned long aurocks_strtoul(const char *s, char **end, int base)
{
    return s ? strtoul(s, end, base) : 0UL;
}

long long aurocks_strtoll(const char *s, char **end, int base)
{
    return s ? strtoll(s, end, base) : 0LL;
}

unsigned long long aurocks_strtoull(const char *s, char **end, int base)
{
    return s ? strtoull(s, end, base) : 0ULL;
}

double aurocks_strtod(const char *s, char **end)
{
    return s ? strtod(s, end) : 0.0;
}

float aurocks_strtof(const char *s, char **end)
{
    return s ? (float)strtod(s, end) : 0.0f;
}

long double aurocks_strtold(const char *s, char **end)
{
    return s ? strtold(s, end) : 0.0L;
}

int aurocks_atoi(const char *s)
{
    return s ? atoi(s) : 0;
}

long aurocks_atol(const char *s)
{
    return s ? atol(s) : 0L;
}

long long aurocks_atoll(const char *s)
{
    return s ? atoll(s) : 0LL;
}

double aurocks_atof(const char *s)
{
    return s ? atof(s) : 0.0;
}

/* --- formatted output (4) --------------------------------------------- */

int aurocks_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

int aurocks_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    return vsnprintf(buf, size, fmt, ap);
}

char *aurocks_vasprintf(const char *fmt, va_list ap)
{
    va_list copy;
    char *buf;
    int n;

    va_copy(copy, ap);
    n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0) return NULL;
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    return buf;
}

char *aurocks_asprintf(const char *fmt, ...)
{
    va_list ap;
    char *buf;
    va_start(ap, fmt);
    buf = aurocks_vasprintf(fmt, ap);
    va_end(ap);
    return buf;
}

/* --- semantic value boxing (5) ---------------------------------------- */

void *aurocks_box_double(double value)
{
    double *p = (double *)malloc(sizeof *p);
    if (p) *p = value;
    return p;
}

double aurocks_unbox_double(const void *box)
{
    return box ? *(const double *)box : 0.0;
}

void *aurocks_box_long(long value)
{
    long *p = (long *)malloc(sizeof *p);
    if (p) *p = value;
    return p;
}

long aurocks_unbox_long(const void *box)
{
    return box ? *(const long *)box : 0L;
}

void *aurocks_box_str(const char *text)
{
    return (void *)aurocks_strdup(text);
}
C_CODE
    return $out;
}

# The parser types are emitted before the C prologue so that the prologue
# may use GLRParser.
sub emit_parser_types {
    my $out = <<'C_CODE';
/* ------------------------------------------------------------------------ *
 * Parser types.
 * ------------------------------------------------------------------------ */

typedef struct GLRParser {
  const char *begin;   /* start of the input buffer          */
  const char *s;       /* current input position             */
  const char *end;     /* end of the input buffer            */
  const char *error;   /* furthest failure seen in this state */
} GLRParser;

enum { GLR_RX_MATCH, GLR_RX_SPLIT, GLR_RX_JMP, GLR_RX_CHAR,
       GLR_RX_CLASS, GLR_RX_ANY, GLR_RX_EOL, GLR_RX_BOL, GLR_RX_WORDB };

struct GLRRegexState { int kind; int out1; int out2; unsigned int arg; };

struct GLRRegexCtx { const char *base; size_t len; size_t start; size_t pos; };

#ifndef GLR_MAX_DEPTH
#define GLR_MAX_DEPTH 512
#endif
C_CODE
    return $out;
}

# ---------------------------------------------------------------------------
# Scannerless matching engine
# ---------------------------------------------------------------------------

sub emit_regex_matcher {
    my ($desc) = @_;
    my $nfa = regex_compile_nfa($desc->{ast}, $desc);
    my $id     = $desc->{id};
    my $suffix = uc $id;
    my $n      = scalar @{ $nfa->{states} || [] };
    my $states = "glr_rx_${id}_states";
    my $class  = "glr_rx_${id}_class";
    my $count  = "GLR_RX_${suffix}_COUNT";

    my $out = "";
    $out .= "enum { ${count} = $n };\n";
    $out .= "static const struct GLRRegexState ${states}[${count}] = {\n";
    my %kind = (
        match  => 'GLR_RX_MATCH',
        split  => 'GLR_RX_SPLIT',
        jmp    => 'GLR_RX_JMP',
        char   => 'GLR_RX_CHAR',
        class  => 'GLR_RX_CLASS',
        any    => 'GLR_RX_ANY',
        eol    => 'GLR_RX_EOL',
        bol    => 'GLR_RX_BOL',
        wordb  => 'GLR_RX_WORDB',
    );
    for my $st (@{ $nfa->{states} }) {
        $out .= sprintf("  { %s, %d, %d, %u },\n",
            $kind{ $st->{kind} }, $st->{out1}, $st->{out2}, $st->{arg});
    }
    $out .= "};\n";

    $out .= "static int ${class}(unsigned int cls, unsigned char ch) {\n";
    if (@{ $nfa->{classes} || [] }) {
        $out .= "  switch (cls) {\n";
        for my $i (0 .. $#{ $nfa->{classes} }) {
            $out .= "    case $i: return " . regex_class_expr($nfa->{classes}[$i], 'ch') . ";\n";
        }
        $out .= "  }\n  return 0;\n";
    }
    else {
        $out .= "  (void)cls; (void)ch; return 0;\n";
    }
    $out .= "}\n";

    $out .= "static int glr_rx_${id}(GLRParser *p, size_t *out_n) {\n";
    $out .= "  int clist[${count}], nlist[${count}];\n";
    $out .= "  unsigned char seen[${count}], seen2[${count}];\n";
    $out .= "  int cn = 0, nn = 0, i;\n";
    $out .= "  size_t len = (size_t)(p->end - p->begin);\n";
    $out .= "  size_t start = (size_t)(p->s - p->begin);\n";
    $out .= "  size_t pos = start, best = start;\n";
    $out .= "  int matched = 0;\n";
    $out .= "  struct GLRRegexCtx ctx;\n";
    $out .= "  ctx.base = p->begin; ctx.len = len; ctx.start = start; ctx.pos = start;\n";
    $out .= "  memset(seen, 0, sizeof seen);\n";
    $out .= "  glr_rx_addstate(${states}, ${count}, $nfa->{start}, clist, &cn, seen, &ctx);\n";
    $out .= "  for (;;) {\n";
    $out .= "    for (i = 0; i < cn; i++) if (${states}[clist[i]].kind == GLR_RX_MATCH) { best = pos; matched = 1; }\n";
    $out .= "    if (pos >= len) break;\n";
    $out .= "    {\n";
    $out .= "      unsigned char ch = (unsigned char)p->begin[pos];\n";
    $out .= "      nn = 0; memset(seen2, 0, sizeof seen2);\n";
    $out .= "      ctx.pos = pos + 1;\n";
    $out .= "      for (i = 0; i < cn; i++) {\n";
    $out .= "        int q = clist[i];\n";
    $out .= "        switch (${states}[q].kind) {\n";
    $out .= "        case GLR_RX_CHAR:\n";
    $out .= "          if ((unsigned char)${states}[q].arg == ch) glr_rx_addstate(${states}, ${count}, ${states}[q].out1, nlist, &nn, seen2, &ctx);\n";
    $out .= "          break;\n";
    $out .= "        case GLR_RX_CLASS:\n";
    $out .= "          if (${class}(${states}[q].arg, ch)) glr_rx_addstate(${states}, ${count}, ${states}[q].out1, nlist, &nn, seen2, &ctx);\n";
    $out .= "          break;\n";
    $out .= "        case GLR_RX_ANY:\n";
    $out .= "          if (ch != '\\n') glr_rx_addstate(${states}, ${count}, ${states}[q].out1, nlist, &nn, seen2, &ctx);\n";
    $out .= "          break;\n";
    $out .= "        default:\n";
    $out .= "          break;\n";
    $out .= "        }\n";
    $out .= "      }\n";
    $out .= "    }\n";
    $out .= "    if (nn == 0) break;\n";
    $out .= "    for (i = 0; i < nn; i++) clist[i] = nlist[i];\n";
    $out .= "    cn = nn;\n";
    $out .= "    pos++;\n";
    $out .= "  }\n";
    $out .= "  if (!matched) return 0;\n";
    $out .= "  *out_n = best - start;\n";
    $out .= "  return 1;\n";
    $out .= "}\n";
    return $out;
}

sub emit_matching_engine {
    my ($terms, $skip_desc) = @_;
    my $out = "";

    $out .= "/* ------------------------------------------------------------------------ *\n";
    $out .= " * Scannerless matching engine.\n";
    $out .= " * ------------------------------------------------------------------------ */\n\n";
    $out .= "static const char *glr_error_pos = NULL;\n\n";
    $out .= "static int glr_depth = 0;\n\n";

    $out .= "static int glr_rx_isword(unsigned char c) {\n";
    $out .= "  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||\n";
    $out .= "         (c >= 'a' && c <= 'z') || c == '_';\n";
    $out .= "}\n\n";

    $out .= "static void glr_rx_addstate(const struct GLRRegexState *st, int count, int s,\n";
    $out .= "                            int *list, int *n, unsigned char *seen,\n";
    $out .= "                            const struct GLRRegexCtx *ctx) {\n";
    $out .= "  if (s < 0 || s >= count || seen[s]) return;\n";
    $out .= "  seen[s] = 1;\n";
    $out .= "  switch (st[s].kind) {\n";
    $out .= "  case GLR_RX_SPLIT:\n";
    $out .= "    glr_rx_addstate(st, count, st[s].out1, list, n, seen, ctx);\n";
    $out .= "    glr_rx_addstate(st, count, st[s].out2, list, n, seen, ctx);\n";
    $out .= "    break;\n";
    $out .= "  case GLR_RX_JMP:\n";
    $out .= "    glr_rx_addstate(st, count, st[s].out1, list, n, seen, ctx);\n";
    $out .= "    break;\n";
    $out .= "  case GLR_RX_EOL:\n";
    $out .= "    if (ctx->pos >= ctx->len) glr_rx_addstate(st, count, st[s].out1, list, n, seen, ctx);\n";
    $out .= "    break;\n";
    $out .= "  case GLR_RX_BOL:\n";
    $out .= "    if (ctx->pos == ctx->start) glr_rx_addstate(st, count, st[s].out1, list, n, seen, ctx);\n";
    $out .= "    break;\n";
    $out .= "  case GLR_RX_WORDB: {\n";
    $out .= "    int before = 0, after = 0, at;\n";
    $out .= "    if (ctx->pos > 0) before = glr_rx_isword((unsigned char)ctx->base[ctx->pos - 1]);\n";
    $out .= "    if (ctx->pos < ctx->len) after = glr_rx_isword((unsigned char)ctx->base[ctx->pos]);\n";
    $out .= "    at = (before != after);\n";
    $out .= "    if (at == (int)st[s].arg) glr_rx_addstate(st, count, st[s].out1, list, n, seen, ctx);\n";
    $out .= "    break;\n";
    $out .= "  }\n";
    $out .= "  default:\n";
    $out .= "    list[(*n)++] = s;\n";
    $out .= "    break;\n";
    $out .= "  }\n";
    $out .= "}\n\n";

    if (defined $skip_desc) {
        $out .= emit_regex_matcher($skip_desc);
        $out .= "static void glr_skip(GLRParser *p) {\n";
        $out .= "  size_t n = 0;\n";
        $out .= "  while (p->s < p->end && glr_rx_skip(p, &n) && n > 0) p->s += n;\n";
        $out .= "}\n\n";
    }
    else {
        $out .= "static void glr_skip(GLRParser *p) { (void)p; }\n\n";
    }

    for my $term (@$terms) {
        $out .= emit_regex_matcher($term);
    }

    $out .= "static void glr_fail(GLRParser *p) {\n";
    $out .= "  if (!p->error || p->s > p->error) p->error = p->s;\n";
    $out .= "  if (!glr_error_pos || p->s > glr_error_pos) glr_error_pos = p->s;\n";
    $out .= "}\n\n";

    $out .= "static int glr_match(GLRParser *p, int term, char **tok) {\n";
    $out .= "  size_t n = 0;\n";
    $out .= "  const char *at = p->s;\n";
    $out .= "  char *x = NULL;\n";
    $out .= "  switch (term) {\n";
    for my $term (@$terms) {
        my $id = $term->{id};
        $out .= "  case AUROCKS_TERM_${id}:\n";
        $out .= "    if (!glr_rx_${id}(p, &n) || n == 0) return 0;\n";
        $out .= "    x = (char *)malloc(n + 1);\n";
        $out .= "    if (!x) return 0;\n";
        $out .= "    memcpy(x, at, n);\n";
        $out .= "    x[n] = '\\0';\n";
        $out .= "    p->s += n;\n";
        $out .= "    *tok = x;\n";
        $out .= "    return 1;\n";
    }
    $out .= "  default:\n";
    $out .= "    break;\n";
    $out .= "  }\n";
    $out .= "  return 0;\n";
    $out .= "}\n\n";
    return $out;
}

# ---------------------------------------------------------------------------
# Generated productions
# ---------------------------------------------------------------------------

sub emit_value_decls {
    my ($rule, $indent, $from, $to) = @_;
    my $out = '';
    my $rhs = $rule->{rhs};
    for my $i ($from .. $to) {
        next if $i > $#{$rhs};
        my $s = $rhs->[$i];
        next if $s eq '%empty';
        if ($rule->{nonterm}{$s}) {
            $out .= "${indent}void *v" . ($i + 1) . " = NULL;\n";
        }
        else {
            $out .= "${indent}char *v" . ($i + 1) . " = NULL;\n";
        }
    }
    return $out;
}

sub emit_match_code {
    my ($rule, $indent, $pvar, $tokvar, $term_ids, $from, $flag) = @_;
    $from = 0 unless defined $from;
    $flag = 'ok' unless defined $flag;
    my $out = '';
    my $rhs = $rule->{rhs};
    for my $i ($from .. $#{$rhs}) {
        my $s = $rhs->[$i];
        next if $s eq '%empty';
        if ($rule->{nonterm}{$s}) {
            $out .= "${indent}if (${flag} && !glr_${s}(&${pvar}, &v" . ($i + 1) . ")) ${flag} = 0;\n";
        }
        else {
            my $id = $term_ids->{$s};
            die "unknown symbol [$s]: it is neither a production, a quoted"
              . " literal nor a /regular expression/\n" unless defined $id;
            $out .= "${indent}if (${flag}) { glr_skip(&${pvar});\n";
            $out .= "${indent}  if (!glr_match(&${pvar}, AUROCKS_TERM_${id}, &v" . ($i + 1) . ")) { glr_fail(&${pvar}); ${flag} = 0; }\n";
            $out .= "${indent}  else " . $tokvar . " = v" . ($i + 1) . ";\n"
                if defined $tokvar;
            $out .= "${indent}}\n";
        }
    }
    return $out;
}

sub emit_free_tokens {
    my ($rule, $indent) = @_;
    my $out = '';
    my $rhs = $rule->{rhs};
    for my $i (0 .. $#{$rhs}) {
        my $s = $rhs->[$i];
        next if $s eq '%empty';
        next if $rule->{nonterm}{$s};
        $out .= "${indent}aurocks_free(v" . ($i + 1) . ");\n";
    }
    return $out;
}

sub emit_nonterminal {
    my ($nt, $seed, $left, $term_ids) = @_;
    my @seed = @$seed;
    my @left = @$left;
    my $out = '';

    $out .= "static int glr_${nt}(GLRParser *p, void **out) {\n";
    $out .= "  GLRParser save = *p;\n";
    $out .= "  if (glr_depth >= GLR_MAX_DEPTH) { glr_fail(p); return 0; }\n";
    $out .= "  glr_depth++;\n";

    if (@left) {
        for my $r (@seed) {
            my $want_token = action_uses_token($r->{action});
            $out .= "  { GLRParser attempt = save;\n";
            $out .= "    void *result = NULL;\n";
            $out .= "    char *token = NULL;\n" if $want_token;
            $out .= emit_value_decls($r, '    ', 0, $#{$r->{rhs}});
            $out .= "    int ok = 1;\n";
            $out .= emit_match_code($r, '    ', 'attempt', ($want_token ? 'token' : undef), $term_ids);
            $out .= "    if (ok) {\n";
            $out .= "      glr_skip(&attempt);\n";
            $out .= "      { " . action_c($r->{action}, 'token') . " }\n";
            $out .= emit_release_unused($r, '      ');
            $out .= "      void *acc = result;\n";
            $out .= "      for (;;) {\n";
            $out .= "        GLRParser step = attempt;\n";
            $out .= "        int advanced = 0;\n";
            for my $lr (@left) {
                my $want_tail = action_uses_token($lr->{action});
                $out .= "        if (!advanced) {\n";
                $out .= "          GLRParser tail = step;\n";
                $out .= "          int tail_ok = 1;\n";
                $out .= "          char *tail_token = NULL;\n" if $want_tail;
                $out .= emit_value_decls($lr, '          ', 1, $#{$lr->{rhs}});
                $out .= emit_match_code($lr, '          ', 'tail', ($want_tail ? 'tail_token' : undef), $term_ids, 1, 'tail_ok');
                $out .= "          if (!tail_ok) {\n";
                $out .= emit_free_tokens($lr, '            ');
                $out .= "          } else {\n";
                $out .= "            void *result = acc;\n";
                $out .= "            void *v1 = acc;\n";
                $out .= "            { " . action_c($lr->{action}, 'tail_token') . " }\n";
                $out .= emit_release_unused($lr, '            ');
                $out .= "            acc = result;\n";
                $out .= "            step = tail;\n";
                $out .= "            advanced = 1;\n";
                $out .= "          }\n";
                $out .= "        }\n";
            }
            $out .= "        if (!advanced) break;\n";
            $out .= "        if (step.s == attempt.s) break;\n";
            $out .= "        attempt = step;\n";
            $out .= "      }\n";
            $out .= "      *p = attempt;\n";
            $out .= "      *out = acc;\n";
            $out .= "      glr_depth--;\n";
            $out .= "      return 1;\n";
            $out .= "    }\n";
            $out .= emit_free_tokens($r, '    ');
            $out .= "  }\n";
        }
    }
    else {
        for my $r (@seed) {
            my $want_token = action_uses_token($r->{action});
            $out .= "  { GLRParser attempt = save;\n";
            $out .= "    void *result = NULL;\n";
            $out .= "    char *token = NULL;\n" if $want_token;
            $out .= emit_value_decls($r, '    ', 0, $#{$r->{rhs}});
            $out .= "    int ok = 1;\n";
            $out .= emit_match_code($r, '    ', 'attempt', ($want_token ? 'token' : undef), $term_ids);
            $out .= "    if (ok) {\n";
            $out .= "      glr_skip(&attempt);\n";
            $out .= "      { " . action_c($r->{action}, 'token') . " }\n";
            $out .= emit_release_unused($r, '      ');
            $out .= "      *p = attempt;\n";
            $out .= "      *out = result;\n";
            $out .= "      glr_depth--;\n";
            $out .= "      return 1;\n";
            $out .= "    }\n";
            $out .= emit_free_tokens($r, '    ');
            $out .= "  }\n";
        }
    }

    $out .= "  glr_depth--;\n";
    $out .= "  *p = save;\n";
    $out .= "  glr_fail(p);\n";
    $out .= "  return 0;\n";
    $out .= "}\n";
    return $out;
}

# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

sub emit {
    my ($grammar, $api_name) = @_;
    my $start = $grammar->{start};
    my $d     = $grammar->{directives};
    my $rules = $grammar->{rules};
    my $nonterm = $grammar->{nonterm};

    $api_name ||= "parse_$start";

    # Reject grammars that would recurse without consuming input.
    my $nullable = compute_nullable($rules, $nonterm);
    check_recursion($rules, $nonterm, $nullable);

    my %by;
    push @{ $by{ $_->{lhs} } }, $_ for @$rules;
    my @nts = sort keys %$nonterm;

    # --- %skip ---------------------------------------------------------
    my $skip = '';
    if ($d->{skip} && $d->{skip}[0]) {
        my $v = $d->{skip}[0];
        if ($v =~ m!\A\s*/!s) {
            my $body = substr($v, index($v, '/') + 1);
            $body =~ s!/\s*\z!!s;
            $skip = $body;
        }
        else {
            $skip = $v;
        }
    }

    my ($terms, $skip_desc, $term_ids) = collect_terminals($rules, $skip);
    my $matching_engine = emit_matching_engine($terms, $skip_desc);

    # --- productions ----------------------------------------------------
    my %rule_nonterm = (%$nonterm);
    for my $r (@$rules) { $r->{nonterm} = \%rule_nonterm; }

    my @protos;
    my @bodies;
    for my $nt (@nts) {
        push @protos, "static int glr_${nt}(GLRParser *p, void **out);\n";
    }
    for my $nt (@nts) {
        my @rules = sort { $b->{dprec} <=> $a->{dprec} } @{ $by{$nt} || [] };

        my @left = grep { @{ $_->{rhs} } && $_->{rhs}[0] eq $nt } @rules;
        my @seed = grep { !(@{ $_->{rhs} } && $_->{rhs}[0] eq $nt) } @rules;
        if (@left && !@seed) {
            die "production [$nt] has no non-left-recursive seed\n";
        }
        push @bodies, emit_nonterminal($nt, \@seed, \@left, $term_ids);
    }

    # --- emit -----------------------------------------------------------
    print "/* Generated by aurocks.pl -- do not edit. */\n";
    print "/* Scannerless GLR/SPPF controls accepted:";
    print " %start %skip %prefer %avoid %longest %dprec %left %right %nonassoc";
    print " %merge %reject %cut %expect %glr %sppf %ambiguity %resolve %priority";
    print " %inline %layout %lexer %error. */\n\n";
    print "#include <stddef.h>\n";
    print "#include <stdlib.h>\n";
    print "#include <string.h>\n";
    print "#include <stdarg.h>\n";
    print "#include <stdio.h>\n\n";
    print "enum { GLR_SPPF_PREFER = 1, GLR_SPPF_AVOID = 2, GLR_SPPF_LONGEST = 4, GLR_SPPF_DPREC = 8 };\n\n";

    print emit_runtime_api();
    print "\n";
    print emit_parser_types();
    print "\n";

    print $grammar->{prologue}, "\n\n" if length $grammar->{prologue};

    print emit_runtime_impl();
    print "\n";

    if (@$terms) {
        print "enum { ", join(", ", map { "AUROCKS_TERM_$_->{id} = $_->{id}" } @$terms), " };\n\n";
    }

    print $matching_engine;

    print "/* ---- forward declarations ---- */\n\n";
    print @protos;
    print "\n/* ---- generated productions ---- */\n";
    print @bodies;

    print "\n/* ---- public entry points ---- */\n\n";
    print "void *${api_name}(const char *input);\n";
    print "void *yyparse(const char *input);\n";
    print "const char *${api_name}_error(void);\n";
    print "void *parse_${start}(const char *input);\n" if $api_name ne "parse_${start}";
    print "const char *parse_${start}_error(void);\n" if $api_name ne "parse_${start}";
    print "\n";
    print "void *${api_name}(const char *input) {\n";
    print "  GLRParser p;\n";
    print "  void *out = NULL;\n";
    print "  glr_error_pos = NULL;\n";
    print "  if (!input) return NULL;\n";
    print "  p.begin = input; p.s = input; p.end = input + strlen(input); p.error = NULL;\n";
    print "  if (!glr_${start}(&p, &out)) return NULL;\n";
    print "  glr_skip(&p);\n";
    print "  if (p.s != p.end) { glr_error_pos = p.s; return NULL; }\n";
    print "  glr_error_pos = NULL;\n";
    print "  return out;\n";
    print "}\n\n";
    print "void *yyparse(const char *input) { return ${api_name}(input); }\n\n";
    print "const char *${api_name}_error(void) { return glr_error_pos; }\n";
    if ($api_name ne "parse_${start}") {
        print "void *parse_${start}(const char *input) { return ${api_name}(input); }\n";
        print "const char *parse_${start}_error(void) { return glr_error_pos; }\n";
    }

    print "\n", $grammar->{epilogue};
    print "\n";
}

sub main {
    my $entrypoint;
    my @args;
    while (@ARGV) {
        my $a = shift @ARGV;
        if ($a eq '--entrypoint' || $a eq '-e') {
            $entrypoint = shift @ARGV // die "--entrypoint requires a name\n";
        }
        elsif ($a eq '--help' || $a eq '-h') {
            print "usage: $0 [--entrypoint NAME] grammar.g\n";
            print "\n";
            print "  --entrypoint NAME  name of the generated entry point\n";
            print "                    (default: parse_<start production>)\n";
            return;
        }
        elsif ($a eq '--version' || $a eq '-V') {
            print "aurocks.pl 2.0\n";
            return;
        }
        else {
            push @args, $a;
        }
    }
    die "usage: $0 [--entrypoint NAME] grammar.g\n" unless @args == 1;
    my ($path) = @args;
    my $grammar = parse_grammar(slurp($path));
    die "invalid entrypoint name\n"
        if defined($entrypoint) && $entrypoint !~ /^[A-Za-z_]\w*$/;
    die "entrypoint name conflicts with generated API\n"
        if defined($entrypoint) && ($entrypoint eq 'yyparse'
            || $entrypoint eq "parse_$grammar->{start}_error");
    emit($grammar, $entrypoint);
}

main();
