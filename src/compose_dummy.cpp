// ダミープラグ — コンパイル短縮用の空実装
// 他モジュールとの結合テスト時に、実装が不要な場合に使用する

#include "ergo/compose/compose_system.h"

namespace ergo::compose::dummy {

/// ダミー初期化 — 何もしない
inline void initialize() {
    ComposeSystem system;
    ComposeConfig config;
    system.initialize(config);
}

} // namespace ergo::compose::dummy
