// ダミープラグ — コンパイル短縮用の空実装
// 他モジュールとの結合テスト時に、実装が不要な場合に使用する

#include "ergo/sound/audio_engine.h"
#include "ergo/sound/quantizer.h"
#include "ergo/sound/waveform_processor.h"

namespace ergo::sound::dummy {

/// ダミー初期化 — 何もしない
inline void initialize() {
    AudioEngine engine;
    AudioEngineConfig config;
    engine.initialize(config);
    engine.shutdown();
}

} // namespace ergo::sound::dummy
