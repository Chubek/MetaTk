#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRID
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMKEY
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TUI
#include "mulib-tui.h"

static void terminal_store16(unsigned char *p, unsigned n, int be) {
    p[be ? 0 : 1] = (unsigned char)(n >> 8);
    p[be ? 1 : 0] = (unsigned char)n;
}
static size_t terminal_fixture(unsigned char p[256], int wide, int be) {
    static const char strings[] = "\033[H\033[2J\0\033[%i%p1%d;%p2%dH\0";
    size_t pos = 18, i;
    memset(p, 0, 256);
    terminal_store16(p, wide ? 0x021e : 0x011a, be);
    terminal_store16(p + 2, 4, be);
    terminal_store16(p + 4, 2, be);
    terminal_store16(p + 6, 3, be);
    terminal_store16(p + 8, 11, be);
    terminal_store16(p + 10, (unsigned)sizeof(strings), be);
    memcpy(p + 12, "t|x", 4);
    p[16] = 0; p[17] = 1;
    for (i = 0; i < 3; ++i) {
        uint32_t n = i == 0 ? (wide ? 100000u : 80u) : i == 1 ? 8u : 24u;
        if (wide) {
            unsigned j;
            for (j = 0; j < 4; ++j) p[pos + (be ? 3u - j : j)] = (unsigned char)(n >> (8u * j));
            pos += 4;
        } else { terminal_store16(p + pos, (unsigned)n, be); pos += 2; }
    }
    for (i = 0; i < 11; ++i) terminal_store16(p + pos + 2 * i, 65535u, be);
    terminal_store16(p + pos + 10, 0, be);
    terminal_store16(p + pos + 12, 65534u, be);
    terminal_store16(p + pos + 20, 8, be);
    pos += 22;
    memcpy(p + pos, strings, sizeof(strings));
    return pos + sizeof(strings);
}
static void terminal_expect_expansion(const char *fmt, const int32_t p[9], const char *expected) {
    mulib_strbuf_t out;
    mulib_strbuf_init(&out);
    assert(mulib_terminfo_expand(fmt, p, &out));
    assert(!strcmp(out.data, expected));
    mulib_strbuf_free(&out);
}
static void test_terminal_terminfo(void) {
    unsigned char fixture[256], bad[256];
    size_t n, i;
    int wide, be;
    mulib_terminfo_t info;
    mulib_strbuf_t out;
    int32_t p[9] = {2, 4, 0, 0, 0, 0, 0, 0, 0};
    const char *failures[] = {"%", "%p0", "%p:", "%p", "%{", "%{}", "%{1", "%{2147483648}",
        "%{-2147483649}", "%{1}%{0}%/", "%{2147483647}%{1}%+", "%{-2147483648}%{-1}%/",
        "%d", "%+", "%!", "%Pz", "%PZ", "%gA", "%{0}%c", "%{256}%c", "%p1%s", "%:99d",
        "%?%{1}%tA", "%e", "%t", "%;", "%?%{1}%;", "%?%{0}%t%s%;", "%'{", "$<2", "$<.2>", "$<2**>"};
    mulib_terminfo_init(&info);
    mulib_terminfo_ansi(&info);
    assert(mulib_terminfo_bool(&info, "am") == 1);
    assert(mulib_terminfo_num(&info, "colors") == 256);
    assert(mulib_terminfo_num(&info, "cols") == -1);
    assert(!mulib_terminfo_str(&info, "not-a-capability"));
    assert(mulib_terminfo_bool(NULL, "am") == -1);
    assert(!mulib_terminfo_load_file(&info, "mulib-terminal-definitely-absent"));
    for (wide = 0; wide < 2; ++wide) for (be = 0; be < 2; ++be) {
        n = terminal_fixture(fixture, wide, be);
        for (i = 0; i < n; ++i) assert(!mulib_terminfo_load(&info, fixture, i));
        assert(mulib_terminfo_load(&info, fixture, n));
        assert(!strcmp(mulib_terminfo_names(&info), "t|x"));
        assert(mulib_terminfo_bool(&info, "am") == 1);
        assert(mulib_terminfo_bool_at(&info, 0) == 0);
        assert(mulib_terminfo_bool_at(&info, 2) == -1);
        assert(mulib_terminfo_num(&info, "cols") == (wide ? 100000 : 80));
        assert(mulib_terminfo_num(&info, "lines") == 24);
        assert(mulib_terminfo_num_at(&info, 2000) == -1);
        assert(!strcmp(mulib_terminfo_str(&info, "clear"), "\033[H\033[2J"));
        assert(!mulib_terminfo_str(&info, "el"));
        assert(!mulib_terminfo_str_at(&info, 0));
        assert(!mulib_terminfo_str_at(&info, 65535));
        mulib_strbuf_init(&out);
        assert(mulib_terminfo_emit(&info, "cup", p, &out));
        assert(!strcmp(out.data, "\033[3;5H"));
        mulib_strbuf_free(&out);
        assert(mulib_terminfo_load(&info, info.data, info.length));
        fixture[n] = 0xaa;
        assert(mulib_terminfo_load(&info, fixture, n + 1)); /* Extensions ignored. */
        memcpy(bad, fixture, n);
        bad[15] = 'x';
        assert(!mulib_terminfo_load(&info, bad, n));
        memcpy(bad, fixture, n); bad[16] = 3;
        assert(!mulib_terminfo_load(&info, bad, n));
        memcpy(bad, fixture, n); bad[0] = bad[1] = 0;
        assert(!mulib_terminfo_load(&info, bad, n));
        memcpy(bad, fixture, n);
        terminal_store16(bad + info.str_offset + 10, 32767, be);
        assert(!mulib_terminfo_load(&info, bad, n));
        assert(!strcmp(mulib_terminfo_names(&info), "t|x"));
    }
    n = terminal_fixture(fixture, 0, 0);
    {
        char path[128];
        unsigned attempt;
        FILE *file = NULL;
        for (attempt = 0; attempt < 100 && !file; ++attempt) {
            int count = snprintf(path, sizeof(path), "mulib-terminal-%lu-%u.bin",
                                 (unsigned long)clock(), attempt);
            assert(count > 0 && (size_t)count < sizeof(path));
            file = fopen(path, "wbx");
        }
        assert(file);
        assert(fwrite(fixture, 1, n, file) == n);
        assert(fclose(file) == 0);
        assert(mulib_terminfo_load_file(&info, path));
        assert(remove(path) == 0);
    }
    memcpy(bad, fixture, n);
    memset(bad + info.table_offset, 'x', info.table_size);
    assert(!mulib_terminfo_load(&info, bad, n));
    memcpy(bad, fixture, n);
    bad[16] = 255; bad[17] = 254;
    terminal_store16(bad + info.num_offset, 65535, 0);
    terminal_store16(bad + info.num_offset + 2, 65534, 0);
    assert(mulib_terminfo_load(&info, bad, n));
    assert(mulib_terminfo_bool_at(&info, 0) == -1 && mulib_terminfo_bool_at(&info, 1) == -1);
    assert(mulib_terminfo_num_at(&info, 0) == -1 && mulib_terminfo_num_at(&info, 1) == -1);
    memset(bad, 0, sizeof(bad));
    terminal_store16(bad, 0x011a, 0); terminal_store16(bad + 2, 3, 0);
    memcpy(bad + 12, "xx", 3);
    assert(!mulib_terminfo_load(&info, bad, 15)); /* Odd name field requires padding. */
    assert(mulib_terminfo_load(&info, bad, 16));
    assert(!strcmp(mulib_terminfo_names(&info), "xx") && !mulib_terminfo_str(&info, "cup"));
    terminal_expect_expansion("%% %i%p1%02d,%p2%:3d", p, "% 03,  5");
    terminal_expect_expansion("%{10}%{3}%-%d/%{9}%{2}%*%d/%{9}%{2}%/%d/%{9}%{2}%m%d", p, "7/18/4/1");
    terminal_expect_expansion("%{42}%Pa%ga%d:%'A'%c", p, "42:A");
    terminal_expect_expansion("%{-2147483648}%d %{-1}%~%d %{-1}%x", p, "-2147483648 0 ffffffff");
    terminal_expect_expansion("%{7}%{3}%&%d%{7}%{3}%|%d%{7}%{3}%^%d%{0}%!%d", p, "3741");
    terminal_expect_expansion("%{1}%{0}%A%d%{1}%{0}%O%d%{3}%{3}%=%d%{4}%{2}%>%d", p, "0111");
    terminal_expect_expansion("a$<12.5*/>b$<0>c", p, "abc");
    terminal_expect_expansion("%?%p1%{8}%<%tlow%e%p1%{16}%<%tmid%ehigh%;", p, "low");
    p[0] = 10;
    terminal_expect_expansion("%?%p1%{8}%<%tlow%e%p1%{16}%<%tmid%ehigh%;", p, "mid");
    p[0] = 200;
    terminal_expect_expansion("%?%p1%{8}%<%tlow%e%p1%{16}%<%tmid%ehigh%;", p, "high");
    p[1] = 0;
    terminal_expect_expansion("%?%p1%tA%?%p2%tB%eC%;%eD%;", p, "AC");
    mulib_strbuf_init(&out);
    assert(mulib_strbuf_append(&out, "unchanged"));
    for (i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        assert(!mulib_terminfo_expand(failures[i], p, &out));
        assert(!strcmp(out.data, "unchanged"));
    }
    p[0] = INT32_MAX;
    assert(!mulib_terminfo_expand("%i", p, &out) && !strcmp(out.data, "unchanged"));
    {
        char stack_overflow[200] = "";
        char *long_format = (char *)malloc(65538);
        assert(long_format);
        for (i = 0; i < 33; ++i) strcat(stack_overflow, "%{1}");
        assert(!mulib_terminfo_expand(stack_overflow, NULL, &out));
        memset(long_format, 'x', 65537); long_format[65537] = 0;
        assert(!mulib_terminfo_expand(long_format, NULL, &out));
        free(long_format);
        assert(!strcmp(out.data, "unchanged"));
    }
    mulib_strbuf_free(&out);
    mulib_terminfo_free(&info);
    mulib_terminfo_free(&info);
}

