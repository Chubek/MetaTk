/**
 * @file mulib-vterm.h
 * @brief Bounded, grid-backed UTF-8/VT screen model with shared terminal cells.
 *
 * Implements useful ECMA-48/DEC cursor, scroll, erase, editing and SGR subsets.
 * Every Unicode scalar occupies one cell: no wcwidth, combining or grapheme
 * processing. No PTY, alternate screen, scrollback, OSC actions or terminal
 * replies. Escape strings are discarded with bounded parser storage. Define
 * MULIB_IMPL_VTERM, MULIB_IMPL_GRID and MULIB_IMPL_UTF8 in one translation unit.
 * See manual/mulib-vterm.md for the precise protocol and ownership contract.
 */
#ifndef MULIB_VTERM_H
#define MULIB_VTERM_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-grid.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Default color sentinel; indexed colors are the integers 0..255. */
#define MULIB_TERM_COLOR_DEFAULT UINT32_C(0xffffffff)
/** @brief Tagged 24-bit RGB color; each argument is masked to eight bits. */
#define MULIB_TERM_RGB(r,g,b) (UINT32_C(0x01000000) | (((uint32_t)(r) & 255u) << 16) | (((uint32_t)(g) & 255u) << 8) | ((uint32_t)(b) & 255u))
/** @brief Maximum cells per screen; bounds memory before calling grid allocation. */
#define MULIB_VTERM_MAX_CELLS 1048576u
/** @brief Shared rendition bits for emulator and TUI. */
enum mulib_term_attribute {
    MULIB_TERM_BOLD = 1u, MULIB_TERM_DIM = 2u, MULIB_TERM_ITALIC = 4u,
    MULIB_TERM_UNDERLINE = 8u, MULIB_TERM_BLINK = 16u, MULIB_TERM_INVERSE = 32u,
    MULIB_TERM_CONCEAL = 64u, MULIB_TERM_STRIKE = 128u
};
/** @brief Value style: default/indexed/RGB foreground and background plus bits. */
typedef struct mulib_term_style {
    uint32_t foreground, background;
    unsigned attributes;
} mulib_term_style_t;
/** @brief One scalar in one cell; blank cells contain U+0020, never NUL. */
typedef struct mulib_term_cell {
    uint32_t codepoint;
    mulib_term_style_t style;
} mulib_term_cell_t;
/** @brief Owned screen/parser. Members are inspectable, not for direct mutation. */
typedef struct mulib_vterm {
    mulib_grid_t grid;
    size_t cursor_x, cursor_y, saved_x, saved_y, scroll_top, scroll_bottom;
    mulib_term_style_t style, saved_style;
    unsigned auto_wrap, origin_mode, cursor_visible, wrap_pending;
    unsigned state, string_escape, utf8_length, utf8_need;
    unsigned char utf8[4], escape[256];
    size_t escape_length, string_length;
    size_t invalid_input, ignored_sequences;
} mulib_vterm_t;

/** @brief Return the default style (default colors, no attributes). */
mulib_term_style_t mulib_term_style_default(void);
/** @brief Validate scalar, safe printable cell, color tags and attribute bits. */
int mulib_term_cell_valid(mulib_term_cell_t cell);
/** @brief Compare cell values without relying on struct padding. */
int mulib_term_cell_equal(mulib_term_cell_t a, mulib_term_cell_t b);
/** @brief Initialize an uninitialized screen. Failure leaves a zero/freeable object. */
int mulib_vterm_init(mulib_vterm_t *screen, size_t width, size_t height);
/** @brief Release storage and zero the screen. NULL is allowed. */
void mulib_vterm_free(mulib_vterm_t *screen);
/** @brief Atomically resize, preserving the top-left overlap; reset scroll margins. */
int mulib_vterm_resize(mulib_vterm_t *screen, size_t width, size_t height);
/** @brief Reset cells, modes, cursor, parser and counters; retain allocation. */
void mulib_vterm_reset(mulib_vterm_t *screen);
/** @brief Borrow a cell until resize/free, or return NULL outside the screen. */
const mulib_term_cell_t *mulib_vterm_cell(const mulib_vterm_t *screen, size_t x, size_t y);
/** @brief Write a validated cell at an in-bounds coordinate; return 1/0. */
int mulib_vterm_set_cell(mulib_vterm_t *screen, size_t x, size_t y, mulib_term_cell_t cell);
/** @brief Fill a clipped rectangle. Invalid cells/screens fail; empty clips succeed. */
int mulib_vterm_fill(mulib_vterm_t *screen, size_t x, size_t y,
                     size_t width, size_t height, mulib_term_cell_t cell);
