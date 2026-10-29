/**
 * @file mulib-regex.h
 * @brief Self-contained Thompson-NFA regular expressions over arbitrary bytes.
 * Define MULIB_IMPL_REGEX in one C11 translation unit. No module dependencies.
 */
#ifndef MULIB_REGEX_H
#define MULIB_REGEX_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_regex mulib_regex_t;
typedef struct mulib_regex_error {
    size_t offset, line, column;
    const char *message;
} mulib_regex_error_t;
typedef struct mulib_regex_match { size_t offset, length; } mulib_regex_match_t;
/** @brief Compile an owned byte regex, or return NULL with a static diagnostic.
 * Syntax: literals, . ^ $, classes/ranges/negation, grouping, |, ? * +,
 * {m}, {m,}, {m,n}, ASCII digit/space/word classes and complements, byte escapes.
 * Groups do not capture. Backreferences, lazy quantifiers and lookaround are
 * unsupported. Dot excludes LF; anchors refer to the entire input.
 * Compilation is bounded by independent syntax, state, depth and emission-work
 * limits; exceeding the work budget reports "regex compilation work limit".
 */
mulib_regex_t *mulib_regex_compile(const char *pattern, size_t length, mulib_regex_error_t *error);
/** @brief Release a compiled regex. NULL is harmless. */
void mulib_regex_destroy(mulib_regex_t *regex);
/** @brief Find the leftmost-longest match at/after start (including empty matches).
 * Return 1 for a match, 0 for none, -1 for invalid arguments/allocation failure.
 * A non-NULL match is cleared on failure; input is length-delimited binary data.
 */
int mulib_regex_search(const mulib_regex_t *regex, const char *source, size_t length,
                       size_t start, mulib_regex_match_t *match);
