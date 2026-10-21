#include "xbind.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef XBIND_SOURCE_DIR
#define XBIND_SOURCE_DIR "."
#endif

static const xbind_target_info xbind_targets[] = {
    {"D", "compilers/D.c", "dombind/domdbind", "typedefs/D-Default.sexpr",
     "D extern(C) buffers, callbacks, and exception containment"},
    {"Go", "compilers/Go.c", "dombind/domgobind", "typedefs/Go-Default.sexpr",
     "cgo buffers, callbacks, and panic containment"},
    {"Java", "compilers/Java.c", "dombind/domjavabind", "typedefs/Java-Default.sexpr",
     "JNI embedding and native calls"},
    {"Lua", "compilers/Lua.c", "dombind/domluabind", "typedefs/Lua-Default.sexpr",
     "Lua embedding and extension callbacks"},
    {"NodeJS", "compilers/NodeJS.c", "dombind/domnodebind",
     "typedefs/NodeJs-Default.sexpr", "Node-API extensions"},
    {"OCaml", "compilers/OCaml.c", "dombind/domocamlbind",
     "typedefs/OCaml-Default.sexpr", "OCaml C interface and runtime callbacks"},
    {"Python", "compilers/Python.c", "dombind/dompybind",
     "typedefs/Python-Default.sexpr", "CPython embedding and extension modules"},
    {"Ruby", "compilers/Ruby.c", "dombind/domrubybind", "typedefs/Ruby-Default.sexpr",
     "CRuby embedding and extensions"},
    {"Rust", "compilers/Rust.c", "dombind/domrustbind", "typedefs/Rust-Default.sexpr",
     "Rust extern C buffers, callbacks, and panic containment"},
};