/** @brief Move to an in-bounds absolute coordinate, clearing delayed wrap. */
int mulib_vterm_move(mulib_vterm_t *screen, size_t x, size_t y);
/** @brief Scroll the current region: positive up, negative down; clear with current style. */
void mulib_vterm_scroll(mulib_vterm_t *screen, int lines);
/** @brief Consume all bytes incrementally. 1 success, 0 invalid arguments/state. */
int mulib_vterm_feed(mulib_vterm_t *screen, const void *bytes, size_t length);
/** @brief Flush truncated UTF-8 as U+FFFD and discard incomplete escape sequences. */
void mulib_vterm_finish(mulib_vterm_t *screen);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_VTERM_H */

#if defined(MULIB_IMPL_VTERM) && !defined(MULIB_VTERM_IMPLEMENTED)
#define MULIB_VTERM_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

mulib_term_style_t mulib_term_style_default(void) {
    mulib_term_style_t s;
    s.foreground = s.background = MULIB_TERM_COLOR_DEFAULT;
    s.attributes = 0;
    return s;
}
static int mulib_vterm_color_(uint32_t c) {
    return c <= 255u || c == MULIB_TERM_COLOR_DEFAULT || (c >> 24) == 1;
}
int mulib_term_cell_valid(mulib_term_cell_t c) {
    return c.codepoint >= 32 && !(c.codepoint >= 127 && c.codepoint < 160) && c.codepoint <= 0x10ffffu &&
        !(c.codepoint >= 0xd800u && c.codepoint <= 0xdfffu) &&
        mulib_vterm_color_(c.style.foreground) && mulib_vterm_color_(c.style.background) &&
        !(c.style.attributes & ~255u);
}
int mulib_term_cell_equal(mulib_term_cell_t a, mulib_term_cell_t b) {
    return a.codepoint == b.codepoint && a.style.foreground == b.style.foreground &&
        a.style.background == b.style.background && a.style.attributes == b.style.attributes;
}
static mulib_term_cell_t mulib_vterm_blank_(const mulib_vterm_t *v) {
    mulib_term_cell_t c;
    c.codepoint = ' '; c.style = v->style;
    return c;
}
static mulib_term_cell_t *mulib_vterm_at_(mulib_vterm_t *v, size_t x, size_t y) {
    return (mulib_term_cell_t *)mulib_grid_at(&v->grid, x, y);
}
static int mulib_vterm_dimensions_(size_t w, size_t h) {
    return w && h && w <= INT_MAX && h <= INT_MAX &&
        w <= MULIB_VTERM_MAX_CELLS && h <= MULIB_VTERM_MAX_CELLS / w;
}
int mulib_vterm_init(mulib_vterm_t *v, size_t w, size_t h) {
    if (!v) return 0;
    memset(v, 0, sizeof(*v));
    if (!mulib_vterm_dimensions_(w, h) || !mulib_grid_init(&v->grid, w, h, sizeof(mulib_term_cell_t)))
        return 0;
    mulib_vterm_reset(v);
    return 1;
}
void mulib_vterm_free(mulib_vterm_t *v) {
    if (v) { mulib_grid_free(&v->grid); memset(v, 0, sizeof(*v)); }
}
void mulib_vterm_reset(mulib_vterm_t *v) {
    mulib_grid_t grid;
    if (!v || !v->grid.data || !v->grid.width || !v->grid.height) return;
    grid = v->grid;
    memset(v, 0, sizeof(*v));
    v->grid = grid;
    v->style = v->saved_style = mulib_term_style_default();
    v->auto_wrap = v->cursor_visible = 1;
    v->scroll_bottom = grid.height - 1;
    (void)mulib_vterm_fill(v, 0, 0, grid.width, grid.height, mulib_vterm_blank_(v));
}
const mulib_term_cell_t *mulib_vterm_cell(const mulib_vterm_t *v, size_t x, size_t y) {
    if (!v || !v->grid.data || x >= v->grid.width || y >= v->grid.height) return NULL;
    return (const mulib_term_cell_t *)(const void *)(v->grid.data +
        (y * v->grid.width + x) * sizeof(mulib_term_cell_t));
}
int mulib_vterm_set_cell(mulib_vterm_t *v, size_t x, size_t y, mulib_term_cell_t c) {
    mulib_term_cell_t *at;
    if (!v || !v->grid.data || !mulib_term_cell_valid(c)) return 0;
    at = mulib_vterm_at_(v, x, y);
    if (!at) return 0;
    *at = c;
    return 1;
}
int mulib_vterm_fill(mulib_vterm_t *v, size_t x, size_t y, size_t w, size_t h, mulib_term_cell_t c) {
    size_t row, col;
    if (!v || !v->grid.data || !mulib_term_cell_valid(c)) return 0;
    if (x >= v->grid.width || y >= v->grid.height) return 1;
    if (w > v->grid.width - x) w = v->grid.width - x;
    if (h > v->grid.height - y) h = v->grid.height - y;
    for (row = 0; row < h; ++row) {
        mulib_term_cell_t *p = mulib_vterm_at_(v, x, y + row);
        for (col = 0; col < w; ++col) p[col] = c;
    }
    return 1;
}
int mulib_vterm_resize(mulib_vterm_t *v, size_t w, size_t h) {
    mulib_vterm_t next;
    size_t row, width, height;
    if (!v || !v->grid.data || !mulib_vterm_init(&next, w, h)) return 0;
    width = w < v->grid.width ? w : v->grid.width;
    height = h < v->grid.height ? h : v->grid.height;
    for (row = 0; row < height; ++row)
        memcpy(mulib_vterm_at_(&next, 0, row), mulib_vterm_at_(v, 0, row), width * sizeof(mulib_term_cell_t));
    mulib_grid_free(&v->grid);
    v->grid = next.grid;
    v->cursor_x = v->cursor_x < w ? v->cursor_x : w - 1;
    v->cursor_y = v->cursor_y < h ? v->cursor_y : h - 1;
    v->saved_x = v->saved_x < w ? v->saved_x : w - 1;
    v->saved_y = v->saved_y < h ? v->saved_y : h - 1;
    v->scroll_top = 0; v->scroll_bottom = h - 1; v->wrap_pending = 0;
    return 1;
}
int mulib_vterm_move(mulib_vterm_t *v, size_t x, size_t y) {
    if (!v || !v->grid.data || x >= v->grid.width || y >= v->grid.height) return 0;
    v->cursor_x = x; v->cursor_y = y; v->wrap_pending = 0;
    return 1;
}
static void mulib_vterm_region_scroll_(mulib_vterm_t *v, size_t top, size_t bottom,
                                       size_t count, int down) {
    size_t rows = bottom - top + 1, stride = v->grid.width * sizeof(mulib_term_cell_t);
    if (count > rows) count = rows;
    if (!count) return;
    if (down) {
        if (count != rows)
            memmove(mulib_vterm_at_(v, 0, top + count), mulib_vterm_at_(v, 0, top), (rows - count) * stride);
        (void)mulib_vterm_fill(v, 0, top, v->grid.width, count, mulib_vterm_blank_(v));
    } else {
        if (count != rows)
            memmove(mulib_vterm_at_(v, 0, top), mulib_vterm_at_(v, 0, top + count), (rows - count) * stride);
        (void)mulib_vterm_fill(v, 0, bottom - count + 1, v->grid.width, count, mulib_vterm_blank_(v));
    }
}
void mulib_vterm_scroll(mulib_vterm_t *v, int lines) {
    size_t n;
    if (!v || !v->grid.data) return;
    n = (size_t)(lines < 0 ? -(int64_t)lines : lines);
    mulib_vterm_region_scroll_(v, v->scroll_top, v->scroll_bottom, n, lines < 0);
}
static void mulib_vterm_index_(mulib_vterm_t *v, int reverse) {
    v->wrap_pending = 0;
    if (reverse) {
        if (v->cursor_y == v->scroll_top) mulib_vterm_scroll(v, -1);
        else if (v->cursor_y) --v->cursor_y;
    } else {
        if (v->cursor_y == v->scroll_bottom) mulib_vterm_scroll(v, 1);
        else if (v->cursor_y + 1 < v->grid.height) ++v->cursor_y;
    }
}
static void mulib_vterm_print_(mulib_vterm_t *v, uint32_t cp) {
    mulib_term_cell_t c;
    if (cp < 32 || (cp >= 127 && cp < 160)) cp = 0xfffdu;
    if (v->wrap_pending && v->auto_wrap) {
        v->cursor_x = 0;
        mulib_vterm_index_(v, 0);
    }
    v->wrap_pending = 0;
    c.codepoint = cp; c.style = v->style;
    *mulib_vterm_at_(v, v->cursor_x, v->cursor_y) = c;
    if (v->cursor_x + 1 < v->grid.width) ++v->cursor_x;
    else v->wrap_pending = v->auto_wrap;
}
static void mulib_vterm_save_(mulib_vterm_t *v) {
    v->saved_x = v->cursor_x; v->saved_y = v->cursor_y; v->saved_style = v->style;
}
static void mulib_vterm_restore_(mulib_vterm_t *v) {
    v->cursor_x = v->saved_x; v->cursor_y = v->saved_y; v->style = v->saved_style;
    v->wrap_pending = 0;
}
static size_t mulib_vterm_param_(const int *p, size_t n, size_t i, size_t def) {
    return i >= n || p[i] <= 0 ? def : (size_t)p[i];
}
static size_t mulib_vterm_vertical_(const mulib_vterm_t *v, size_t y, size_t count, int down) {
    size_t lo = 0, hi = v->grid.height - 1;
    if (v->origin_mode) {
        lo = v->scroll_top; hi = v->scroll_bottom;
        /* The absolute C API can place the cursor outside the origin region. */
        if (y < lo) y = lo;
        if (y > hi) y = hi;
    } else if (down) {
        if (y <= v->scroll_bottom) hi = v->scroll_bottom;
    } else if (y >= v->scroll_top) lo = v->scroll_top;
    return down ? (count > hi - y ? hi : y + count) : (count > y - lo ? lo : y - count);
}
static void mulib_vterm_sgr_(mulib_vterm_t *v, const int *p, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        int a = p[i] < 0 ? 0 : p[i];
        if (!a) v->style = mulib_term_style_default();
        else if (a == 1) v->style.attributes |= MULIB_TERM_BOLD;
        else if (a == 2) v->style.attributes |= MULIB_TERM_DIM;
        else if (a == 3) v->style.attributes |= MULIB_TERM_ITALIC;
        else if (a == 4) v->style.attributes |= MULIB_TERM_UNDERLINE;
        else if (a == 5) v->style.attributes |= MULIB_TERM_BLINK;
        else if (a == 7) v->style.attributes |= MULIB_TERM_INVERSE;
        else if (a == 8) v->style.attributes |= MULIB_TERM_CONCEAL;
        else if (a == 9) v->style.attributes |= MULIB_TERM_STRIKE;
        else if (a == 22) v->style.attributes &= ~(MULIB_TERM_BOLD | MULIB_TERM_DIM);
        else if (a == 23) v->style.attributes &= ~MULIB_TERM_ITALIC;
        else if (a == 24) v->style.attributes &= ~MULIB_TERM_UNDERLINE;
        else if (a == 25) v->style.attributes &= ~MULIB_TERM_BLINK;
        else if (a == 27) v->style.attributes &= ~MULIB_TERM_INVERSE;
        else if (a == 28) v->style.attributes &= ~MULIB_TERM_CONCEAL;
        else if (a == 29) v->style.attributes &= ~MULIB_TERM_STRIKE;
        else if (a >= 30 && a <= 37) v->style.foreground = (uint32_t)(a - 30);
        else if (a >= 40 && a <= 47) v->style.background = (uint32_t)(a - 40);
        else if (a >= 90 && a <= 97) v->style.foreground = (uint32_t)(a - 90 + 8);
        else if (a >= 100 && a <= 107) v->style.background = (uint32_t)(a - 100 + 8);
        else if (a == 39) v->style.foreground = MULIB_TERM_COLOR_DEFAULT;
        else if (a == 49) v->style.background = MULIB_TERM_COLOR_DEFAULT;
        else if (a == 38 || a == 48) {
            uint32_t color;
            if (i + 2 < n && p[i + 1] == 5 && p[i + 2] >= 0 && p[i + 2] <= 255) {
                color = (uint32_t)p[i + 2]; i += 2;
            } else if (i + 4 < n && p[i + 1] == 2 &&
                p[i + 2] >= 0 && p[i + 2] <= 255 && p[i + 3] >= 0 && p[i + 3] <= 255 &&
                p[i + 4] >= 0 && p[i + 4] <= 255) {
                color = MULIB_TERM_RGB(p[i + 2], p[i + 3], p[i + 4]); i += 4;
            } else { ++v->ignored_sequences; break; }
            if (a == 38) v->style.foreground = color; else v->style.background = color;
        } else ++v->ignored_sequences;
    }
}
static void mulib_vterm_erase_(mulib_vterm_t *v, unsigned final, int mode) {
    mulib_term_cell_t c = mulib_vterm_blank_(v);
    size_t w = v->grid.width, h = v->grid.height, x = v->cursor_x, y = v->cursor_y;
    if (final == 'K') {
        if (mode == 0) (void)mulib_vterm_fill(v, x, y, w - x, 1, c);
        else if (mode == 1) (void)mulib_vterm_fill(v, 0, y, x + 1, 1, c);
        else if (mode == 2) (void)mulib_vterm_fill(v, 0, y, w, 1, c);
        else ++v->ignored_sequences;
    } else if (mode == 0) {
        (void)mulib_vterm_fill(v, x, y, w - x, 1, c);
        (void)mulib_vterm_fill(v, 0, y + 1, w, h - y - 1, c);
    } else if (mode == 1) {
        (void)mulib_vterm_fill(v, 0, 0, w, y, c);
        (void)mulib_vterm_fill(v, 0, y, x + 1, 1, c);
    } else if (mode == 2) (void)mulib_vterm_fill(v, 0, 0, w, h, c);
    else ++v->ignored_sequences;
}
static void mulib_vterm_csi_(mulib_vterm_t *v, unsigned final) {
    int params[32], value = -1;
    size_t i = 0, n = 0, a, b, lo, hi, x = v->cursor_x, y = v->cursor_y;
    unsigned private_mode = 0;
    if (v->escape_length && v->escape[0] == '?') { private_mode = 1; ++i; }
    for (; i < v->escape_length; ++i) {
        unsigned c = v->escape[i];
        if (c >= '0' && c <= '9') {
            if (value < 0) value = 0;
            if (value > (1000000 - (int)(c - '0')) / 10) goto invalid;
            value = value * 10 + (int)(c - '0');
        } else if (c == ';') {
            if (n == 31) goto invalid;
            params[n++] = value; value = -1;
        } else { ++v->ignored_sequences; return; }
    }
    params[n++] = value;
    a = mulib_vterm_param_(params, n, 0, 1);
    lo = v->origin_mode ? v->scroll_top : 0;
    hi = v->origin_mode ? v->scroll_bottom : v->grid.height - 1;
    if (private_mode) {
        if (final != 'h' && final != 'l') { ++v->ignored_sequences; return; }
        for (i = 0; i < n; ++i) {
            if (params[i] == 6) {
                v->origin_mode = final == 'h';
                v->cursor_x = 0; v->cursor_y = v->origin_mode ? v->scroll_top : 0;
            } else if (params[i] == 7) v->auto_wrap = final == 'h';
            else if (params[i] == 25) v->cursor_visible = final == 'h';
            else ++v->ignored_sequences;
        }
        v->wrap_pending = 0;
        return;
    }
    switch (final) {
        case 'A': y = mulib_vterm_vertical_(v, y, a, 0); break;
        case 'B': case 'e': y = mulib_vterm_vertical_(v, y, a, 1); break;
        case 'C': case 'a': x = a > v->grid.width - 1 - x ? v->grid.width - 1 : x + a; break;
        case 'D': x = a > x ? 0 : x - a; break;
        case 'E': y = mulib_vterm_vertical_(v, y, a, 1); x = 0; break;
        case 'F': y = mulib_vterm_vertical_(v, y, a, 0); x = 0; break;
        case 'G': case '`': x = a > v->grid.width ? v->grid.width - 1 : a - 1; break;
        case 'd': y = a - 1 > hi - lo ? hi : lo + a - 1; break;
        case 'H': case 'f':
            b = mulib_vterm_param_(params, n, 1, 1);
            x = b > v->grid.width ? v->grid.width - 1 : b - 1;
            y = a - 1 > hi - lo ? hi : lo + a - 1;
            break;
        case 'J': case 'K': mulib_vterm_erase_(v, final, params[0] < 0 ? 0 : params[0]); break;
        case 'm': mulib_vterm_sgr_(v, params, n); return;
        case 'S': case 'T': mulib_vterm_region_scroll_(v, v->scroll_top, v->scroll_bottom, a, final == 'T'); break;
        case 'L': case 'M':
            if (y >= v->scroll_top && y <= v->scroll_bottom)
                mulib_vterm_region_scroll_(v, y, v->scroll_bottom, a, final == 'L');
            break;
        case '@': case 'P': case 'X': {
            mulib_term_cell_t *row = mulib_vterm_at_(v, 0, y);
            size_t left = v->grid.width - x;
            if (a > left) a = left;
            if (final == '@') {
                memmove(row + x + a, row + x, (left - a) * sizeof(*row));
                (void)mulib_vterm_fill(v, x, y, a, 1, mulib_vterm_blank_(v));
            } else if (final == 'P') {
                memmove(row + x, row + x + a, (left - a) * sizeof(*row));
                (void)mulib_vterm_fill(v, v->grid.width - a, y, a, 1, mulib_vterm_blank_(v));
            } else (void)mulib_vterm_fill(v, x, y, a, 1, mulib_vterm_blank_(v));
            break;
        }
        case 'r':
            b = mulib_vterm_param_(params, n, 1, v->grid.height);
            if (a < b && b <= v->grid.height) {
                v->scroll_top = a - 1; v->scroll_bottom = b - 1;
                v->cursor_x = 0; v->cursor_y = v->origin_mode ? a - 1 : 0; v->wrap_pending = 0;
            } else ++v->ignored_sequences;
            return;
        case 's': mulib_vterm_save_(v); return;
        case 'u': mulib_vterm_restore_(v); return;
        default: ++v->ignored_sequences; return;
    }
    v->cursor_x = x; v->cursor_y = y; v->wrap_pending = 0;
    return;
invalid:
    ++v->invalid_input;
}