static mulib_termkey_event_t terminal_key(const char *text, int fragmented) {
    mulib_termkey_t d;
    mulib_termkey_event_t e;
    size_t n = strlen(text), i;
    mulib_termkey_init(&d);
    if (fragmented) {
        for (i = 0; i < n; ++i) {
            assert(mulib_termkey_feed(&d, text + i, 1) == 1);
            assert(mulib_termkey_next(&d, &e, 0) == (i + 1 == n ? 1 : 0));
        }
    } else {
        assert(mulib_termkey_feed(&d, text, n) == n);
        assert(mulib_termkey_next(&d, &e, 0) == 1);
    }
    assert(e.length == n && !memcmp(e.raw, text, n));
    assert(mulib_termkey_next(&d, &e, 0) == 0);
    return e;
}
static void test_terminal_termkey(void) {
    static const struct { const char *sequence; mulib_termkey_code_t key; unsigned modifiers; } cases[] = {
        {"\033[A", MULIB_TERMKEY_UP, 0}, {"\033OB", MULIB_TERMKEY_DOWN, 0},
        {"\033[1;5D", MULIB_TERMKEY_LEFT, MULIB_TERMKEY_CTRL},
        {"\033[1;3C", MULIB_TERMKEY_RIGHT, MULIB_TERMKEY_ALT},
        {"\033[Z", MULIB_TERMKEY_TAB, MULIB_TERMKEY_SHIFT},
        {"\033[1~", MULIB_TERMKEY_HOME, 0}, {"\033[F", MULIB_TERMKEY_END, 0},
        {"\033[2~", MULIB_TERMKEY_INSERT, 0}, {"\033[3~", MULIB_TERMKEY_DELETE, 0},
        {"\033[5~", MULIB_TERMKEY_PAGE_UP, 0}, {"\033[6~", MULIB_TERMKEY_PAGE_DOWN, 0},
        {"\033OP", MULIB_TERMKEY_F1, 0}, {"\033[24;2~", MULIB_TERMKEY_F12, MULIB_TERMKEY_SHIFT},
        {"\033[9;5u", MULIB_TERMKEY_TAB, MULIB_TERMKEY_CTRL},
        {"\r", MULIB_TERMKEY_ENTER, 0}, {"\177", MULIB_TERMKEY_BACKSPACE, 0}
    };
    mulib_termkey_t d;
    mulib_termkey_event_t e, f;
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        e = terminal_key(cases[i].sequence, 0);
        f = terminal_key(cases[i].sequence, 1);
        assert(e.key == cases[i].key && e.modifiers == cases[i].modifiers);
        assert(e.type == MULIB_TERMKEY_KEY && !memcmp(&e, &f, sizeof(e)));
    }
    e = terminal_key("\xf4\x8f\xbf\xbf", 1);
    assert(e.codepoint == 0x10ffff);
    e = terminal_key("\033\xe2\x82\xac", 1);
    assert(e.codepoint == 0x20ac && e.modifiers == MULIB_TERMKEY_ALT);
    e = terminal_key("\033[1114111u", 1);
    assert(e.codepoint == 0x10ffff);
    e = terminal_key("\001", 0);
    assert(e.codepoint == 'a' && e.modifiers == MULIB_TERMKEY_CTRL);
    e = terminal_key("\033[<20;12;3M", 1);
    assert(e.type == MULIB_TERMKEY_MOUSE && e.x == 11 && e.y == 2 && e.mouse_button == 1);
    assert(e.modifiers == (MULIB_TERMKEY_SHIFT | MULIB_TERMKEY_CTRL) && e.mouse_action == MULIB_TERMKEY_PRESS);
    e = terminal_key("\033[<0;12;3m", 1);
    assert(e.mouse_action == MULIB_TERMKEY_RELEASE);
    e = terminal_key("\033[<65;1;1M", 1);
    assert(e.mouse_action == MULIB_TERMKEY_WHEEL && e.mouse_button == 5);
    e = terminal_key("\033[<32;1;1M", 1);
    assert(e.mouse_action == MULIB_TERMKEY_MOVE);
    e = terminal_key("\033[<0;0;1M", 0);
    assert(e.type == MULIB_TERMKEY_UNKNOWN);
    e = terminal_key("\033[<128;1;1M", 0);
    assert(e.type == MULIB_TERMKEY_UNKNOWN);
    e = terminal_key("\033[99999999999999A", 0);
    assert(e.type == MULIB_TERMKEY_UNKNOWN);
    e = terminal_key("\033[1;99A", 0);
    assert(e.type == MULIB_TERMKEY_UNKNOWN);
    mulib_termkey_init(&d);
    assert(mulib_termkey_feed(&d, "\033", 1) == 1);
    assert(!mulib_termkey_next(&d, &e, 0));
    assert(mulib_termkey_next(&d, &e, 1) == 1 && e.key == MULIB_TERMKEY_ESCAPE);
    assert(mulib_termkey_feed(&d, "\033[", 2) == 2);
    assert(mulib_termkey_next(&d, &e, 1) == 1 && e.type == MULIB_TERMKEY_UNKNOWN);
    assert(mulib_termkey_feed(&d, "\xe2\x82", 2) == 2);
    assert(!mulib_termkey_next(&d, &e, 0));
    assert(mulib_termkey_next(&d, &e, 1) == 1 && e.codepoint == 0xfffd && e.length == 1);
    assert(mulib_termkey_next(&d, &e, 1) == 1 && e.codepoint == 0xfffd);
    assert(mulib_termkey_feed(&d, "\xe2(\xa1", 3) == 3);
    assert(mulib_termkey_next(&d, &e, 0) == 1 && e.codepoint == 0xfffd);
    assert(mulib_termkey_next(&d, &e, 0) == 1 && e.codepoint == '(');
    assert(mulib_termkey_next(&d, &e, 0) == 1 && e.codepoint == 0xfffd);
    {
        unsigned char long_sequence[150];
        memset(long_sequence, '1', sizeof(long_sequence));
        long_sequence[0] = 27; long_sequence[1] = '[';
        assert(mulib_termkey_feed(&d, long_sequence, sizeof(long_sequence)) == MULIB_TERMKEY_CAPACITY);
        assert(mulib_termkey_next(&d, &e, 0) == 1 && e.type == MULIB_TERMKEY_UNKNOWN);
        assert(e.length == MULIB_TERMKEY_CAPACITY);
    }
    mulib_termkey_init(&d);
    assert(mulib_termkey_feed(&d, "\033[200~", 6) == 6);
    assert(mulib_termkey_next(&d, &e, 0) == 1 && e.type == MULIB_TERMKEY_PASTE_BEGIN);
    assert(mulib_termkey_feed(&d, "a\033[A\r\033[20", 9) == 9);
    assert(mulib_termkey_next(&d, &e, 0) == 1 && e.type == MULIB_TERMKEY_PASTE_TEXT);
    assert(e.length == 5 && !memcmp(e.raw, "a\033[A\r", 5));
    assert(!mulib_termkey_next(&d, &e, 0));
    assert(mulib_termkey_feed(&d, "1~x", 3) == 3);
    assert(mulib_termkey_next(&d, &e, 0) == 1 && e.type == MULIB_TERMKEY_PASTE_END);
    assert(mulib_termkey_next(&d, &e, 0) == 1 && e.codepoint == 'x');
    assert(mulib_termkey_next(NULL, &e, 0) == -1);
    assert(mulib_termkey_feed(&d, NULL, 2) == 0);
    {
        unsigned char bytes[128], recovered[128];
        uint32_t random = 7;
        size_t round;
        for (round = 0; round < 2000; ++round) {
            size_t length = round % 128 + 1, total = 0;
            mulib_termkey_init(&d);
            for (i = 0; i < length; ++i) {
                random = random * 1664525u + 1013904223u;
                bytes[i] = (unsigned char)(random >> 24);
            }
            assert(mulib_termkey_feed(&d, bytes, length) == length);
            while (mulib_termkey_next(&d, &e, 1) == 1) {
                assert(e.length && e.length <= length - total);
                memcpy(recovered + total, e.raw, e.length);
                total += e.length;
            }
            assert(total == length && !memcmp(bytes, recovered, length));
        }
    }
}

