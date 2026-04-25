#include "png_writer.h"

#include <cstring>

// 最小 PNG エンコーダ — zlib STORED block 経由で圧縮なし PNG を作る。
//
// PNG 構造:
//   [8 byte signature]
//   [IHDR chunk] : 4 byte len + "IHDR" + 13 byte data + CRC32
//   [IDAT chunk] : zlib STORED stream (filtered scanlines)
//   [IEND chunk]
//
// zlib STORED:
//   [2 byte header: 0x78 0x01]
//   for each block:
//     [1 byte BFINAL+BTYPE]
//     [2 byte LEN little-endian]
//     [2 byte NLEN little-endian (~LEN)]
//     [LEN bytes raw data]
//   [4 byte Adler32 of uncompressed data, big-endian]
//
// 各 PNG scanline 先頭に **filter byte (0 = None)** を 1 byte 入れる。

namespace ergo::custos::detail {

namespace {

// ─── CRC32 (PNG) ────────────────────────────

std::uint32_t crc_table[256];
bool          crc_table_ready = false;

void make_crc_table() {
    if (crc_table_ready) return;
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
        crc_table[n] = c;
    }
    crc_table_ready = true;
}

std::uint32_t crc32(const std::uint8_t* buf, std::size_t len) {
    make_crc_table();
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

// ─── Adler32 (zlib) ─────────────────────────

std::uint32_t adler32(const std::uint8_t* buf, std::size_t len) {
    constexpr std::uint32_t MOD = 65521;
    std::uint32_t a = 1, b = 0;
    for (std::size_t i = 0; i < len; ++i) {
        a = (a + buf[i]) % MOD;
        b = (b + a)      % MOD;
    }
    return (b << 16) | a;
}

// ─── helpers: big-endian write ──────────────

void put_u32_be(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(std::uint8_t(v >> 24));
    out.push_back(std::uint8_t(v >> 16));
    out.push_back(std::uint8_t(v >> 8));
    out.push_back(std::uint8_t(v));
}
void put_u16_le(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(std::uint8_t(v));
    out.push_back(std::uint8_t(v >> 8));
}

void write_chunk(std::vector<std::uint8_t>& out,
                 const char type[4],
                 const std::uint8_t* data,
                 std::uint32_t len)
{
    put_u32_be(out, len);
    std::size_t crc_start = out.size();
    out.insert(out.end(), reinterpret_cast<const std::uint8_t*>(type),
               reinterpret_cast<const std::uint8_t*>(type) + 4);
    if (data && len) out.insert(out.end(), data, data + len);
    std::uint32_t c = crc32(out.data() + crc_start, 4 + len);
    put_u32_be(out, c);
}

// ─── zlib STORED stream ─────────────────────

std::vector<std::uint8_t> zlib_stored(const std::uint8_t* data, std::size_t len) {
    std::vector<std::uint8_t> out;
    // CMF=0x78 (deflate, 32K window) FLG=0x01 (FCHECK で 0x7801 が 31 倍数になる)
    out.push_back(0x78);
    out.push_back(0x01);

    std::size_t off = 0;
    while (off < len || len == 0) {
        std::size_t   chunk = std::min<std::size_t>(len - off, 0xFFFF);
        std::uint8_t  bfinal = (off + chunk == len) ? 1 : 0;
        out.push_back(bfinal);          // BFINAL=1, BTYPE=00 (stored)
        std::uint16_t l = std::uint16_t(chunk);
        std::uint16_t n = std::uint16_t(~chunk);
        put_u16_le(out, l);
        put_u16_le(out, n);
        if (chunk) out.insert(out.end(), data + off, data + off + chunk);
        off += chunk;
        if (bfinal) break;
        if (len == 0) break;
    }

    std::uint32_t adler = adler32(data, len);
    put_u32_be(out, adler);
    return out;
}

} // anonymous

std::vector<std::uint8_t> encode_png_rgba8(
    const std::uint8_t* rgba,
    std::uint32_t       width,
    std::uint32_t       height)
{
    if (!rgba || width == 0 || height == 0) return {};

    std::vector<std::uint8_t> out;
    out.reserve(width * height * 4 + 1024);

    // PNG signature
    static const std::uint8_t sig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    out.insert(out.end(), sig, sig + 8);

    // IHDR
    {
        std::uint8_t ihdr[13];
        std::uint32_t w = width, h = height;
        ihdr[0]  = std::uint8_t(w >> 24);
        ihdr[1]  = std::uint8_t(w >> 16);
        ihdr[2]  = std::uint8_t(w >> 8);
        ihdr[3]  = std::uint8_t(w);
        ihdr[4]  = std::uint8_t(h >> 24);
        ihdr[5]  = std::uint8_t(h >> 16);
        ihdr[6]  = std::uint8_t(h >> 8);
        ihdr[7]  = std::uint8_t(h);
        ihdr[8]  = 8;     // bit depth
        ihdr[9]  = 6;     // color type: RGBA
        ihdr[10] = 0;     // compression: deflate
        ihdr[11] = 0;     // filter: none
        ihdr[12] = 0;     // interlace: none
        write_chunk(out, "IHDR", ihdr, 13);
    }

    // IDAT — filtered scanlines + zlib STORED
    {
        std::vector<std::uint8_t> raw;
        raw.reserve(std::size_t(height) * (1 + width * 4));
        const std::size_t row_bytes = std::size_t(width) * 4;
        for (std::uint32_t y = 0; y < height; ++y) {
            raw.push_back(0);   // filter: None
            const std::uint8_t* row = rgba + std::size_t(y) * row_bytes;
            raw.insert(raw.end(), row, row + row_bytes);
        }
        auto zlib = zlib_stored(raw.data(), raw.size());
        write_chunk(out, "IDAT", zlib.data(), std::uint32_t(zlib.size()));
    }

    // IEND
    write_chunk(out, "IEND", nullptr, 0);

    return out;
}

} // namespace ergo::custos::detail
