#include "ergo/event/event_queue.h"
#include "ergo/event/event_sequencer.h"

// ダミープラグ: コンパイル短縮用の空実装
// リンク時にこのオブジェクトを使用することで、
// 実装が未完了でもコンパイルを通すことができる

namespace ergo::event {
namespace dummy {
    static EventSequencer* instance = nullptr;
}
} // namespace ergo::event