static void terminal_feed(mulib_vterm_t *v, const char *s) {
    assert(mulib_vterm_feed(v, s, strlen(s)));
}
static void terminal_row(const mulib_vterm_t *v, size_t y, const char *expected) {
    size_t x;
    assert(strlen(expected) == v->grid.width);
    for (x = 0; x < v->grid.width; ++x) assert(mulib_vterm_cell(v, x, y)->codepoint == (unsigned char)expected[x]);
}
static void terminal_screens_equal(const mulib_vterm_t *a, const mulib_vterm_t *b) {
    size_t x, y;
    assert(a->grid.width == b->grid.width && a->grid.height == b->grid.height);
    for (y = 0; y < a->grid.height; ++y) for (x = 0; x < a->grid.width; ++x)
        assert(mulib_term_cell_equal(*mulib_vterm_cell(a, x, y), *mulib_vterm_cell(b, x, y)));
}
static void test_terminal_vterm(void) {
    mulib_vterm_t v, whole, pieces;
    mulib_term_cell_t c;
    size_t i, x, y;
    assert(!mulib_vterm_init(&v, SIZE_MAX, 2));
    mulib_vterm_free(&v);
    assert(!mulib_vterm_init(&v, 0, 2));
    assert(mulib_vterm_init(&v, 4, 4));
    terminal_feed(&v, "abcd");
    assert(v.cursor_x == 3 && v.cursor_y == 0 && v.wrap_pending);
    terminal_feed(&v, "\033[1;31mE");
    assert(v.cursor_x == 1 && v.cursor_y == 1 && !v.wrap_pending);
    assert(mulib_vterm_cell(&v, 0, 1)->style.foreground == 1);
    assert(mulib_vterm_cell(&v, 0, 1)->style.attributes == MULIB_TERM_BOLD);
    terminal_feed(&v, "\033[2J");
    assert(v.cursor_x == 1 && v.cursor_y == 1);
    terminal_row(&v, 0, "    ");
    assert(mulib_vterm_cell(&v, 0, 0)->style.foreground == 1);
    terminal_feed(&v, "\033[0m\033[HAAAA\r\nBBBB\r\nCCCC\r\nDDDD");
    terminal_row(&v, 0, "AAAA"); terminal_row(&v, 3, "DDDD");
    terminal_feed(&v, "\033[2;3r\033[3;1H\n");
    terminal_row(&v, 0, "AAAA"); terminal_row(&v, 1, "CCCC");
    terminal_row(&v, 2, "    "); terminal_row(&v, 3, "DDDD");
    terminal_feed(&v, "\033[2;1H\033M");
    terminal_row(&v, 1, "    "); terminal_row(&v, 2, "CCCC");
    terminal_feed(&v, "\033[?6h\033[1;1HX");
    assert(mulib_vterm_cell(&v, 0, 1)->codepoint == 'X');
    terminal_feed(&v, "\033[999B"); assert(v.cursor_y == 2);
    assert(mulib_vterm_move(&v, 0, 0)); /* Absolute API remains safe in origin mode. */
    terminal_feed(&v, "\033[A"); assert(v.cursor_y == 1);
    terminal_feed(&v, "\033[?6l\033[r\033[H\033[2Kabcd\r\033[2G\033[2@");
    terminal_row(&v, 0, "a  b");
    terminal_feed(&v, "\033[P"); terminal_row(&v, 0, "a b ");
    terminal_feed(&v, "\033[2X"); terminal_row(&v, 0, "a   ");
    terminal_feed(&v, "\033[1K"); terminal_row(&v, 0, "    ");
    terminal_feed(&v, "\033[H\033[?7labcde"); terminal_row(&v, 0, "abce");
    assert(v.cursor_y == 0 && v.cursor_x == 3);
    terminal_feed(&v, "\033[?7h\033[2;2H\033[38;5;200;48;2;1;2;3;3;4m\0337Q\033[0m\033[4;4H\0338R");
    assert(mulib_vterm_cell(&v, 1, 1)->codepoint == 'R');
    assert(mulib_vterm_cell(&v, 1, 1)->style.foreground == 200);
    assert(mulib_vterm_cell(&v, 1, 1)->style.background == MULIB_TERM_RGB(1, 2, 3));
    assert(mulib_vterm_cell(&v, 1, 1)->style.attributes == (MULIB_TERM_ITALIC | MULIB_TERM_UNDERLINE));
    terminal_feed(&v, "\033[23;24;39;49m");
    assert(v.style.attributes == 0 && v.style.foreground == MULIB_TERM_COLOR_DEFAULT);
    assert(mulib_vterm_resize(&v, 6, 5));
    assert(mulib_vterm_cell(&v, 1, 1)->codepoint == 'R');
    assert(mulib_vterm_cell(&v, 5, 4)->codepoint == ' ');
    assert(!mulib_vterm_resize(&v, SIZE_MAX, SIZE_MAX) && v.grid.width == 6);
    c.codepoint = 'x'; c.style = mulib_term_style_default();
    assert(mulib_vterm_fill(&v, 5, 4, SIZE_MAX, SIZE_MAX, c));
    assert(mulib_vterm_cell(&v, 5, 4)->codepoint == 'x');
    assert(!mulib_vterm_cell(&v, SIZE_MAX, 0));
    c.codepoint = 27; assert(!mulib_vterm_set_cell(&v, 0, 0, c));
    c.codepoint = 0xd800; assert(!mulib_vterm_set_cell(&v, 0, 0, c));
    c.codepoint = 'x'; c.style.foreground = 256; assert(!mulib_term_cell_valid(c));
    mulib_vterm_scroll(&v, INT_MIN);
    for (y = 0; y < 5; ++y) for (x = 0; x < 6; ++x) assert(mulib_vterm_cell(&v, x, y)->codepoint == ' ');
    mulib_vterm_reset(&v);
    terminal_feed(&v, "\xe2\x82");
    assert(v.cursor_x == 0);
    terminal_feed(&v, "\xac"); assert(mulib_vterm_cell(&v, 0, 0)->codepoint == 0x20ac);
    terminal_feed(&v, "\xe2X");
    assert(mulib_vterm_cell(&v, 1, 0)->codepoint == 0xfffd);
    assert(mulib_vterm_cell(&v, 2, 0)->codepoint == 'X');
    terminal_feed(&v, "\xe2"); mulib_vterm_finish(&v);
    assert(mulib_vterm_cell(&v, 3, 0)->codepoint == 0xfffd && v.invalid_input == 2);
    mulib_vterm_reset(&v);
    terminal_feed(&v, "\033]title\033");
    terminal_feed(&v, "\\X\033Pignored\007");
    terminal_feed(&v, "\033\\Y");
    terminal_row(&v, 0, "XY    ");
    terminal_feed(&v, "\033[12\030Z"); assert(mulib_vterm_cell(&v, 2, 0)->codepoint == 'Z');
    {
        char long_escape[5000];
        memset(long_escape, '1', sizeof(long_escape));
        terminal_feed(&v, "\033[");
        assert(mulib_vterm_feed(&v, long_escape, sizeof(long_escape)));
        terminal_feed(&v, "mA"); assert(mulib_vterm_cell(&v, 3, 0)->codepoint == 'A');
        terminal_feed(&v, "\033]");
        assert(mulib_vterm_feed(&v, long_escape, sizeof(long_escape)));
        terminal_feed(&v, "\007B"); assert(mulib_vterm_cell(&v, 4, 0)->codepoint == 'B');
        assert(v.invalid_input == 2);
    }
    terminal_feed(&v, "\033[123"); mulib_vterm_finish(&v); assert(v.state == 0);
    mulib_vterm_free(&v);
    assert(mulib_vterm_init(&whole, 20, 7));
    assert(mulib_vterm_init(&pieces, 20, 7));
    {
        unsigned char bytes[4096];
        uint32_t random = 0x12345678u;
        for (i = 0; i < sizeof(bytes); ++i) {
            random = random * 1664525u + 1013904223u;
            bytes[i] = (unsigned char)(random >> 24);
        }
        assert(mulib_vterm_feed(&whole, bytes, sizeof(bytes)));
        for (i = 0; i < sizeof(bytes); ++i) {
            assert(mulib_vterm_feed(&pieces, bytes + i, 1));
            assert(pieces.cursor_x < 20 && pieces.cursor_y < 7);
        }
        mulib_vterm_finish(&whole); mulib_vterm_finish(&pieces);
        terminal_screens_equal(&whole, &pieces);
        assert(whole.cursor_x == pieces.cursor_x && whole.cursor_y == pieces.cursor_y);
        for (y = 0; y < 7; ++y) for (x = 0; x < 20; ++x)
            assert(mulib_term_cell_valid(*mulib_vterm_cell(&pieces, x, y)));
    }
    mulib_vterm_free(&whole); mulib_vterm_free(&pieces);
}

