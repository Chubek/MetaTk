/* Unit tests for the carbine tooling in tooling/. Run through ctest. The
 * files are written to the current directory (the build tree under ctest). */
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int failures = 0;

#define CHECK(cond, ...)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);      \
            fprintf(stderr, __VA_ARGS__);                             \
            fputc('\n', stderr);                                      \
            failures++;                                               \
        }                                                             \
    } while (0)

typedef cb_status (*fmt_fn)(cb_buf *, const char *);

static void expect_fmt(fmt_fn fmt, const char *name, const char *in, const char *want) {
    cb_buf b;
    cb_status st;
    cb_buf_init(&b);
    st = fmt(&b, in);
    CHECK(st == CB_OK, "%s(\"%s\") returned %s", name, in, cb_status_text(st));
    CHECK(b.data && strcmp(b.data, want) == 0, "%s(\"%s\") = \"%s\", want \"%s\"",
          name, in, b.data ? b.data : "", want);
    cb_buf_free(&b);
}

static void write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL, "cannot write %s", path);
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void test_quoting(void) {
    /* Shell: a word with a space is single-quoted; an embedded quote is
     * closed, escaped and reopened. */
    expect_fmt(cb_fmt_shell, "cb_fmt_shell", "a b", "'a b'");
    expect_fmt(cb_fmt_shell, "cb_fmt_shell", "it's", "'it'\\''s'");
    /* Make recipes and ninja variables: $ is doubled. */
    expect_fmt(cb_fmt_make, "cb_fmt_make", "echo $HOME", "echo $$HOME");
    expect_fmt(cb_fmt_ninja, "cb_fmt_ninja", "$x", "$$x");
}

static void test_lint(void) {
    size_t issues = 0;
    write_file("unit_lint_bad.txt", "x \n\ty\n");
    CHECK(cb_lint_file("unit_lint_bad.txt", &issues) == CB_OK, "lint of bad file failed");
    CHECK(issues >= 2, "lint found %zu issues in a file with trailing space and a tab, want >= 2", issues);

    issues = 99;
    write_file("unit_lint_ok.txt", "ok\n");
    CHECK(cb_lint_file("unit_lint_ok.txt", &issues) == CB_OK, "lint of clean file failed");
    CHECK(issues == 0, "lint found %zu issues in a clean file", issues);
}

static void test_url(void) {
    cb_url u;
    CHECK(cb_url_parse("http://example.com:8080/a/b", &u) == CB_OK, "parse of http URL failed");
    CHECK(strcmp(u.scheme, "http") == 0, "scheme \"%s\"", u.scheme);
    CHECK(strcmp(u.host, "example.com") == 0, "host \"%s\"", u.host);
    CHECK(strcmp(u.port, "8080") == 0, "port \"%s\"", u.port);
    CHECK(cb_url_parse("ftp://host.example/pub/x.tar", &u) == CB_OK, "parse of ftp URL failed");
    CHECK(strcmp(u.scheme, "ftp") == 0 && strcmp(u.host, "host.example") == 0,
          "ftp URL parsed as scheme \"%s\" host \"%s\"", u.scheme, u.host);
}

static void test_archive(void) {
    cb_list files;
    cb_status st;
    int rc;
    mkdir("unit_root", 0777);
    mkdir("unit_root/d", 0777);
    write_file("unit_root/d/f.txt", "hello\n");
    cb_list_init(&files);
    st = cb_list_push(&files, "d/f.txt");
    CHECK(st == CB_OK, "cb_list_push failed");
    st = cb_archive_tar("unit.tar", "unit_root", &files);
    CHECK(st == CB_OK, "cb_archive_tar returned %s", cb_status_text(st));
    cb_list_free(&files);
    /* The system tar must list the member under its relative name. */
    rc = system("tar -tf unit.tar | grep -qx 'd/f.txt'");
    CHECK(rc == 0, "tar -tf does not list d/f.txt");
}

int main(void) {
    test_quoting();
    test_lint();
    test_url();
    test_archive();
    if (failures) {
        fprintf(stderr, "unit_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("unit_test: all passed\n");
    return 0;
}
