/**
 * @file mulib-tui.h
 * @brief Clipped immediate-mode widgets and transactional double-buffer rendering.
 *
 * Reuses vterm cells/grid, termkey events, UTF-8 and strbuf. Rendering uses an
 * explicit terminfo record or the ANSI fallback (NULL). No terminal mode, I/O,
 * event loop or allocation is hidden in drawing. All scalars occupy one cell.
 * Define the TUI, VTERM, GRID, TERMINFO, STRBUF and UTF8 implementation switches
 * once; add TERMKEY to decode input. See manual/mulib-tui.md for limitations.
 */
#ifndef MULIB_TUI_H
#define MULIB_TUI_H
#include <stddef.h>
#include "mulib-vterm.h"
#include "mulib-termkey.h"
#include "mulib-terminfo.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Unsigned rectangle; drawing clips without overflowing x+width/y+height. */
typedef struct mulib_tui_rect { size_t x, y, width, height; } mulib_tui_rect_t;
/** @brief Owned back/front screens. Use APIs to mutate; inspect back cells freely. */
typedef struct mulib_tui {
    mulib_vterm_t back;
    mulib_grid_t front;
    size_t cursor_x, cursor_y, front_cursor_x, front_cursor_y;
    unsigned cursor_visible, front_cursor_visible, invalid;
} mulib_tui_t;

/** @brief Initialize two blank buffers with a hidden cursor; failure is freeable. */
int mulib_tui_init(mulib_tui_t *ui, size_t width, size_t height);
/** @brief Release both buffers and zero state; NULL is allowed. */
void mulib_tui_free(mulib_tui_t *ui);
/** @brief Atomically resize, preserve back-buffer overlap, invalidate front. */
int mulib_tui_resize(mulib_tui_t *ui, size_t width, size_t height);
/** @brief Force a complete repaint after external terminal output or write failure. */
void mulib_tui_invalidate(mulib_tui_t *ui);
/** @brief Borrow a back-buffer cell; NULL outside bounds. */
const mulib_term_cell_t *mulib_tui_cell(const mulib_tui_t *ui, size_t x, size_t y);
/** @brief Put one validated in-bounds cell. */
int mulib_tui_put(mulib_tui_t *ui, size_t x, size_t y, mulib_term_cell_t cell);
/** @brief Fill a clipped rectangle with a validated cell, returning 1/0. */
int mulib_tui_fill(mulib_tui_t *ui, mulib_tui_rect_t rect, mulib_term_cell_t cell);
/** @brief Fill the back buffer with spaces in style; return 1/0. */
int mulib_tui_clear(mulib_tui_t *ui, mulib_term_style_t style);
/**
 * @brief Draw valid UTF-8 in a clipped rectangle, wrapping one scalar per cell.
 * LF starts a new row, CR is ignored, TAB fills to a four-column stop. Other
 * control scalars become U+FFFD. Invalid UTF-8/style leaves cells unchanged.
 */
int mulib_tui_text(mulib_tui_t *ui, mulib_tui_rect_t rect, const char *text,
                   size_t length, mulib_term_style_t style);
/** @brief Draw an ASCII +-| border on the clipped rectangle. */
int mulib_tui_box(mulib_tui_t *ui, mulib_tui_rect_t rect, mulib_term_style_t style);
/** @brief Draw a boxed, horizontally centered single-line label; focused adds inverse. */
int mulib_tui_button(mulib_tui_t *ui, mulib_tui_rect_t rect, const char *label,
                     size_t length, int focused, mulib_term_style_t style);
/** @brief Activate on focused Enter/Space or unmodified left mouse press in rect. */
int mulib_tui_button_event(mulib_tui_rect_t rect, const mulib_termkey_event_t *event, int focused);
/** @brief Split columns (vertical=0) or rows (vertical=1), with an explicit gap. */
int mulib_tui_split(mulib_tui_rect_t rect, int vertical, size_t first, size_t gap,
                    mulib_tui_rect_t *a, mulib_tui_rect_t *b);
/** @brief Set desired cursor position/visibility; out-of-bounds coordinates fail. */
int mulib_tui_cursor(mulib_tui_t *ui, size_t x, size_t y, int visible);
/**
 * @brief Append changed-cell terminal output and commit front buffer atomically.
 * NULL info selects explicit ANSI fallback. Missing capabilities, unsupported
 * expansion/style, allocation failure or output over 64 MiB returns 0 with
 * output/front unchanged. On success the caller must write ALL appended bytes;
 * call invalidate if transport fails. No changes produces no output.
 * Painting requires both sgr0 and op: attribute reset does not imply color reset.
 */
int mulib_tui_render(mulib_tui_t *ui, const mulib_terminfo_t *info, mulib_strbuf_t *output);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_TUI_H */