static void test_terminal_vterm_margins(void) {
    static const struct {
        unsigned origin;
        size_t row, column, expected_row, expected_column;
        const char *motion;
    } cases[] = {
        {0, 1, 0, 1, 0, "\033[A"}, {0, 3, 0, 3, 0, "\033[B"},
        {0, 2, 1, 1, 1, "\033[99A"}, {0, 2, 1, 3, 1, "\033[99B"},
        {0, 0, 1, 0, 1, "\033[99A"}, {0, 4, 1, 4, 1, "\033[99B"},
        {0, 4, 1, 1, 1, "\033[99A"}, {0, 0, 1, 3, 1, "\033[99B"},
        {0, 1, 2, 1, 0, "\033[F"}, {0, 3, 2, 3, 0, "\033[E"},
        {0, 3, 2, 3, 2, "\033[e"}, {0, 2, 2, 3, 0, "\033[99E"},
        {0, 2, 2, 1, 0, "\033[99F"}, {0, 2, 2, 3, 2, "\033[99e"},
        {1, 0, 0, 1, 0, "\033[A"}, {1, 4, 0, 3, 0, "\033[B"},
        {1, 1, 0, 1, 0, "\033[99A"}, {1, 3, 0, 3, 0, "\033[99B"},
        {1, 0, 0, 0, 1, "\033[C"}, {1, 4, 2, 4, 1, "\033[D"},
        {1, 0, 2, 0, 0, "\033[1G"}, {1, 4, 0, 4, 2, "\033[3G"},
        {0, 2, 0, 0, 0, "\033[1d"}, {0, 2, 0, 4, 0, "\033[5;1H"},
        {1, 0, 0, 1, 0, "\033[1d"}, {1, 4, 0, 3, 0, "\033[99;1H"}
    };
    static const char *const nonmotions[] = {
        "\033[K", "\033[J", "\033[@", "\033[P", "\033[X", "\033[L", "\033[M",
        "\033[S", "\033[T", "\033[1m", "\033[s"
    };
    mulib_vterm_t v;
    mulib_term_cell_t cell;
    size_t i, row;
    assert(mulib_vterm_init(&v, 3, 5));
    terminal_feed(&v, "\033[2;4r\033[2;1H\033[AX");
    assert(v.cursor_y == 1 && mulib_vterm_cell(&v, 0, 1)->codepoint == 'X');
    mulib_vterm_reset(&v);
    terminal_feed(&v, "\033[2;4r\033[4;1H\033[BX");
    assert(v.cursor_y == 3 && mulib_vterm_cell(&v, 0, 3)->codepoint == 'X');
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        mulib_vterm_reset(&v);
        terminal_feed(&v, "\033[2;4r");
        if (cases[i].origin) terminal_feed(&v, "\033[?6h");
        assert(mulib_vterm_move(&v, cases[i].column, cases[i].row));
        terminal_feed(&v, cases[i].motion);
        assert(v.cursor_y == cases[i].expected_row && v.cursor_x == cases[i].expected_column);
    }
    for (row = 0; row <= 4; row += 4) {
        for (i = 0; i < sizeof(nonmotions) / sizeof(nonmotions[0]); ++i) {
            mulib_vterm_reset(&v);
            terminal_feed(&v, "\033[2;4r\033[?6h");
            assert(mulib_vterm_move(&v, 0, row));
            terminal_feed(&v, nonmotions[i]);
            assert(v.cursor_x == 0 && v.cursor_y == row);
        }
    }
    mulib_vterm_reset(&v);
    terminal_feed(&v, "\033[2;4r\033[?6h");
    cell.codepoint = 'A'; cell.style = mulib_term_style_default();
    assert(mulib_vterm_fill(&v, 0, 0, 3, 1, cell));
    cell.codepoint = 'B';
    assert(mulib_vterm_fill(&v, 0, 1, 3, 1, cell));
    assert(mulib_vterm_move(&v, 1, 0));
    terminal_feed(&v, "\033[K");
    assert(v.cursor_x == 1 && v.cursor_y == 0);
    terminal_row(&v, 0, "A  "); terminal_row(&v, 1, "BBB");
    cell.codepoint = 'A';
    assert(mulib_vterm_fill(&v, 0, 0, 3, 1, cell));
    terminal_feed(&v, "\033[P");
    assert(v.cursor_x == 1 && v.cursor_y == 0);
    terminal_row(&v, 0, "AA "); terminal_row(&v, 1, "BBB");
    mulib_vterm_free(&v);
}