static int xbind_ascii_ieq(const char *left, const char *right) {
  unsigned char a;
  unsigned char b;
  if (!left || !right) {
    return 0;
  }
  while (*left && *right) {
    a = (unsigned char)*left++;
    b = (unsigned char)*right++;
    if (a >= 'A' && a <= 'Z') {
      a = (unsigned char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = (unsigned char)(b - 'A' + 'a');
    }
    if (a != b) {
      return 0;
    }
  }
  return *left == '\0' && *right == '\0';
}

static int xbind_has_suffix(const char *text, const char *suffix) {
  size_t text_len;
  size_t suffix_len;
  if (!text || !suffix) {
    return 0;
  }
  text_len = strlen(text);
  suffix_len = strlen(suffix);
  return text_len >= suffix_len &&
         strcmp(text + text_len - suffix_len, suffix) == 0;
}

static xbind_status xbind_join_path(char *output,
                                    size_t output_size,
                                    const char *left,
                                    const char *right) {
  size_t left_len;
  size_t right_len;
  size_t needs_sep;
  if (!output || output_size == 0 || !left || !right) {
    return XBIND_STATUS_INVALID_ARGUMENT;
  }
  left_len = strlen(left);
  right_len = strlen(right);
  needs_sep = left_len > 0 && left[left_len - 1] != '/';
  if (left_len >= output_size || needs_sep > output_size - left_len - 1 ||
      right_len > output_size - left_len - needs_sep - 1) {
    return XBIND_STATUS_BUFFER_TOO_SMALL;
  }
  memcpy(output, left, left_len);
  if (needs_sep) {
    output[left_len] = '/';
  }
  memcpy(output + left_len + needs_sep, right, right_len + 1);
  return XBIND_STATUS_OK;
}

static int xbind_file_exists(const char *path) {
  struct stat info;
  if (!path || stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
    return 0;
  }
  FILE *file = fopen(path, "rb");
  if (!file) {
    return 0;
  }
  fclose(file);
  return 1;
}

static int xbind_directory_exists(const char *path) {
  struct stat info;
  return path && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static void xbind_emit(xbind_diagnostic_fn diagnostic,
                       void *user,
                       const char *format,
                       ...) {
  char message[1024];
  va_list args;
  if (!diagnostic) {
    return;
  }
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);
  diagnostic(user, message);
}

static size_t xbind_check_file(const char *root,
                               const char *relative,
                               xbind_diagnostic_fn diagnostic,
                               void *user) {
  char path[1024];
  if (xbind_join_path(path, sizeof(path), root, relative) != XBIND_STATUS_OK ||
      !xbind_file_exists(path)) {
    xbind_emit(diagnostic, user, "missing file: %s/%s", root, relative);
    return 1;
  }
  return 0;
}

static size_t xbind_check_directory(const char *root,
                                    const char *relative,
                                    xbind_diagnostic_fn diagnostic,
                                    void *user) {
  char path[1024];
  if (xbind_join_path(path, sizeof(path), root, relative) != XBIND_STATUS_OK ||
      !xbind_directory_exists(path)) {
    xbind_emit(diagnostic, user, "missing directory: %s/%s", root, relative);
    return 1;
  }
  return 0;
}

const char *xbind_version(void) { return XBIND_VERSION; }

const char *xbind_status_string(xbind_status status) {
  switch (status) {
  case XBIND_STATUS_OK:
    return "ok";
  case XBIND_STATUS_INVALID_ARGUMENT:
    return "invalid argument";
  case XBIND_STATUS_NOT_FOUND:
    return "not found";
  case XBIND_STATUS_BUFFER_TOO_SMALL:
    return "buffer too small";
  }
  return "unknown status";
}

size_t xbind_target_count(void) {
  return sizeof(xbind_targets) / sizeof(xbind_targets[0]);
}

const xbind_target_info *xbind_target_at(size_t index) {
  if (index >= xbind_target_count()) {
    return NULL;
  }
  return &xbind_targets[index];
}

const xbind_target_info *xbind_find_target(const char *name) {
  size_t index;
  if (!name) {
    return NULL;
  }
  for (index = 0; index < xbind_target_count(); ++index) {
    if (xbind_ascii_ieq(name, xbind_targets[index].name)) {
      return &xbind_targets[index];
    }
  }
  if (xbind_ascii_ieq(name, "Node") || xbind_ascii_ieq(name, "NodeJs")) {
    return xbind_find_target("NodeJS");
  }
  return NULL;
}

xbind_status xbind_resolve_spec(const char *directory,
                                const char *library_name,
                                char *output,
                                size_t output_size) {
  const char *root = directory && directory[0] ? directory : ".";
  char candidate[1024];
  xbind_status status;
  if (!library_name || !library_name[0] || !output || output_size == 0) {
    return XBIND_STATUS_INVALID_ARGUMENT;
  }
  if (xbind_has_suffix(library_name, ".xbind") ||
      xbind_has_suffix(library_name, ".lua")) {
    status = xbind_join_path(candidate, sizeof(candidate), root, library_name);
    if (status != XBIND_STATUS_OK) {
      return status;
    }
    if (xbind_file_exists(candidate)) {
      return xbind_join_path(output, output_size, root, library_name);
    }
    return XBIND_STATUS_NOT_FOUND;
  }

  status = xbind_join_path(candidate, sizeof(candidate), root, library_name);
  if (status != XBIND_STATUS_OK) {
    return status;
  }
  if (strlen(candidate) + strlen(".xbind") + 1 > sizeof(candidate)) {
    return XBIND_STATUS_BUFFER_TOO_SMALL;
  }
  strcat(candidate, ".xbind");
  if (xbind_file_exists(candidate)) {
    if (strlen(candidate) + 1 > output_size) {
      return XBIND_STATUS_BUFFER_TOO_SMALL;
    }
    strcpy(output, candidate);
    return XBIND_STATUS_OK;
  }

  status = xbind_join_path(candidate, sizeof(candidate), root, library_name);
  if (status != XBIND_STATUS_OK) {
    return status;
  }
  if (strlen(candidate) + strlen(".lua") + 1 > sizeof(candidate)) {
    return XBIND_STATUS_BUFFER_TOO_SMALL;
  }
  strcat(candidate, ".lua");
  if (xbind_file_exists(candidate)) {
    if (strlen(candidate) + 1 > output_size) {
      return XBIND_STATUS_BUFFER_TOO_SMALL;
    }
    strcpy(output, candidate);
    return XBIND_STATUS_OK;
  }
  return XBIND_STATUS_NOT_FOUND;
}

size_t xbind_check_layout(const char *root,
                          xbind_diagnostic_fn diagnostic,
                          void *user) {
  const char *base = root && root[0] ? root : XBIND_SOURCE_DIR;
  size_t errors = 0;
  size_t index;
  errors += xbind_check_file(base, "XFeats.schema.json", diagnostic, user);
  errors += xbind_check_file(base, "xbind.h", diagnostic, user);
  errors += xbind_check_file(base, "xbind.hpp", diagnostic, user);
  errors += xbind_check_file(base, "lxbind/lxbind.lua", diagnostic, user);
  errors += xbind_check_file(base, "dombind/README.md", diagnostic, user);
  errors += xbind_check_directory(base, "stdlib", diagnostic, user);
  errors += xbind_check_directory(base, "typedefs", diagnostic, user);
  errors += xbind_check_directory(base, "compilers", diagnostic, user);
  for (index = 0; index < xbind_target_count(); ++index) {
    const xbind_target_info *target = &xbind_targets[index];
    errors += xbind_check_directory(base, target->compiler_directory, diagnostic, user);
    errors += xbind_check_directory(base, target->dombind_directory, diagnostic, user);
    errors += xbind_check_file(base, target->typedef_file, diagnostic, user);
  }
  return errors;
}

#ifdef XBIND_STANDALONE
static void xbind_cli_diagnostic(void *user, const char *message) {
  FILE *stream = (FILE *)user;
  fprintf(stream ? stream : stderr, "%s\n", message);
}

static void xbind_print_usage(FILE *stream) {
  fprintf(stream,
          "usage: xbind <command> [args]\n"
          "\n"
          "commands:\n"
          "  --help                         Show this help.\n"
          "  --version                      Print the scaffold version.\n"
          "  --list-targets                 List supported output targets.\n"
          "  --target NAME                  Show one target's scaffold paths.\n"
          "  --show-paths [ROOT]            Show key XBind source directories.\n"
          "  --check-layout [ROOT]          Validate the scaffold layout.\n"
          "  --find-spec LIB [DIR]          Resolve LIB.xbind or LIB.lua.\n");
}

static void xbind_print_target(const xbind_target_info *target) {
  printf("%s\n", target->name);
  printf("  compiler: %s\n", target->compiler_directory);
  printf("  dombind:  %s\n", target->dombind_directory);
  printf("  typedefs: %s\n", target->typedef_file);
  printf("  native:   %s\n", target->native_boundary);
}

int main(int argc, char **argv) {
  size_t index;
  if (argc <= 1 || strcmp(argv[1], "--help") == 0) {
    xbind_print_usage(stdout);
    return 0;
  }
  if (strcmp(argv[1], "--version") == 0) {
    printf("%s\n", xbind_version());
    return 0;
  }
  if (strcmp(argv[1], "--list-targets") == 0) {
    for (index = 0; index < xbind_target_count(); ++index) {
      puts(xbind_target_at(index)->name);
    }
    return 0;
  }
  if (strcmp(argv[1], "--target") == 0) {
    const xbind_target_info *target;
    if (argc < 3) {
      fprintf(stderr, "xbind: --target requires a target name\n");
      return 2;
    }
    target = xbind_find_target(argv[2]);
    if (!target) {
      fprintf(stderr, "xbind: unknown target: %s\n", argv[2]);
      return 1;
    }
    xbind_print_target(target);
    return 0;
  }
  if (strcmp(argv[1], "--show-paths") == 0) {
    const char *root = argc >= 3 ? argv[2] : XBIND_SOURCE_DIR;
    printf("root:     %s\n", root);
    printf("lxbind:   %s/lxbind\n", root);
    printf("stdlib:   %s/stdlib\n", root);
    printf("typedefs: %s/typedefs\n", root);
    printf("dombind:  %s/dombind\n", root);
    return 0;
  }
  if (strcmp(argv[1], "--check-layout") == 0) {
    const char *root = argc >= 3 ? argv[2] : XBIND_SOURCE_DIR;
    size_t errors = xbind_check_layout(root, xbind_cli_diagnostic, stderr);
    if (errors == 0) {
      printf("XBind scaffold layout OK: %s\n", root);
      return 0;
    }
    fprintf(stderr, "xbind: %lu layout issue(s)\n", (unsigned long)errors);
    return 1;
  }
  if (strcmp(argv[1], "--find-spec") == 0) {
    char resolved[1024];
    xbind_status status;
    const char *directory;
    if (argc < 3) {
      fprintf(stderr, "xbind: --find-spec requires a library name\n");
      return 2;
    }
    directory = argc >= 4 ? argv[3] : ".";
    status = xbind_resolve_spec(directory, argv[2], resolved, sizeof(resolved));
    if (status != XBIND_STATUS_OK) {
      fprintf(stderr, "xbind: %s: %s\n", argv[2], xbind_status_string(status));
      return status == XBIND_STATUS_NOT_FOUND ? 1 : 2;
    }
    puts(resolved);
    return 0;
  }
  fprintf(stderr, "xbind: unknown command: %s\n", argv[1]);
  xbind_print_usage(stderr);
  return 2;
}
#endif
