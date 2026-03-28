#pragma once

#include "ergo/event/types.h"
#include <vector>
#include <cstddef>

namespace ergo::event {

/// 優先度付きイベントキュー
class EventQueue {
public:
    explicit EventQueue(uint32_t maxSize = 4096);

    /// イベントをキューに追加する。キュー満杯時はfalseを返す
    bool push(const Event& event);

    /// 最も優先度の高いイベントを取り出す
    /// キューが空の場合はfalseを返す
    bool pop(Event& out);

    /// キュー内のイベント数
    std::size_t size() const;

    /// キューが空か
    bool empty() const;

    /// キューをクリア
    void clear();

    /// キュー最大サイズ
    uint32_t maxSize() const;

private:
    uint32_t maxSize_;
    std::vector<Event> heap_;

    void heapUp(std::size_t index);
    void heapDown(std::size_t index);
};

} // namespace ergo::event
