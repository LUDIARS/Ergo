#ifndef ERGO_STUDIO_PLUGIN_ABI_H
#define ERGO_STUDIO_PLUGIN_ABI_H
/* C ABI of external Ergo Studio plugin packs, version 1.
 *
 * A pack is a directory named in ERGO_PLUGIN_DIR. Every immediate
 * subdirectory holding ERGO_STUDIO_PLUGIN_FILE is one plugin. The library
 * exports ERGO_STUDIO_PLUGIN_ENTRY:
 *
 *   const ergo_studio_plugin_v1* ergo_studio_plugin_v1_create(const ergo_studio_host_v1* host);
 *
 * Rules
 *  - Strings are (data, len) pairs and are not NUL-terminated, except the
 *    info fields (id / title / description / icon), which are NUL-terminated
 *    and must stay valid until destroy().
 *  - Memory crosses the boundary only by borrowing: the host's strings are
 *    valid for the duration of the call; a response the plugin fills in stays
 *    valid until the host calls release_response(). Neither side frees the
 *    other's memory.
 *  - Callbacks may run on any host thread, concurrently. The plugin guards
 *    its own state. A callback must not throw across the boundary.
 *  - Any callback pointer may be NULL: the plugin lacks that capability.
 *  - The `host` table stays valid until destroy() returns.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ERGO_STUDIO_PLUGIN_ABI_VERSION 1u
#define ERGO_STUDIO_PLUGIN_ENTRY "ergo_studio_plugin_v1_create"

#if defined(_WIN32)
#  define ERGO_STUDIO_PLUGIN_FILE "ergo_studio_plugin.dll"
#  define ERGO_STUDIO_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__APPLE__)
#  define ERGO_STUDIO_PLUGIN_FILE "ergo_studio_plugin.dylib"
#  define ERGO_STUDIO_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#  define ERGO_STUDIO_PLUGIN_FILE "ergo_studio_plugin.so"
#  define ERGO_STUDIO_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

typedef struct ergo_studio_str_v1 {
    const char* data;
    size_t      len;
} ergo_studio_str_v1;

/* What the host offers the plugin. */
typedef struct ergo_studio_host_v1 {
    uint32_t abi_version;   /* ERGO_STUDIO_PLUGIN_ABI_VERSION */
    void*    ctx;           /* pass back unchanged */
    /* level: "info" | "warn" | "error" */
    void (*log)(void* ctx, const char* level, ergo_studio_str_v1 msg);
    /* Send to / close one WebSocket opened by ws_open. Unknown or closed ids are ignored. */
    void (*ws_send_text)(void* ctx, uint64_t conn, ergo_studio_str_v1 text);
    void (*ws_close)(void* ctx, uint64_t conn, uint16_t code);
} ergo_studio_host_v1;

typedef struct ergo_studio_http_request_v1 {
    ergo_studio_str_v1 method;
    ergo_studio_str_v1 sub_path;       /* path without the "/<id>" prefix, at least "/" */
    ergo_studio_str_v1 query;          /* raw query string without '?', may be empty */
    ergo_studio_str_v1 content_type;
    ergo_studio_str_v1 body;
} ergo_studio_http_request_v1;

typedef struct ergo_studio_http_response_v1 {
    int                status;
    ergo_studio_str_v1 content_type;   /* empty = application/json */
    ergo_studio_str_v1 body;
    void*              opaque;         /* for the plugin's release_response */
} ergo_studio_http_response_v1;

typedef struct ergo_studio_plugin_v1 {
    uint32_t    abi_version;           /* ERGO_STUDIO_PLUGIN_ABI_VERSION */
    void*       self;                  /* pass back unchanged */
    const char* id;
    const char* title;
    const char* description;           /* may be NULL */
    const char* icon;                  /* may be NULL */

    /* HTTP under /<id>/. Return 0 after filling `out`, non-zero for 500. */
    int  (*http)(void* self, const ergo_studio_http_request_v1* req, ergo_studio_http_response_v1* out);
    void (*release_response)(void* self, ergo_studio_http_response_v1* res);

    /* WebSocket under /<id>/ws. ws_open returning non-zero refuses the connection. */
    int  (*ws_open)(void* self, uint64_t conn);
    void (*ws_text)(void* self, uint64_t conn, ergo_studio_str_v1 text);
    void (*ws_closed)(void* self, uint64_t conn);

    /* Health as a JSON object with a boolean "ok". Same release rule as http. */
    int  (*health)(void* self, ergo_studio_http_response_v1* out);

    void (*destroy)(void* self);
} ergo_studio_plugin_v1;

typedef const ergo_studio_plugin_v1* (*ergo_studio_plugin_v1_create_fn)(const ergo_studio_host_v1* host);

#ifdef __cplusplus
}
#endif

#endif /* ERGO_STUDIO_PLUGIN_ABI_H */