static void mulib_vterm_byte_(mulib_vterm_t *v, unsigned char c) {
    uint32_t cp;
    size_t used;
    /* CAN/SUB cancel any escape; ESC restarts except inside string payloads. */
    if (v->state && (c == 0x18 || c == 0x1a)) {
        v->state = 0; v->escape_length = 0; v->string_escape = 0; return;
    }
    if (v->state == 3 || v->state == 4) {
        if ((v->state == 3 && c == 7) || (v->string_escape && c == '\\')) {
            v->state = 0; v->string_escape = 0; return;
        }
        v->string_escape = c == 27;
        if (v->string_length < 4097) {
            ++v->string_length;
            if (v->string_length == 4097) ++v->invalid_input;
        }
        return;
    }
    if (v->state == 5) { /* One-byte character-set/intermediate designation. */
        if (c == 27) v->state = 1; else v->state = 0;
        return;
    }
    if (v->state == 2 || v->state == 6) {
        if (c == 27) { v->state = 1; v->escape_length = 0; return; }
        if (c >= 0x40 && c <= 0x7e) {
            if (v->state == 2) mulib_vterm_csi_(v, c);
            v->state = 0; v->escape_length = 0;
        } else if (c >= 0x20 && c <= 0x3f) {
            if (v->state == 2) {
                if (v->escape_length == sizeof(v->escape)) { v->state = 6; ++v->invalid_input; }
                else v->escape[v->escape_length++] = c;
            }
        } else if (c >= 0x80) {
            v->state = 6; ++v->invalid_input;
        } else if (c == '\r') { v->cursor_x = 0; v->wrap_pending = 0; }
        else if (c == '\n' || c == '\v' || c == '\f') mulib_vterm_index_(v, 0);
        else if (c == '\b') { if (v->cursor_x) --v->cursor_x; v->wrap_pending = 0; }
        return;
    }
    if (v->state == 1) {
        v->state = 0;
        switch (c) {
            case '[': v->state = 2; v->escape_length = 0; break;
            case ']': v->state = 3; v->string_length = 0; v->string_escape = 0; ++v->ignored_sequences; break;
            case 'P': case '_': case '^': case 'X':
                v->state = 4; v->string_length = 0; v->string_escape = 0; ++v->ignored_sequences; break;
            case '(': case ')': case '*': case '+': case '%': case '#':
                v->state = 5; ++v->ignored_sequences; break;
            case '7': mulib_vterm_save_(v); break;
            case '8': mulib_vterm_restore_(v); break;
            case 'D': mulib_vterm_index_(v, 0); break;
            case 'E': v->cursor_x = 0; mulib_vterm_index_(v, 0); break;
            case 'M': mulib_vterm_index_(v, 1); break;
            case 'c': mulib_vterm_reset(v); break;
            case 27: v->state = 1; break;
            case '=': case '>': break;
            default: ++v->ignored_sequences; break;
        }
        return;
    }
    if (v->utf8_length) {
        if ((c & 0xc0u) == 0x80u) {
            v->utf8[v->utf8_length++] = c;
            if (v->utf8_length == v->utf8_need) {
                if (!mulib_utf8_decode(v->utf8, v->utf8_length, &cp, &used)) {
                    cp = 0xfffdu; ++v->invalid_input;
                }
                v->utf8_length = 0;
                mulib_vterm_print_(v, cp);
            }
            return;
        }
        v->utf8_length = 0; ++v->invalid_input;
        mulib_vterm_print_(v, 0xfffdu);
        /* Reprocess the non-continuation byte, including control/escape bytes. */
    }
    if (c == 27) { v->state = 1; return; }
    if (c < 32 || c == 127) {
        if (c == '\r') { v->cursor_x = 0; v->wrap_pending = 0; }
        else if (c == '\n' || c == '\v' || c == '\f') mulib_vterm_index_(v, 0);
        else if (c == '\b') { if (v->cursor_x) --v->cursor_x; v->wrap_pending = 0; }
        else if (c == '\t') {
            size_t next = (v->cursor_x / 8u + 1u) * 8u;
            v->cursor_x = next < v->grid.width ? next : v->grid.width - 1;
            v->wrap_pending = 0;
        }
        return;
    }
    if (c < 0x80) { mulib_vterm_print_(v, c); return; }
    if (c >= 0xc2 && c <= 0xf4) {
        v->utf8[0] = c; v->utf8_length = 1;
        v->utf8_need = c < 0xe0 ? 2u : c < 0xf0 ? 3u : 4u;
    } else { ++v->invalid_input; mulib_vterm_print_(v, 0xfffdu); }
}
int mulib_vterm_feed(mulib_vterm_t *v, const void *bytes, size_t n) {
    const unsigned char *p = (const unsigned char *)bytes;
    size_t i;
    if (!v || !v->grid.data || (!p && n)) return 0;
    for (i = 0; i < n; ++i) mulib_vterm_byte_(v, p[i]);
    return 1;
}
void mulib_vterm_finish(mulib_vterm_t *v) {
    if (!v || !v->grid.data) return;
    if (v->utf8_length) { mulib_vterm_print_(v, 0xfffdu); ++v->invalid_input; }
    if (v->state) ++v->invalid_input;
    v->utf8_length = 0; v->state = 0; v->escape_length = 0; v->string_escape = 0;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_VTERM_IMPLEMENTED */
