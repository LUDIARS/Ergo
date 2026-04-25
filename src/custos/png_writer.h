#pragma once
//
// 最小 PNG エンコーダ。stb_image_write を入れずに RGBA8 → PNG を作る。
// zlib STORED block (圧縮なし) で deflate するので圧縮率は 0 だが、
// **追加依存ゼロ**で正規 PNG が出る。Custos が「とりあえず 1 枚撮る」
// snapshot 用途には十分。

#include <cstdint>
#include <vector>

namespace ergo::custos::detail {

/// `rgba` は row-major, top-to-bottom, tightly packed 4 byte/pixel。
/// 出力 PNG (RGBA8) のバイト列を返す。失敗時は空ベクタ。
std::vector<std::uint8_t> encode_png_rgba8(
    const std::uint8_t* rgba,
    std::uint32_t       width,
    std::uint32_t       height);

} // namespace ergo::custos::detail