static void terminal_color_cap(unsigned char *record, size_t offsets, size_t table,
                                size_t *size, size_t index, const char *value) {
    size_t length = strlen(value) + 1;
    assert(table + *size + length <= 2048);
    terminal_store16(record + offsets + 2 * index, (unsigned)*size, 0);
    memcpy(record + table + *size, value, length);
    *size += length;
}
static void terminal_color_fixture(mulib_terminfo_t *info, const char *op) {
    unsigned char record[2048] = {0};
    const size_t numbers = 14, offsets = 42, table = 764;
    size_t size = 0, i;
    terminal_store16(record, 0x011a, 0);
    terminal_store16(record + 2, 2, 0);
    terminal_store16(record + 6, 14, 0);
    terminal_store16(record + 8, 361, 0);
    memcpy(record + 12, "t", 2);
    for (i = 0; i < 14; ++i) terminal_store16(record + numbers + 2 * i, 65535, 0);
    terminal_store16(record + numbers + 26, 256, 0);
    for (i = 0; i < 361; ++i) terminal_store16(record + offsets + 2 * i, 65535, 0);
    terminal_color_cap(record, offsets, table, &size, 10, "\033[%i%p1%d;%p2%dH");
    terminal_color_cap(record, offsets, table, &size, 13, "\033[?25l");
    terminal_color_cap(record, offsets, table, &size, 16, "\033[?25h");
    terminal_color_cap(record, offsets, table, &size, 27, "\033[1m");
    terminal_color_cap(record, offsets, table, &size, 39, "\033[22;23;24;25;27;28;29m");
    if (op) terminal_color_cap(record, offsets, table, &size, 297, op);
    terminal_color_cap(record, offsets, table, &size, 359, "\033[38;5;%p1%dm");
    terminal_color_cap(record, offsets, table, &size, 360, "\033[48;5;%p1%dm");
    terminal_store16(record + 10, (unsigned)size, 0);
    assert(mulib_terminfo_load(info, record, table + size));
}
static void test_terminal_tui_color_reset(void) {
    static const uint32_t foregrounds[6] = {1, MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT,
        1, 1, MULIB_TERM_COLOR_DEFAULT};
    static const uint32_t backgrounds[6] = {MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT, 4,
        4, MULIB_TERM_COLOR_DEFAULT, 4};
    mulib_tui_t ui;
    mulib_vterm_t replay;
    mulib_terminfo_t good, bad;
    mulib_strbuf_t output;
    mulib_term_cell_t cell;
    unsigned char front[6 * sizeof(mulib_term_cell_t)];
    size_t i, length, front_x, front_y;
    unsigned front_visible;
    assert(mulib_tui_init(&ui, 3, 2));
    assert(mulib_vterm_init(&replay, 3, 2));
    mulib_terminfo_init(&good); mulib_terminfo_init(&bad);
    terminal_color_fixture(&good, "\033[39;49m");
    terminal_color_fixture(&bad, NULL);
    mulib_strbuf_init(&output);
    assert(mulib_strbuf_append(&output, "prefix"));
    for (i = 0; i < 6; ++i) {
        cell.codepoint = (uint32_t)'A' + (uint32_t)i;
        cell.style = mulib_term_style_default();
        cell.style.foreground = foregrounds[i]; cell.style.background = backgrounds[i];
        if (i == 3) cell.style.attributes = MULIB_TERM_BOLD;
        assert(mulib_tui_put(&ui, i % 3, i / 3, cell));
    }
    memcpy(front, ui.front.data, sizeof(front));
    assert(!mulib_tui_render(&ui, &bad, &output));
    assert(ui.invalid && !strcmp(output.data, "prefix") && !memcmp(front, ui.front.data, sizeof(front)));
    terminal_feed(&replay, "\033[38;5;2;48;5;3m"); /* Initial terminal colors need not be default. */
    length = output.length;
    assert(mulib_tui_render(&ui, &good, &output));
    assert(mulib_vterm_feed(&replay, output.data + length, output.length - length));
    terminal_screens_equal(&ui.back, &replay);
    assert(mulib_vterm_cell(&replay, 1, 0)->style.foreground == MULIB_TERM_COLOR_DEFAULT);
    assert(replay.style.foreground == MULIB_TERM_COLOR_DEFAULT && replay.style.background == MULIB_TERM_COLOR_DEFAULT);
    assert(replay.style.attributes == 0);
    length = output.length;
    assert(mulib_tui_render(&ui, &bad, &output) && output.length == length);
    assert(mulib_tui_cursor(&ui, 1, 0, 1));
    assert(mulib_tui_render(&ui, &bad, &output)); /* Cursor-only output needs no color reset. */
    assert(mulib_vterm_feed(&replay, output.data + length, output.length - length));
    assert(replay.cursor_visible && replay.cursor_x == 1 && replay.cursor_y == 0);
    length = output.length;
    memcpy(front, ui.front.data, sizeof(front));
    front_x = ui.front_cursor_x; front_y = ui.front_cursor_y; front_visible = ui.front_cursor_visible;
    cell = *mulib_tui_cell(&ui, 0, 0); cell.style.foreground = 2;
    assert(mulib_tui_put(&ui, 0, 0, cell));
    assert(mulib_tui_cursor(&ui, 2, 1, 0));
    assert(!mulib_tui_render(&ui, &bad, &output));
    assert(output.length == length && !ui.invalid && !memcmp(front, ui.front.data, sizeof(front)));
    assert(ui.front_cursor_x == front_x && ui.front_cursor_y == front_y && ui.front_cursor_visible == front_visible);
    terminal_color_fixture(&bad, "%p0");
    assert(!mulib_tui_render(&ui, &bad, &output));
    assert(output.length == length && !memcmp(front, ui.front.data, sizeof(front)));
    assert(mulib_tui_render(&ui, &good, &output));
    assert(mulib_vterm_feed(&replay, output.data + length, output.length - length));
    terminal_screens_equal(&ui.back, &replay);
    length = output.length;
    memcpy(front, ui.front.data, sizeof(front));
    assert(mulib_tui_clear(&ui, mulib_term_style_default()));
    mulib_tui_invalidate(&ui);
    terminal_color_fixture(&bad, NULL);
    assert(!mulib_tui_render(&ui, &bad, &output)); /* Default-only repaint also requires op. */
    assert(output.length == length && ui.invalid && !memcmp(front, ui.front.data, sizeof(front)));
    assert(mulib_tui_render(&ui, &good, &output));
    assert(mulib_vterm_feed(&replay, output.data + length, output.length - length));
    terminal_screens_equal(&ui.back, &replay);
    mulib_strbuf_free(&output);
    mulib_terminfo_free(&good); mulib_terminfo_free(&bad);
    mulib_vterm_free(&replay); mulib_tui_free(&ui);
}