/** @brief Require a match spanning the entire input; returns 1, 0, or -1. */
int mulib_regex_full_match(const mulib_regex_t *regex, const char *source, size_t length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_REGEX_H */

#if defined(MULIB_IMPL_REGEX) && !defined(MULIB_REGEX_IMPLEMENTED)
#define MULIB_REGEX_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef MULIB_REGEX_MAX_DEPTH
#define MULIB_REGEX_MAX_DEPTH 128
#endif
#ifndef MULIB_REGEX_MAX_STATES
#define MULIB_REGEX_MAX_STATES 100000
#endif
#ifndef MULIB_REGEX_MAX_REPEAT
#define MULIB_REGEX_MAX_REPEAT 1000
#endif
#ifndef MULIB_REGEX_MAX_EMIT_STEPS
#define MULIB_REGEX_MAX_EMIT_STEPS 1000000
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_regex_node_kind_ {
    MULIB_REGEX_SET_, MULIB_REGEX_ANY_, MULIB_REGEX_BOL_, MULIB_REGEX_EOL_,
    MULIB_REGEX_SEQUENCE_, MULIB_REGEX_CHOICE_, MULIB_REGEX_REPEAT_
} mulib_regex_node_kind_;
typedef struct mulib_regex_node_ {
    mulib_regex_node_kind_ kind;
    struct mulib_regex_node_ *next, **children;
    size_t count, min, max, offset;
    unsigned char bits[32];
} mulib_regex_node_;
typedef enum mulib_regex_opcode_ {
    MULIB_REGEX_OP_SET_, MULIB_REGEX_OP_ANY_, MULIB_REGEX_OP_BOL_,
    MULIB_REGEX_OP_EOL_, MULIB_REGEX_OP_SPLIT_, MULIB_REGEX_OP_MATCH_
} mulib_regex_opcode_;
typedef struct mulib_regex_state_ {
    mulib_regex_opcode_ op;
    size_t a, b;
    unsigned char bits[32];
} mulib_regex_state_;
struct mulib_regex {
    mulib_regex_state_ *states;
    size_t count, capacity, start;
};
typedef struct mulib_regex_reader_ {
    const char *source;
    size_t length, pos, depth, nodes_count, emit_steps;
    mulib_regex_error_t *error;
    mulib_regex_node_ *nodes;
    mulib_regex_t *regex;
    int failed;
} mulib_regex_reader_;
static void mulib_regex_fail_(mulib_regex_reader_ *r, size_t at, const char *message) {
    size_t i;
    if (r->failed) return;
    r->failed = 1;
    if (!r->error) return;
    r->error->offset = at; r->error->line = r->error->column = 1; r->error->message = message;
    for (i = 0; i < at; ++i) {
        if (r->source[i] == '\n') { ++r->error->line; r->error->column = 1; } else ++r->error->column;
    }
}
static mulib_regex_node_ *mulib_regex_node_new_(mulib_regex_reader_ *r, mulib_regex_node_kind_ kind) {
    mulib_regex_node_ *n;
    if (r->nodes_count >= (size_t)MULIB_REGEX_MAX_STATES) {
        mulib_regex_fail_(r, r->pos, "regex syntax node limit"); return NULL;
    }
    n = (mulib_regex_node_ *)calloc(1, sizeof *n);
    if (!n) { mulib_regex_fail_(r, r->pos, "out of memory"); return NULL; }
    n->kind = kind; n->offset = r->pos; n->next = r->nodes; r->nodes = n; ++r->nodes_count; return n;
}
static void mulib_regex_bit_(unsigned char bits[32], unsigned c) { bits[c >> 3] |= (unsigned char)(1u << (c & 7)); }
static int mulib_regex_hex_(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* Read one class atom or escaped byte; singleton is -1 for shorthand sets. */
static int mulib_regex_byte_(mulib_regex_reader_ *r, unsigned char bits[32], int *singleton) {
    unsigned char c; unsigned k;
    memset(bits, 0, 32); *singleton = -1;
    if (r->pos == r->length) { mulib_regex_fail_(r, r->pos, "truncated escape or class"); return 0; }
    c = (unsigned char)r->source[r->pos++];
    if (c == '\\') {
        if (r->pos == r->length) { mulib_regex_fail_(r, r->pos, "truncated escape"); return 0; }
        c = (unsigned char)r->source[r->pos++];
        switch (c) {
        case 'd': case 'D': case 's': case 'S': case 'w': case 'W': {
            unsigned char lower = (unsigned char)(c | 32);
            for (k = 0; k < 256; ++k) {
                int yes = lower == 'd' ? k >= '0' && k <= '9' :
                    lower == 'w' ? (k >= '0' && k <= '9') || (k >= 'a' && k <= 'z') ||
                                    (k >= 'A' && k <= 'Z') || k == '_' :
                    k == ' ' || k == '\t' || k == '\n' || k == '\r' || k == '\f' || k == '\v';
                if (c < 'a') yes = !yes;
                if (yes) mulib_regex_bit_(bits, k);
            }
            return 1;
        }
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        case 'f': c = '\f'; break;
        case 'v': c = '\v'; break;
        case '0': c = 0; break;
        case 'x': {
            int a, b;
            if (r->length - r->pos < 2 || (a = mulib_regex_hex_((unsigned char)r->source[r->pos])) < 0 ||
                (b = mulib_regex_hex_((unsigned char)r->source[r->pos + 1])) < 0) {
                mulib_regex_fail_(r, r->pos, "expected two hexadecimal digits"); return 0;
            }
            c = (unsigned char)(a * 16 + b); r->pos += 2; break;
        }
        case '\\': case '.': case '^': case '$': case '|': case '?': case '*': case '+':
        case '(': case ')': case '[': case ']': case '{': case '}': case '-': break;
        default: mulib_regex_fail_(r, r->pos - 1, "unsupported regex escape"); return 0;
        }
    }
    *singleton = c; mulib_regex_bit_(bits, c); return 1;
}
static mulib_regex_node_ *mulib_regex_expression_(mulib_regex_reader_ *r);
static mulib_regex_node_ *mulib_regex_primary_(mulib_regex_reader_ *r) {
    mulib_regex_node_ *n;
    unsigned char c = (unsigned char)r->source[r->pos];
    if (c == '(') {
        ++r->pos;
        if (r->depth >= (size_t)MULIB_REGEX_MAX_DEPTH) { mulib_regex_fail_(r, r->pos, "regex nesting limit"); return NULL; }
        ++r->depth; n = mulib_regex_expression_(r); --r->depth;
        if (!n) return NULL;
        if (r->pos == r->length || r->source[r->pos] != ')') { mulib_regex_fail_(r, r->pos, "expected closing parenthesis"); return NULL; }
        ++r->pos; return n;
    }
    if (c == '*' || c == '+' || c == '?' || c == '{' || c == '}' || c == ']') {
        mulib_regex_fail_(r, r->pos, "unexpected regex metacharacter"); return NULL;
    }
    n = mulib_regex_node_new_(r, MULIB_REGEX_SET_); if (!n) return NULL;
    if (c == '.' || c == '^' || c == '$') {
        ++r->pos;
        n->kind = c == '.' ? MULIB_REGEX_ANY_ : c == '^' ? MULIB_REGEX_BOL_ : MULIB_REGEX_EOL_;
        return n;
    }
    if (c == '[') {
        int negate = 0, any = 0;
        ++r->pos;
        if (r->pos < r->length && r->source[r->pos] == '^') { negate = 1; ++r->pos; }
        while (r->pos < r->length && r->source[r->pos] != ']') {
            unsigned char bits[32]; int lo, hi; size_t k;
            if (r->length - r->pos >= 2 && r->source[r->pos] == '[' && r->source[r->pos + 1] == ':') {
                mulib_regex_fail_(r, r->pos, "POSIX named classes are unsupported"); return NULL;
            }
            if (!mulib_regex_byte_(r, bits, &lo)) return NULL;
            if (r->length - r->pos >= 2 && r->source[r->pos] == '-' && r->source[r->pos + 1] != ']') {
                unsigned char other[32]; ++r->pos;
                if (!mulib_regex_byte_(r, other, &hi)) return NULL;
                if (lo < 0 || hi < 0 || lo > hi) { mulib_regex_fail_(r, r->pos, "invalid character range"); return NULL; }
                memset(bits, 0, sizeof bits);
                for (k = (size_t)lo; k <= (size_t)hi; ++k) mulib_regex_bit_(bits, (unsigned)k);
            }
            for (k = 0; k < sizeof bits; ++k) n->bits[k] |= bits[k];
            any = 1;
        }
        if (r->pos == r->length || !any) { mulib_regex_fail_(r, r->pos, "unterminated or empty byte class"); return NULL; }
        ++r->pos;
        if (negate) { size_t k; for (k = 0; k < sizeof n->bits; ++k) n->bits[k] = (unsigned char)~n->bits[k]; }
        return n;
    }
    { int singleton; if (!mulib_regex_byte_(r, n->bits, &singleton)) return NULL; }
    return n;
}
static int mulib_regex_count_(mulib_regex_reader_ *r, size_t *out) {
    size_t value = 0, begin = r->pos;
    while (r->pos < r->length && r->source[r->pos] >= '0' && r->source[r->pos] <= '9') {
        unsigned d = (unsigned)(r->source[r->pos++] - '0');
        if (value > (SIZE_MAX - d) / 10) { mulib_regex_fail_(r, begin, "repeat count overflow"); return 0; }
        value = value * 10 + d;
    }
    if (begin == r->pos) { mulib_regex_fail_(r, begin, "expected repeat count"); return 0; }
    if (value > (size_t)MULIB_REGEX_MAX_REPEAT) { mulib_regex_fail_(r, begin, "repeat count limit"); return 0; }
    *out = value; return 1;
}
static mulib_regex_node_ *mulib_regex_quantified_(mulib_regex_reader_ *r) {
    mulib_regex_node_ *child = mulib_regex_primary_(r), *n;
    size_t min, max;
    char c;
    if (!child || r->pos == r->length) return child;
    c = r->source[r->pos];
    if (c != '*' && c != '+' && c != '?' && c != '{') return child;
    ++r->pos; min = c == '+' ? 1 : 0; max = c == '?' ? 1 : SIZE_MAX;
    if (c == '{') {
        if (!mulib_regex_count_(r, &min)) return NULL;
        max = min;
        if (r->pos < r->length && r->source[r->pos] == ',') {
            ++r->pos; max = SIZE_MAX;
            if (r->pos < r->length && r->source[r->pos] != '}') if (!mulib_regex_count_(r, &max)) return NULL;
        }
        if (r->pos == r->length || r->source[r->pos] != '}' || min > max) {
            mulib_regex_fail_(r, r->pos, "invalid repeat bounds or missing brace"); return NULL;
        }
        ++r->pos;
    }
    if (r->pos < r->length && (r->source[r->pos] == '*' || r->source[r->pos] == '+' ||
                              r->source[r->pos] == '?' || r->source[r->pos] == '{')) {
        mulib_regex_fail_(r, r->pos, "multiple, lazy or possessive quantifiers are unsupported"); return NULL;
    }
    n = mulib_regex_node_new_(r, MULIB_REGEX_REPEAT_); if (!n) return NULL;
    n->children = (mulib_regex_node_ **)malloc(sizeof *n->children);
    if (!n->children) { mulib_regex_fail_(r, r->pos, "out of memory"); return NULL; }
    n->children[0] = child; n->count = 1; n->min = min; n->max = max; return n;
}
static int mulib_regex_push_(mulib_regex_reader_ *r, mulib_regex_node_ ***items, size_t *count,
                              size_t *capacity, mulib_regex_node_ *child) {
    if (*count == *capacity) {
        size_t cap = *capacity ? *capacity : 8; mulib_regex_node_ **p;
        if (*capacity) {
            if (cap > SIZE_MAX / 2) { mulib_regex_fail_(r, r->pos, "regex allocation overflow"); return 0; }
            cap *= 2;
        }
        if (cap > SIZE_MAX / sizeof *p) { mulib_regex_fail_(r, r->pos, "regex allocation overflow"); return 0; }
        p = (mulib_regex_node_ **)realloc(*items, cap * sizeof *p);
        if (!p) { mulib_regex_fail_(r, r->pos, "out of memory"); return 0; }
        *items = p; *capacity = cap;
    }
    (*items)[(*count)++] = child; return 1;
}
static mulib_regex_node_ *mulib_regex_sequence_(mulib_regex_reader_ *r) {
    mulib_regex_node_ **items = NULL, *n;
    size_t count = 0, capacity = 0;
    while (r->pos < r->length && r->source[r->pos] != ')' && r->source[r->pos] != '|') {
        n = mulib_regex_quantified_(r);
        if (!n || !mulib_regex_push_(r, &items, &count, &capacity, n)) { free(items); return NULL; }
    }
    if (count == 1) { n = items[0]; free(items); return n; }
    n = mulib_regex_node_new_(r, MULIB_REGEX_SEQUENCE_);
    if (!n) { free(items); return NULL; }
    n->children = items; n->count = count; return n;
}
static mulib_regex_node_ *mulib_regex_expression_(mulib_regex_reader_ *r) {
    mulib_regex_node_ **items = NULL, *n;
    size_t count = 0, capacity = 0;
    do {
        n = mulib_regex_sequence_(r);
        if (!n || !mulib_regex_push_(r, &items, &count, &capacity, n)) { free(items); return NULL; }
        if (r->pos == r->length || r->source[r->pos] != '|') break;
        ++r->pos;
    } while (!r->failed);
    if (count == 1) { n = items[0]; free(items); return n; }
    n = mulib_regex_node_new_(r, MULIB_REGEX_CHOICE_);
    if (!n) { free(items); return NULL; }
    n->children = items; n->count = count; return n;
}
static size_t mulib_regex_state_new_(mulib_regex_reader_ *r, mulib_regex_opcode_ op, size_t a, size_t b) {
    mulib_regex_t *re = r->regex;
    mulib_regex_state_ *s;
    if (re->count >= (size_t)MULIB_REGEX_MAX_STATES) { mulib_regex_fail_(r, r->pos, "compiled regex state limit"); return SIZE_MAX; }
    if (re->count == re->capacity) {
        size_t cap = re->capacity ? re->capacity : 16;
        if (re->capacity) {
            if (cap > SIZE_MAX / 2) { mulib_regex_fail_(r, r->pos, "regex allocation overflow"); return SIZE_MAX; }
            cap *= 2;
        }
        if (cap > (size_t)MULIB_REGEX_MAX_STATES) cap = (size_t)MULIB_REGEX_MAX_STATES;
        if (cap > SIZE_MAX / sizeof *s) { mulib_regex_fail_(r, r->pos, "regex allocation overflow"); return SIZE_MAX; }
        s = (mulib_regex_state_ *)realloc(re->states, cap * sizeof *s);
        if (!s) { mulib_regex_fail_(r, r->pos, "out of memory"); return SIZE_MAX; }
        re->states = s; re->capacity = cap;
    }
    s = &re->states[re->count]; memset(s, 0, sizeof *s); s->op = op; s->a = a; s->b = b;
    return re->count++;
}
static size_t mulib_regex_emit_(mulib_regex_reader_ *r, const mulib_regex_node_ *n, size_t next, size_t depth) {
    size_t i, out = next, a, split;
    if (r->failed) return SIZE_MAX;
    /* Count every AST visit, including empty sequences and {0} nodes that emit
     * no state. State/depth limits alone cannot bound nested repeat expansion.
     * Check before incrementing so even a SIZE_MAX budget cannot wrap. */
    if (r->emit_steps >= (size_t)MULIB_REGEX_MAX_EMIT_STEPS) {
        mulib_regex_fail_(r, n->offset, "regex compilation work limit"); return SIZE_MAX;
    }
    ++r->emit_steps;
    if (depth > (size_t)MULIB_REGEX_MAX_DEPTH * 4u) { mulib_regex_fail_(r, n->offset, "regex compilation nesting limit"); return SIZE_MAX; }
    switch (n->kind) {
    case MULIB_REGEX_SET_:
        out = mulib_regex_state_new_(r, MULIB_REGEX_OP_SET_, next, SIZE_MAX);
        if (out != SIZE_MAX) memcpy(r->regex->states[out].bits, n->bits, sizeof n->bits);
        break;
    case MULIB_REGEX_ANY_: out = mulib_regex_state_new_(r, MULIB_REGEX_OP_ANY_, next, SIZE_MAX); break;
    case MULIB_REGEX_BOL_: out = mulib_regex_state_new_(r, MULIB_REGEX_OP_BOL_, next, SIZE_MAX); break;
    case MULIB_REGEX_EOL_: out = mulib_regex_state_new_(r, MULIB_REGEX_OP_EOL_, next, SIZE_MAX); break;
    case MULIB_REGEX_SEQUENCE_:
        for (i = n->count; i && !r->failed; --i) out = mulib_regex_emit_(r, n->children[i - 1], out, depth + 1);
        break;
    case MULIB_REGEX_CHOICE_:
        out = mulib_regex_emit_(r, n->children[n->count - 1], next, depth + 1);
        for (i = n->count - 1; i && !r->failed; --i) {
            a = mulib_regex_emit_(r, n->children[i - 1], next, depth + 1);
            if (!r->failed) out = mulib_regex_state_new_(r, MULIB_REGEX_OP_SPLIT_, a, out);
        }
        break;
    case MULIB_REGEX_REPEAT_:
        if (n->max == SIZE_MAX) {
            split = mulib_regex_state_new_(r, MULIB_REGEX_OP_SPLIT_, SIZE_MAX, next);
            if (split == SIZE_MAX) return SIZE_MAX;
            a = mulib_regex_emit_(r, n->children[0], split, depth + 1);
            if (r->failed) return SIZE_MAX;
            r->regex->states[split].a = a; out = split;
        } else {
            for (i = n->min; i < n->max && !r->failed; ++i) {
                a = mulib_regex_emit_(r, n->children[0], out, depth + 1);
                if (!r->failed) out = mulib_regex_state_new_(r, MULIB_REGEX_OP_SPLIT_, a, out);
            }
        }
        for (i = 0; i < n->min && !r->failed; ++i) out = mulib_regex_emit_(r, n->children[0], out, depth + 1);
        break;
    }
    return r->failed ? SIZE_MAX : out;
}
void mulib_regex_destroy(mulib_regex_t *re) { if (re) { free(re->states); free(re); } }
mulib_regex_t *mulib_regex_compile(const char *pattern, size_t length, mulib_regex_error_t *error) {
    mulib_regex_reader_ r;
    mulib_regex_node_ *root;
    mulib_regex_t *re;
    if (error) memset(error, 0, sizeof *error);
    memset(&r, 0, sizeof r); r.source = pattern; r.length = length; r.error = error;
    if ((!pattern && length) || length == SIZE_MAX) { mulib_regex_fail_(&r, 0, "invalid pattern input"); return NULL; }
    re = (mulib_regex_t *)calloc(1, sizeof *re);
    if (!re) { mulib_regex_fail_(&r, 0, "out of memory"); return NULL; }
    r.regex = re; root = mulib_regex_expression_(&r);
    if (root && r.pos != length) mulib_regex_fail_(&r, r.pos, "unexpected closing parenthesis");
    if (!r.failed) {
        size_t end = mulib_regex_state_new_(&r, MULIB_REGEX_OP_MATCH_, SIZE_MAX, SIZE_MAX);
        if (end != SIZE_MAX) re->start = mulib_regex_emit_(&r, root, end, 0);
    }
    while (r.nodes) { mulib_regex_node_ *n = r.nodes; r.nodes = n->next; free(n->children); free(n); }
    if (r.failed) { mulib_regex_destroy(re); return NULL; }
    return re;
}
typedef struct mulib_regex_queue_ {
    size_t *items, head, tail, count, capacity;
    unsigned char *queued;
} mulib_regex_queue_;
static void mulib_regex_enqueue_(mulib_regex_queue_ *q, size_t state) {
    if (q->queued[state]) return;
    q->queued[state] = 1; q->items[q->tail] = state;
    if (++q->tail == q->capacity) q->tail = 0;
    ++q->count;
}
static void mulib_regex_reach_(size_t *origins, mulib_regex_queue_ *q, size_t state, size_t origin) {
    if (origin < origins[state]) { origins[state] = origin; mulib_regex_enqueue_(q, state); }
}
static void mulib_regex_closure_(const mulib_regex_t *re, size_t *origins, size_t pos, size_t length,
                                 mulib_regex_queue_ *q) {
    size_t i;
    q->head = q->tail = q->count = 0; memset(q->queued, 0, re->count);
    for (i = 0; i < re->count; ++i) if (origins[i] != SIZE_MAX) mulib_regex_enqueue_(q, i);
    while (q->count) {
        const mulib_regex_state_ *s;
        size_t at = q->items[q->head], origin = origins[at];
        if (++q->head == q->capacity) q->head = 0;
        --q->count; q->queued[at] = 0; s = &re->states[at];
        if (s->op == MULIB_REGEX_OP_SPLIT_) {
            mulib_regex_reach_(origins, q, s->a, origin); mulib_regex_reach_(origins, q, s->b, origin);
        } else if ((s->op == MULIB_REGEX_OP_BOL_ && pos == 0) || (s->op == MULIB_REGEX_OP_EOL_ && pos == length))
            mulib_regex_reach_(origins, q, s->a, origin);
    }
}
static int mulib_regex_run_(const mulib_regex_t *re, const char *source, size_t length,
                            size_t start, int full, mulib_regex_match_t *match) {
    size_t *current, *next, *storage, i, pos, best = SIZE_MAX, best_end = 0;
    mulib_regex_queue_ q;
    if (match) memset(match, 0, sizeof *match);
    if (!re || (!source && length) || length == SIZE_MAX || start > length ||
        re->count > SIZE_MAX / (2 * sizeof *storage)) return -1;
    storage = (size_t *)malloc(re->count * 2 * sizeof *storage);
    q.items = (size_t *)malloc(re->count * sizeof *q.items);
    q.queued = (unsigned char *)malloc(re->count); q.capacity = re->count;
    if (!storage || !q.items || !q.queued) { free(storage); free(q.items); free(q.queued); return -1; }
    current = storage; next = storage + re->count;
    for (i = 0; i < re->count; ++i) current[i] = SIZE_MAX;
    for (pos = start;; ++pos) {
        if ((!full || pos == 0) && (best == SIZE_MAX || pos <= best) && pos < current[re->start]) current[re->start] = pos;
        mulib_regex_closure_(re, current, pos, length, &q);
        /* The unique accept instruction is state zero. */
        if (current[0] != SIZE_MAX && (!full || pos == length) &&
            (current[0] < best || (current[0] == best && pos > best_end))) {
            best = current[0]; best_end = pos;
        }
        if (pos == length) break;
        for (i = 0; i < re->count; ++i) next[i] = SIZE_MAX;
        for (i = 0; i < re->count; ++i) if (current[i] != SIZE_MAX) {
            const mulib_regex_state_ *s = &re->states[i];
            unsigned char c = (unsigned char)source[pos];
            int yes = s->op == MULIB_REGEX_OP_ANY_ ? c != '\n' :
                s->op == MULIB_REGEX_OP_SET_ && (s->bits[c >> 3] & (1u << (c & 7)));
            if (yes && current[i] < next[s->a]) next[s->a] = current[i];
        }
        { size_t *swap = current; current = next; next = swap; }
    }
    free(storage); free(q.items); free(q.queued);
    if (best == SIZE_MAX) return 0;
    if (match) { match->offset = best; match->length = best_end - best; }
    return 1;
}
int mulib_regex_search(const mulib_regex_t *re, const char *source, size_t length,
                       size_t start, mulib_regex_match_t *match) {
    return mulib_regex_run_(re, source, length, start, 0, match);
}
int mulib_regex_full_match(const mulib_regex_t *re, const char *source, size_t length) {
    return mulib_regex_run_(re, source, length, 0, 1, NULL);
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
