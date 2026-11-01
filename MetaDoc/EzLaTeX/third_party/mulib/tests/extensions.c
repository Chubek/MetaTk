/* Real cross-group composition through tests/implementation.c, not local
 * implementation switches: memory VFS -> file server -> router -> HTTP wire
 * -> webclient/download -> MicroJS. No sockets or public services are needed.
 */
#include "mulib-vfs.h"
#include "mulib-fileserver.h"
#include "mulib-miniwget.h"
#include "mulib-microjs.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static mulib_fileserver_result memory_load(void *user, const char *relative,
    size_t maximum, int metadata_only, unsigned char **data, size_t *length) {
    mulib_vfs *vfs = (mulib_vfs *)user;
    mulib_vfs_stat info;
    char path[MULIB_VFS_PATH_CAP];
    size_t n = strlen(relative), count = 0;
    mulib_vfs_result rc;
    *data = NULL; *length = 0;
    if (n > sizeof(path) - 2) return MULIB_FILESERVER_LIMIT;
    path[0] = '/'; memcpy(path + 1, relative, n + 1);
    rc = mulib_vfs_getstat(vfs, path, &info);
    if (rc == MULIB_VFS_ENOENT) return MULIB_FILESERVER_NOT_FOUND;
    if (rc != MULIB_VFS_OK) return MULIB_FILESERVER_IO;
    if (info.kind != MULIB_VFS_FILE) return MULIB_FILESERVER_FORBIDDEN;
    if (info.size > maximum || info.size > SIZE_MAX) return MULIB_FILESERVER_LIMIT;
    *length = (size_t)info.size;
    if (metadata_only) return MULIB_FILESERVER_FOUND;
    *data = (unsigned char *)malloc(*length ? *length : 1);
    if (!*data) return MULIB_FILESERVER_NOMEM;
    rc = mulib_vfs_read(vfs, path, 0, *data, *length, &count);
    return rc == MULIB_VFS_OK && count == *length ? MULIB_FILESERVER_FOUND : MULIB_FILESERVER_IO;
}

static mulib_http_result serve_memory(void *vfs, const mulib_http_message_t *request,
    mulib_http_message_t *response) {
    return mulib_fileserver_serve(request, NULL, memory_load, vfs, response);
}

static int local_transport(void *user, const mulib_uri_t *endpoint,
    const void *wire, size_t length, mulib_webclient_receive_fn receive, void *receiver) {
    mulib_webframework_t *router = (mulib_webframework_t *)user;
    mulib_http_message_t request = {0}, response = {0};
    mulib_http_options_t options = {0, 0, 0, MULIB_HTTP_EOF};
    unsigned char *reply = NULL;
    size_t consumed = 0, reply_length = 0;
    int ok = 0;
    assert(endpoint && endpoint->host.length == strlen("memory.local"));
    assert(!memcmp(endpoint->host.data, "memory.local", endpoint->host.length));
    if (mulib_http_parse(wire, length, 0, &options, &request, &consumed) != MULIB_HTTP_OK ||
        consumed != length) goto done;
    if (mulib_webframework_dispatch(router, &request, &response) != MULIB_HTTP_OK) goto done;
    if (!strcmp(request.method, "HEAD")) options.flags |= MULIB_HTTP_HEAD_RESPONSE;
    if (mulib_http_serialize(&response, &options, &reply, &reply_length) != MULIB_HTTP_OK) goto done;
    for (size_t offset = 0; offset < reply_length;) {
        size_t chunk = reply_length - offset;
        if (chunk > 7) chunk = 7;
        if (!receive(receiver, reply + offset, chunk)) goto done;
        offset += chunk;
    }
    ok = 1;
done:
    free(reply);
    mulib_http_message_free(&response);
    mulib_http_message_free(&request);
    return ok;
}

int main(void) {
    const unsigned char binary[] = {'x', 0, 'y', 255};
    const char source[] = "let answer = 6 * 7; answer";
    unsigned char downloaded[64];
    size_t length = 999;
    mulib_vfs_mem storage;
    mulib_vfs vfs;
    mulib_vfs_ops backend = mulib_vfs_mem_ops();
    mulib_webframework_t router;
    mulib_http_message_t response = {0};
    mulib_microjs_t *interpreter;
    mulib_microjs_value_t result = {0};
    mulib_microjs_error_t error = {0};

    assert(mulib_vfs_mem_init(&storage, 4, 1024) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_put(&storage, "/index.bin", binary, sizeof(binary)) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_put(&storage, "/calc.js", source, sizeof(source) - 1) == MULIB_VFS_OK);
    assert(mulib_vfs_init(&vfs, 1) == MULIB_VFS_OK);
    assert(mulib_vfs_mount(&vfs, "/", &backend, &storage) == MULIB_VFS_OK);
    mulib_webframework_init(&router, 2);
    assert(mulib_webframework_route(&router, "GET", "/index.bin", serve_memory, &vfs) == MULIB_HTTP_OK);
    assert(mulib_webframework_route(&router, "GET", "/calc.js", serve_memory, &vfs) == MULIB_HTTP_OK);

    assert(mulib_webclient_request("http://memory.local/index.bin", "GET", NULL, 0,
        NULL, 0, NULL, local_transport, &router, &response) == MULIB_WEBCLIENT_OK);
    assert(response.status == 200 && response.body_length == sizeof(binary));
    assert(!memcmp(response.body, binary, sizeof(binary)));
    mulib_http_message_free(&response);

    assert(mulib_webclient_request("http://memory.local/index.bin", "HEAD", NULL, 0,
        NULL, 0, NULL, local_transport, &router, &response) == MULIB_WEBCLIENT_OK);
    assert(response.status == 200 && response.body_length == 0);
    assert(!strcmp(mulib_http_header(&response, "Content-Length"), "4"));
    mulib_http_message_free(&response);

    assert(mulib_miniwget_download("http://memory.local/index.bin?q=1", NULL,
        local_transport, &router, downloaded, sizeof(binary), &length) == MULIB_MINIWGET_OK);
    assert(length == sizeof(binary) && !memcmp(downloaded, binary, length));
    memset(downloaded, 0x55, sizeof(downloaded)); length = 999;
    assert(mulib_miniwget_download("http://memory.local/index.bin", NULL,
        local_transport, &router, downloaded, sizeof(binary) - 1, &length) == MULIB_MINIWGET_LIMIT);
    assert(length == 999 && downloaded[0] == 0x55);

    assert(mulib_miniwget_download("http://memory.local/calc.js", NULL,
        local_transport, &router, downloaded, sizeof(downloaded), &length) == MULIB_MINIWGET_OK);
    interpreter = mulib_microjs_create(NULL);
    assert(interpreter);
    assert(mulib_microjs_eval(interpreter, (const char *)downloaded, length, &result, &error) == MULIB_MICROJS_OK);
    assert(result.type == MULIB_MICROJS_NUMBER && result.number == 42.0);
    mulib_microjs_value_free(&result);
    mulib_microjs_destroy(interpreter);

    assert(mulib_webclient_request("http://memory.local/missing", "GET", NULL, 0,
        NULL, 0, NULL, local_transport, &router, &response) == MULIB_WEBCLIENT_OK);
    assert(response.status == 404);
    mulib_http_message_free(&response);
    mulib_webframework_free(&router);
    mulib_vfs_free(&vfs);
    mulib_vfs_mem_free(&storage);
    return 0;
}