#if defined(MULIB_IMPL_TUI) && !defined(MULIB_TUI_IMPLEMENTED)
#define MULIB_TUI_IMPLEMENTED
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_tui_init(mulib_tui_t *ui, size_t w, size_t h) {
    if (!ui) return 0;
    memset(ui, 0, sizeof(*ui));
    if (!mulib_vterm_init(&ui->back, w, h)) return 0;
    if (!mulib_grid_init(&ui->front, w, h, sizeof(mulib_term_cell_t))) {
        mulib_vterm_free(&ui->back); return 0;
    }
    ui->invalid = 1;
    return 1;
}
void mulib_tui_free(mulib_tui_t *ui) {
    if (ui) { mulib_vterm_free(&ui->back); mulib_grid_free(&ui->front); memset(ui, 0, sizeof(*ui)); }
}
int mulib_tui_resize(mulib_tui_t *ui, size_t w, size_t h) {
    mulib_tui_t next;
    size_t y, width, height;
    if (!ui || !ui->back.grid.data || !mulib_tui_init(&next, w, h)) return 0;
    width = w < ui->back.grid.width ? w : ui->back.grid.width;
    height = h < ui->back.grid.height ? h : ui->back.grid.height;
    for (y = 0; y < height; ++y)
        memcpy(mulib_grid_at(&next.back.grid, 0, y), mulib_vterm_cell(&ui->back, 0, y),
               width * sizeof(mulib_term_cell_t));
    next.cursor_x = ui->cursor_x < w ? ui->cursor_x : w - 1;
    next.cursor_y = ui->cursor_y < h ? ui->cursor_y : h - 1;
    next.cursor_visible = ui->cursor_visible;
    mulib_tui_free(ui);
    *ui = next;
    return 1;
}
void mulib_tui_invalidate(mulib_tui_t *ui) { if (ui) ui->invalid = 1; }
const mulib_term_cell_t *mulib_tui_cell(const mulib_tui_t *ui, size_t x, size_t y) {
    return ui ? mulib_vterm_cell(&ui->back, x, y) : NULL;
}
int mulib_tui_put(mulib_tui_t *ui, size_t x, size_t y, mulib_term_cell_t c) {
    return ui ? mulib_vterm_set_cell(&ui->back, x, y, c) : 0;
}
int mulib_tui_fill(mulib_tui_t *ui, mulib_tui_rect_t r, mulib_term_cell_t c) {
    return ui ? mulib_vterm_fill(&ui->back, r.x, r.y, r.width, r.height, c) : 0;
}
int mulib_tui_clear(mulib_tui_t *ui, mulib_term_style_t style) {
    mulib_tui_rect_t r;
    mulib_term_cell_t c;
    if (!ui) return 0;
    r.x = r.y = 0; r.width = ui->back.grid.width; r.height = ui->back.grid.height;
    c.codepoint = ' '; c.style = style;
    return mulib_tui_fill(ui, r, c);
}
static mulib_tui_rect_t mulib_tui_clip_(const mulib_tui_t *ui, mulib_tui_rect_t r) {
    size_t w = ui->back.grid.width, h = ui->back.grid.height;
    if (r.x >= w || r.y >= h) { r.width = r.height = 0; return r; }
    if (r.width > w - r.x) r.width = w - r.x;
    if (r.height > h - r.y) r.height = h - r.y;
    return r;
}
int mulib_tui_text(mulib_tui_t *ui, mulib_tui_rect_t r, const char *s, size_t n, mulib_term_style_t style) {
    mulib_term_cell_t c;
    size_t x = 0, y = 0, used, pos = 0;
    uint32_t cp;
    c.codepoint = ' '; c.style = style;
    if (!ui || !ui->back.grid.data || !mulib_term_cell_valid(c) ||
        (!s && n) || !mulib_utf8_validate((const unsigned char *)s, n)) return 0;
    r = mulib_tui_clip_(ui, r);
    if (!r.width || !r.height) return 1;
    while (pos < n && y < r.height) {
        if (!mulib_utf8_decode((const unsigned char *)s + pos, n - pos, &cp, &used)) return 0;
        pos += used;
        if (cp == '\r') continue;
        if (cp == '\n') { x = 0; ++y; continue; }
        if (x == r.width) { x = 0; if (++y == r.height) break; }
        if (cp == '\t') {
            size_t end = (x / 4u + 1u) * 4u;
            if (end > r.width) end = r.width;
            c.codepoint = ' ';
            while (x < end) (void)mulib_tui_put(ui, r.x + x++, r.y + y, c);
        } else {
            c.codepoint = cp < 32 || (cp >= 127 && cp < 160) ? 0xfffdu : cp;
            (void)mulib_tui_put(ui, r.x + x++, r.y + y, c);
        }
    }
    return 1;
}
int mulib_tui_box(mulib_tui_t *ui, mulib_tui_rect_t r, mulib_term_style_t style) {
    mulib_term_cell_t c;
    size_t x, y;
    c.codepoint = '+'; c.style = style;
    if (!ui || !ui->back.grid.data || !mulib_term_cell_valid(c)) return 0;
    r = mulib_tui_clip_(ui, r);
    if (!r.width || !r.height) return 1;
    for (x = 0; x < r.width; ++x) {
        c.codepoint = x == 0 || x + 1 == r.width ? '+' : '-';
        (void)mulib_tui_put(ui, r.x + x, r.y, c);
        (void)mulib_tui_put(ui, r.x + x, r.y + r.height - 1, c);
    }
    c.codepoint = '|';
    for (y = 1; y + 1 < r.height; ++y) {
        (void)mulib_tui_put(ui, r.x, r.y + y, c);
        (void)mulib_tui_put(ui, r.x + r.width - 1, r.y + y, c);
    }
    return 1;
}
int mulib_tui_button(mulib_tui_t *ui, mulib_tui_rect_t r, const char *s, size_t n,
                     int focused, mulib_term_style_t style) {
    size_t pos = 0, count = 0, used;
    uint32_t cp;
    mulib_term_cell_t c;
    mulib_tui_rect_t label;
    c.codepoint = ' '; c.style = style;
    if (!ui || !ui->back.grid.data || !mulib_term_cell_valid(c) || (!s && n) ||
        !mulib_utf8_validate((const unsigned char *)s, n)) return 0;
    while (pos < n) {
        if (!mulib_utf8_decode((const unsigned char *)s + pos, n - pos, &cp, &used)) return 0;
        if (cp < 32 || (cp >= 127 && cp < 160)) return 0;
        pos += used; ++count;
    }
    if (focused) style.attributes |= MULIB_TERM_INVERSE;
    c.style = style;
    r = mulib_tui_clip_(ui, r);
    if (!mulib_tui_fill(ui, r, c) || !mulib_tui_box(ui, r, style)) return 0;
    if (r.width < 3 || r.height < 3) return 1;
    label.width = r.width - 2; label.height = 1;
    label.x = r.x + 1 + (count < label.width ? (label.width - count) / 2 : 0);
    label.y = r.y + r.height / 2;
    if (count < label.width) label.width = count;
    return mulib_tui_text(ui, label, s, n, style);
}
int mulib_tui_button_event(mulib_tui_rect_t r, const mulib_termkey_event_t *e, int focused) {
    if (!e || e->modifiers) return 0;
    if (e->type == MULIB_TERMKEY_KEY && focused)
        return e->key == MULIB_TERMKEY_ENTER || (e->key == MULIB_TERMKEY_CHAR && e->codepoint == ' ');
    return e->type == MULIB_TERMKEY_MOUSE && e->mouse_action == MULIB_TERMKEY_PRESS &&
        e->mouse_button == 1 && e->x >= r.x && e->x - r.x < r.width &&
        e->y >= r.y && e->y - r.y < r.height;
}
int mulib_tui_split(mulib_tui_rect_t r, int vertical, size_t first, size_t gap,
                    mulib_tui_rect_t *a, mulib_tui_rect_t *b) {
    size_t extent = vertical ? r.height : r.width;
    mulib_tui_rect_t left = r, right = r;
    if (!a || !b || a == b || r.width > SIZE_MAX - r.x || r.height > SIZE_MAX - r.y ||
        first > extent || gap > extent - first) return 0;
    if (vertical) {
        left.height = first; right.y += first + gap; right.height -= first + gap;
    } else {
        left.width = first; right.x += first + gap; right.width -= first + gap;
    }
    *a = left; *b = right;
    return 1;
}
int mulib_tui_cursor(mulib_tui_t *ui, size_t x, size_t y, int visible) {
    if (!ui || !ui->back.grid.data || x >= ui->back.grid.width || y >= ui->back.grid.height) return 0;
    ui->cursor_x = x; ui->cursor_y = y; ui->cursor_visible = !!visible;
    return 1;
}

