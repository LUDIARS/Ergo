#include "ergo/event/event_sequencer.h"
#include <algorithm>

namespace ergo::event {

EventSequencer::EventSequencer(const SequencerConfig& config)
    : config_(config)
    , queue_(config.maxQueueSize)
{
}

bool EventSequencer::emit(EventId id, std::any payload, Priority priority) {
    Event event;
    event.id = id;
    event.payload = std::move(payload);
    event.priority = priority;
    event.delayFrames = 0;

    // 処理中に発行されたイベントは遅延キューに退避
    if (dispatching_) {
        deferredEmits_.push_back(std::move(event));
        return true;
    }

    return queue_.push(event);
}

bool EventSequencer::emitDelayed(EventId id, uint32_t delayFrames,
                                  std::any payload, Priority priority) {
    if (delayFrames == 0) {
        return emit(id, std::move(payload), priority);
    }

    Event event;
    event.id = id;
    event.payload = std::move(payload);
    event.priority = priority;
    event.delayFrames = delayFrames;

    delayedEvents_.push_back(std::move(event));
    return true;
}

ListenerId EventSequencer::on(EventId eventId, EventCallback callback) {
    ListenerId lid = nextListenerId_++;

    Listener listener;
    listener.listenerId = lid;
    listener.eventId = eventId;
    listener.callback = std::move(callback);

    listeners_[eventId].push_back(std::move(listener));
    return lid;
}

bool EventSequencer::off(ListenerId listenerId) {
    for (auto& [eventId, list] : listeners_) {
        auto it = std::find_if(list.begin(), list.end(),
            [listenerId](const Listener& l) {
                return l.listenerId == listenerId;
            });
        if (it != list.end()) {
            list.erase(it);
            return true;
        }
    }
    return false;
}

void EventSequencer::offAll(EventId eventId) {
    listeners_.erase(eventId);
}

uint32_t EventSequencer::update() {
    // 遅延イベントを処理
    processDelayedEvents();

    uint32_t processed = 0;
    Event event;

    dispatching_ = true;
    while (processed < config_.maxProcessPerFrame && queue_.pop(event)) {
        dispatch(event);
        ++processed;
    }
    dispatching_ = false;

    // 処理中に発行されたイベントをキューに戻す
    for (auto& deferred : deferredEmits_) {
        queue_.push(deferred);
    }
    deferredEmits_.clear();

    return processed;
}

std::size_t EventSequencer::pendingCount() const {
    return queue_.size();
}

std::size_t EventSequencer::delayedCount() const {
    return delayedEvents_.size();
}

void EventSequencer::clear() {
    queue_.clear();
    delayedEvents_.clear();
    deferredEmits_.clear();
}

const SequencerConfig& EventSequencer::config() const {
    return config_;
}

void EventSequencer::dispatch(const Event& event) {
    auto it = listeners_.find(event.id);
    if (it == listeners_.end()) {
        return;
    }
    for (const auto& listener : it->second) {
        if (listener.callback) {
            listener.callback(event);
        }
    }
}

void EventSequencer::processDelayedEvents() {
    auto it = delayedEvents_.begin();
    while (it != delayedEvents_.end()) {
        if (it->delayFrames <= 1) {
            Event event = std::move(*it);
            event.delayFrames = 0;
            queue_.push(event);
            it = delayedEvents_.erase(it);
        } else {
            --(it->delayFrames);
            ++it;
        }
    }
}

} // namespace ergo::event
