// libFuzzer entry: the Studio server's HTTP request parser and WebSocket frame
// decoder over the same bytes (first byte picks the split points of the feed).
#include "ergo/studio/net/http_parser.h"
#include "ergo/studio/net/ws_frame.h"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace ergo::studio::net;
    const char* bytes = reinterpret_cast<const char*>(data);
    const std::size_t step = size == 0 ? 1 : 1 + data[0] % 64;

    HttpRequestParser http(HttpLimits{4096, 4096, 64});
    for (std::size_t off = 0; off < size; off += step) {
        const std::size_t n = size - off < step ? size - off : step;
        if (http.feed(bytes + off, n) != ParseState::NeedMore) break;
    }
    (void)http.take_leftover();

    WsFrameDecoder ws(4096);
    WsMessage msg;
    for (std::size_t off = 0; off < size; off += step) {
        const std::size_t n = size - off < step ? size - off : step;
        ws.append(bytes + off, n);
        for (;;) {
            const auto r = ws.next(msg);
            if (r != WsDecodeResult::Message) {
                if (r != WsDecodeResult::NeedMore) return 0;
                break;
            }
        }
    }
    return 0;
}