static void test_terminal_tui(void) {
    mulib_tui_t ui;
    mulib_vterm_t replay;
    mulib_terminfo_t empty;
    mulib_strbuf_t output;
    mulib_term_style_t style = mulib_term_style_default();
    mulib_term_cell_t c;
    mulib_tui_rect_t full = {0, 0, 12, 6}, button = {1, 1, 10, 3}, a, b;
    mulib_termkey_event_t event;
    size_t n, i;
    assert(mulib_tui_init(&ui, 12, 6));
    assert(mulib_vterm_init(&replay, 12, 6));
    mulib_terminfo_init(&empty);
    mulib_strbuf_init(&output);
    assert(mulib_tui_split(full, 1, 2, 1, &a, &b));
    assert(a.height == 2 && b.y == 3 && b.height == 3);
    assert(mulib_tui_split(full, 0, 4, 2, &a, &b));
    assert(a.width == 4 && b.x == 6 && b.width == 6);
    assert(!mulib_tui_split(full, 0, 12, 1, &a, &b));
    a = full; a.x = SIZE_MAX;
    assert(!mulib_tui_split(a, 0, 1, 1, &a, &b));
    assert(mulib_tui_text(&ui, full, "ab\tZ\n\xe2\x82\xac\033", 10, style));
    assert(mulib_tui_cell(&ui, 4, 0)->codepoint == 'Z');
    assert(mulib_tui_cell(&ui, 0, 1)->codepoint == 0x20ac);
    assert(mulib_tui_cell(&ui, 1, 1)->codepoint == 0xfffd);
    assert(!mulib_tui_text(&ui, full, "\xc0\xaf", 2, style));
    assert(mulib_tui_cell(&ui, 0, 0)->codepoint == 'a');
    assert(mulib_tui_clear(&ui, style));
    assert(mulib_tui_button(&ui, button, "OK", 2, 1, style));
    assert(!mulib_tui_button(&ui, button, "bad\n", 4, 0, style));
    memset(&event, 0, sizeof(event));
    event.type = MULIB_TERMKEY_KEY; event.key = MULIB_TERMKEY_ENTER;
    assert(mulib_tui_button_event(button, &event, 1));
    assert(!mulib_tui_button_event(button, &event, 0));
    event.modifiers = MULIB_TERMKEY_ALT;
    assert(!mulib_tui_button_event(button, &event, 1));
    event.modifiers = 0; event.type = MULIB_TERMKEY_MOUSE;
    event.mouse_action = MULIB_TERMKEY_PRESS; event.mouse_button = 1; event.x = event.y = 1;
    assert(mulib_tui_button_event(button, &event, 0));
    event.x = 11; assert(!mulib_tui_button_event(button, &event, 0));
    assert(mulib_strbuf_append(&output, "prefix"));
    assert(!mulib_tui_render(&ui, &empty, &output));
    assert(ui.invalid && !strcmp(output.data, "prefix"));
    n = output.length;
    assert(mulib_tui_render(&ui, NULL, &output));
    assert(mulib_vterm_feed(&replay, output.data + n, output.length - n));
    terminal_screens_equal(&ui.back, &replay);
    n = output.length;
    assert(mulib_tui_render(&ui, NULL, &output) && output.length == n);
    style.foreground = 202; style.background = MULIB_TERM_RGB(10, 20, 30);
    style.attributes = MULIB_TERM_BOLD | MULIB_TERM_UNDERLINE | MULIB_TERM_STRIKE;
    c.codepoint = 0x2603; c.style = style;
    assert(mulib_tui_put(&ui, 11, 5, c)); /* Bottom-right must not scroll on replay. */
    assert(!mulib_tui_render(&ui, &empty, &output) && output.length == n);
    assert(mulib_tui_render(&ui, NULL, &output));
    assert(mulib_vterm_feed(&replay, output.data + n, output.length - n));
    terminal_screens_equal(&ui.back, &replay);
    n = output.length;
    assert(mulib_tui_cursor(&ui, 2, 4, 1));
    assert(mulib_tui_render(&ui, NULL, &output));
    assert(mulib_vterm_feed(&replay, output.data + n, output.length - n));
    assert(replay.cursor_visible && replay.cursor_x == 2 && replay.cursor_y == 4);
    n = output.length;
    assert(mulib_tui_render(&ui, NULL, &output) && output.length == n);
    mulib_tui_invalidate(&ui);
    assert(mulib_tui_render(&ui, NULL, &output) && output.length > n);
    assert(mulib_vterm_feed(&replay, output.data + n, output.length - n));
    terminal_screens_equal(&ui.back, &replay);
    assert(mulib_tui_resize(&ui, 7, 4));
    assert(ui.invalid && ui.cursor_x == 2 && ui.cursor_y == 3);
    assert(mulib_tui_cell(&ui, 1, 1)->codepoint == '+');
    assert(!mulib_tui_resize(&ui, 0, 1) && ui.back.grid.width == 7);
    {
        mulib_tui_rect_t huge = {6, 3, SIZE_MAX, SIZE_MAX};
        assert(mulib_tui_box(&ui, huge, mulib_term_style_default()));
        assert(mulib_tui_cell(&ui, 6, 3)->codepoint == '+');
    }
    for (i = 0; i < 256; ++i) {
        c.codepoint = (uint32_t)'x'; c.style = mulib_term_style_default(); c.style.foreground = (uint32_t)i;
        assert(mulib_tui_put(&ui, i % 7, (i / 7) % 4, c));
    }
    assert(mulib_vterm_resize(&replay, 7, 4));
    {
        uint32_t random = 42;
        size_t frame;
        for (frame = 0; frame < 20; ++frame) {
            for (i = 0; i < 28; ++i) {
                random = random * 1664525u + 1013904223u;
                c.codepoint = (uint32_t)'!' + random % 94u;
                c.style.attributes = (unsigned)((random >> 16) & 255u);
                c.style.foreground = random & 255u;
                c.style.background = MULIB_TERM_RGB(random >> 16, random >> 8, random);
                assert(mulib_tui_put(&ui, i % 7, i / 7, c));
            }
            n = output.length;
            assert(mulib_tui_render(&ui, NULL, &output));
            assert(mulib_vterm_feed(&replay, output.data + n, output.length - n));
            terminal_screens_equal(&ui.back, &replay);
        }
    }
    mulib_tui_free(&ui); mulib_tui_free(&ui);
    mulib_vterm_free(&replay);
    mulib_strbuf_free(&output); mulib_terminfo_free(&empty);
}

int main(void) {
    test_terminal_terminfo();
    test_terminal_termkey();
    test_terminal_vterm();
    test_terminal_vterm_margins();
    test_terminal_tui_color_reset();
    test_terminal_tui();
    return 0;
}
