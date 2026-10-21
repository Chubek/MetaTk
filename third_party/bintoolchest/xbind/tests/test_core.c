#undef NDEBUG
#include "xbind.h"

#include <assert.h>
#include <string.h>

int main(int argc, char** argv) {
  assert(argc == 2);
  assert(strcmp(xbind_version(), XBIND_VERSION) == 0);
  assert(xbind_target_count() == 9);
  assert(xbind_target_at(9) == NULL);
  assert(xbind_find_target(NULL) == NULL);
  assert(xbind_find_target("missing") == NULL);
  assert(xbind_find_target("node") == xbind_find_target("NodeJS"));
  assert(strcmp(xbind_status_string((xbind_status)999), "unknown status") == 0);
  char path[4096];
  assert(xbind_resolve_spec(argv[1], "hello", path, sizeof(path)) == XBIND_STATUS_OK);
  assert(strstr(path, "hello.xbind") != NULL);
  assert(xbind_resolve_spec(argv[1], "hello.xbind", path, sizeof(path)) == XBIND_STATUS_OK);
  assert(xbind_resolve_spec(argv[1], "nonexistent", path, sizeof(path)) == XBIND_STATUS_NOT_FOUND);
  assert(xbind_resolve_spec(argv[1], "hello", path, 1) == XBIND_STATUS_BUFFER_TOO_SMALL);
  assert(xbind_resolve_spec(argv[1], NULL, path, sizeof(path)) == XBIND_STATUS_INVALID_ARGUMENT);
  assert(xbind_resolve_spec(argv[1], "hello", NULL, 0) == XBIND_STATUS_INVALID_ARGUMENT);
  return 0;
}
