/* Test-only external pack written in plain C against plugin_abi.h:
 *   HTTP  : echoes "<method> <sub_path>?<query> <body>" as text/plain
 *   WS    : replies "echo:<text>" through the host
 *   health: {"ok":true,"clients":<open sockets>} */
#include "ergo/studio/plugin_abi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Appends `len` bytes at `*at` (the caller sized the buffer); the analyzer's
 * C11 bounds-checking rule flags memcpy / snprintf, so the pack copies itself. */
static void put(char** at, const char* data, size_t len) {
    for (size_t i = 0; i < len; ++i) (*at)[i] = data[i];
    *at += len;
}

/* Decimal text of `value` (at most 11 characters with the sign). */
static size_t put_int(char* out, int value) {
    char         digits[12];
    size_t       n        = 0;
    unsigned int magnitude = value < 0 ? 0u - (unsigned int)value : (unsigned int)value;
    do {
        digits[n++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u);
    size_t len = 0;
    if (value < 0) out[len++] = '-';
    while (n > 0) out[len++] = digits[--n];
    return len;
}

typedef struct {
    const ergo_studio_host_v1* host;
    ergo_studio_plugin_v1      table;
    int                        clients;   /* the test drives one thread at a time */
} echo_state;

static int echo_http(void* self, const ergo_studio_http_request_v1* req, ergo_studio_http_response_v1* out) {
    (void)self;
    size_t n   = req->method.len + req->sub_path.len + req->query.len + req->body.len + 3;
    char*  buf = (char*)malloc(n);
    if (!buf) return 1;
    char* at = buf;
    put(&at, req->method.data, req->method.len);
    put(&at, " ", 1);
    put(&at, req->sub_path.data, req->sub_path.len);
    put(&at, "?", 1);
    put(&at, req->query.data, req->query.len);
    put(&at, " ", 1);
    put(&at, req->body.data, req->body.len);
    size_t len = (size_t)(at - buf);
    out->status            = 200;
    out->content_type.data = "text/plain";
    out->content_type.len  = 10;
    out->body.data         = buf;
    out->body.len          = len;
    out->opaque            = buf;
    return 0;
}

static void echo_release(void* self, ergo_studio_http_response_v1* res) {
    (void)self;
    free(res->opaque);
}

static int echo_ws_open(void* self, uint64_t conn) {
    (void)conn;
    ((echo_state*)self)->clients++;
    return 0;
}

static void echo_ws_text(void* self, uint64_t conn, ergo_studio_str_v1 text) {
    echo_state* s   = (echo_state*)self;
    char*       buf = (char*)malloc(text.len + 5);
    if (!buf) return;
    char* at = buf;
    put(&at, "echo:", 5);
    put(&at, text.data, text.len);
    ergo_studio_str_v1 reply = {buf, text.len + 5};
    s->host->ws_send_text(s->host->ctx, conn, reply);
    free(buf);
}

static void echo_ws_closed(void* self, uint64_t conn) {
    (void)conn;
    ((echo_state*)self)->clients--;
}

static int echo_health(void* self, ergo_studio_http_response_v1* out) {
    static const char head[] = "{\"ok\":true,\"clients\":";
    char*             buf    = (char*)malloc(48);
    if (!buf) return 1;
    char* at = buf;
    put(&at, head, sizeof(head) - 1);
    at += put_int(at, ((echo_state*)self)->clients);
    put(&at, "}", 1);
    out->status    = 200;
    out->body.data = buf;
    out->body.len  = (size_t)(at - buf);
    out->opaque    = buf;
    return 0;
}

static void echo_destroy(void* self) { free(self); }

ERGO_STUDIO_PLUGIN_EXPORT const ergo_studio_plugin_v1* ergo_studio_plugin_v1_create(const ergo_studio_host_v1* host) {
    if (!host || host->abi_version != ERGO_STUDIO_PLUGIN_ABI_VERSION) return NULL;
    echo_state* s = (echo_state*)calloc(1, sizeof(echo_state));
    if (!s) return NULL;
    s->host                     = host;
    s->table.abi_version        = ERGO_STUDIO_PLUGIN_ABI_VERSION;
    s->table.self               = s;
    s->table.id                 = "echo";
    s->table.title              = "Echo";
    s->table.description        = "test pack";
    s->table.http               = echo_http;
    s->table.release_response   = echo_release;
    s->table.ws_open            = echo_ws_open;
    s->table.ws_text            = echo_ws_text;
    s->table.ws_closed          = echo_ws_closed;
    s->table.health             = echo_health;
    s->table.destroy            = echo_destroy;
    return &s->table;
}
