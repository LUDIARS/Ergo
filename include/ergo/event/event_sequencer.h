#pragma once

#include "ergo/event/types.h"
#include "ergo/event/event_queue.h"
#include <unordered_map>
#include <vector>
#include <mutex>

namespace ergo::event {

/// イベントシーケンサー
/// イベントのキューイング・リスナー管理・フレーム単位ディスパッチを行う
class EventSequencer {
public:
    explicit EventSequencer(const SequencerConfig& config = {});

    // ----- イベント発行 -----

    /// イベントをキューに追加する
    /// @return キューに追加できた場合true
    bool emit(EventId id, std::any payload = {}, Priority priority = Priority::Normal);

    /// 遅延イベントを発行する（指定フレーム後にキューに追加）
    /// @return 遅延キューに追加できた場合true
    bool emitDelayed(EventId id, uint32_t delayFrames, std::any payload = {},
                     Priority priority = Priority::Normal);

    // ----- リスナー管理 -----

    /// リスナーを登録する
    /// @return リスナーID（解除に使用）
    ListenerId on(EventId eventId, EventCallback callback);

    /// リスナーを解除する
    /// @return 解除に成功した場合true
    bool off(ListenerId listenerId);

    /// 指定イベントIDの全リスナーを解除する
    void offAll(EventId eventId);

    // ----- フレーム処理 -----

    /// 1フレーム分のイベントを処理する
    /// @return 処理したイベント数
    uint32_t update();

    // ----- 状態 -----

    /// キュー内のイベント数
    std::size_t pendingCount() const;

    /// 遅延キュー内のイベント数
    std::size_t delayedCount() const;

    /// キューをクリア（遅延キューも含む）
    void clear();

    /// 設定を取得
    const SequencerConfig& config() const;

private:
    SequencerConfig config_;
    EventQueue queue_;
    std::vector<Event> delayedEvents_;
    std::unordered_map<EventId, std::vector<Listener>> listeners_;
    ListenerId nextListenerId_ = 1;
    bool dispatching_ = false;
    std::vector<Event> deferredEmits_;

    void dispatch(const Event& event);
    void processDelayedEvents();
};

} // namespace ergo::event