static int mulib_tui_emit_(const mulib_terminfo_t *ti, const char *name,
                            const int32_t *p, mulib_strbuf_t *out) {
    return out->length <= 67108864u && mulib_terminfo_emit(ti, name, p, out) && out->length <= 67108864u;
}
static int mulib_tui_reset_(const mulib_terminfo_t *ti, mulib_strbuf_t *out) {
    /* sgr0 resets attributes, but need not restore the original color pair. */
    return mulib_tui_emit_(ti, "sgr0", NULL, out) && mulib_tui_emit_(ti, "op", NULL, out);
}
static int mulib_tui_style_(const mulib_terminfo_t *ti, mulib_term_style_t s, mulib_strbuf_t *out) {
    static const char *const names[] = {"bold", "dim", "sitm", "smul", "blink", "rev", "invis", "smxx"};
    unsigned i;
    int32_t p[9] = {0};
    if (!mulib_tui_reset_(ti, out)) return 0;
    for (i = 0; i < 8; ++i)
        if ((s.attributes & (1u << i)) && !mulib_tui_emit_(ti, names[i], NULL, out)) return 0;
    for (i = 0; i < 2; ++i) {
        uint32_t c = i ? s.background : s.foreground;
        if (c == MULIB_TERM_COLOR_DEFAULT) continue;
        if (c <= 255) {
            int32_t colors = mulib_terminfo_num(ti, "colors");
            if (colors >= 0 && c >= (uint32_t)colors) return 0;
            p[0] = (int32_t)c;
            if (!mulib_tui_emit_(ti, i ? "setab" : "setaf", p, out)) return 0;
        } else {
            char bytes[48];
            int count;
            if (!ti->ansi) return 0;
            count = snprintf(bytes, sizeof(bytes), "\033[%u;2;%u;%u;%um", i ? 48u : 38u,
                             (unsigned)((c >> 16) & 255u), (unsigned)((c >> 8) & 255u), (unsigned)(c & 255u));
            if (count < 0 || (size_t)count >= sizeof(bytes) ||
                !mulib_strbuf_append_n(out, bytes, (size_t)count)) return 0;
        }
    }
    return out->length <= 67108864u;
}
static const mulib_term_cell_t *mulib_tui_front_(const mulib_tui_t *ui, size_t x, size_t y) {
    return (const mulib_term_cell_t *)(const void *)(ui->front.data +
        (y * ui->front.width + x) * sizeof(mulib_term_cell_t));
}
int mulib_tui_render(mulib_tui_t *ui, const mulib_terminfo_t *info, mulib_strbuf_t *out) {
    mulib_terminfo_t fallback;
    const mulib_terminfo_t *ti = info;
    mulib_strbuf_t text;
    mulib_term_style_t current = mulib_term_style_default();
    size_t x, y;
    int started = 0, wrote = 0, ok = 0;
    int32_t p[9] = {0};
    if (!ui || !ui->back.grid.data || !ui->front.data || !out) return 0;
    if (!ti) { mulib_terminfo_init(&fallback); mulib_terminfo_ansi(&fallback); ti = &fallback; }
    mulib_strbuf_init(&text);
    for (y = 0; y < ui->back.grid.height; ++y) {
        int run = 0;
        for (x = 0; x < ui->back.grid.width; ++x) {
            const mulib_term_cell_t *c = mulib_tui_cell(ui, x, y);
            unsigned char bytes[4];
            size_t n;
            mulib_term_cell_t compare;
            if (!ui->invalid && mulib_term_cell_equal(*c, *mulib_tui_front_(ui, x, y))) { run = 0; continue; }
            if (!mulib_term_cell_valid(*c)) goto done;
            if (!started) {
                if (!mulib_tui_reset_(ti, &text)) goto done;
                started = 1;
            }
            if (!run) {
                p[0] = (int32_t)y; p[1] = (int32_t)x;
                if (!mulib_tui_emit_(ti, "cup", p, &text)) goto done;
                run = 1;
            }
            compare = *c; compare.style = current;
            if (!mulib_term_cell_equal(*c, compare)) {
                if (!mulib_tui_style_(ti, c->style, &text)) goto done;
                current = c->style;
            }
            n = mulib_utf8_encode(c->codepoint, bytes, sizeof(bytes));
            if (!n || !mulib_strbuf_append_n(&text, (const char *)bytes, n) || text.length > 67108864u) goto done;
            wrote = 1;
        }
    }
    if (wrote && !mulib_tui_reset_(ti, &text)) goto done;
    if (ui->invalid || ui->cursor_visible != ui->front_cursor_visible) {
        if (!mulib_tui_emit_(ti, ui->cursor_visible ? "cnorm" : "civis", NULL, &text)) goto done;
    }
    if (ui->cursor_visible && (wrote || ui->invalid || !ui->front_cursor_visible ||
        ui->cursor_x != ui->front_cursor_x || ui->cursor_y != ui->front_cursor_y)) {
        p[0] = (int32_t)ui->cursor_y; p[1] = (int32_t)ui->cursor_x;
        if (!mulib_tui_emit_(ti, "cup", p, &text)) goto done;
    }
    if (!mulib_strbuf_append_n(out, text.data, text.length)) goto done;
    memcpy(ui->front.data, ui->back.grid.data, ui->front.width * ui->front.height * sizeof(mulib_term_cell_t));
    ui->front_cursor_x = ui->cursor_x; ui->front_cursor_y = ui->cursor_y;
    ui->front_cursor_visible = ui->cursor_visible; ui->invalid = 0;
    ok = 1;
done:
    mulib_strbuf_free(&text);
    return ok;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TUI_IMPLEMENTED */
