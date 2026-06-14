#pragma once

/// VariableRegistry — the live store of bound host variables.
///
/// Single responsibility: own the variable table (by handle / by name),
/// allocate handles, and detect getter-side value changes. Thread-safe via
/// its own mutex; it knows nothing about the wire protocol or the WS client.

#include "ergo/bind/types.h"

#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ergo::bind::detail {

class VariableRegistry {
public:
    struct Entry {
        Handle                            handle = INVALID_HANDLE;
        std::string                       name;
        VarKind                           kind = VarKind::Bool;
        VarMeta                           meta;
        std::function<Value()>            getter;
        std::function<void(const Value&)> setter;
        Value                             last_seen;
        bool                              published = false;
    };

    /// Register (or replace a same-named) variable. Returns the new handle
    /// and writes a copy of the stored entry to `out` for the initial bind.
    Handle insert(std::string name, VarKind kind,
                  std::function<Value()> getter,
                  std::function<void(const Value&)> setter,
                  VarMeta meta, Entry& out)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = by_name_.find(name);
        if (it != by_name_.end()) {
            std::fprintf(stderr,
                "[ergo::bind] WARN: re-binding existing variable \"%s\" — old handle dropped\n",
                name.c_str());
            by_handle_.erase(it->second);
            by_name_.erase(it);
        }
        Entry e{};
        e.handle = next_handle_++;
        if (next_handle_ == INVALID_HANDLE) next_handle_ = 1;
        e.name      = name;
        e.kind      = kind;
        e.meta      = std::move(meta);
        e.getter    = std::move(getter);
        e.setter    = std::move(setter);
        if (e.getter) e.last_seen = e.getter();
        e.published = false;
        by_name_[name] = e.handle;
        auto [pos, _] = by_handle_.emplace(e.handle, std::move(e));
        out = pos->second;
        return out.handle;
    }

    /// Mark a variable as published (its initial bind has been sent).
    void mark_published(Handle h) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = by_handle_.find(h);
        if (it != by_handle_.end()) it->second.published = true;
    }

    /// Erase a variable. Returns false if unknown; otherwise reports the
    /// variable's name and whether it had been published.
    bool erase(Handle h, std::string& out_name, bool& out_was_published) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = by_handle_.find(h);
        if (it == by_handle_.end()) return false;
        out_name           = it->second.name;
        out_was_published  = it->second.published;
        by_name_.erase(it->second.name);
        by_handle_.erase(it);
        return true;
    }

    /// Resolve an inbound `set` target. False if unknown or read-only.
    bool resolve_writable(const std::string& name, Handle& out_h, VarKind& out_kind) const {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = by_name_.find(name);
        if (it == by_name_.end()) return false;
        auto eit = by_handle_.find(it->second);
        if (eit == by_handle_.end()) return false;
        if (eit->second.meta.read_only) return false;
        out_h    = it->second;
        out_kind = eit->second.kind;
        return true;
    }

    /// Fetch the setter/meta/kind for applying a write. False if unknown or
    /// read-only.
    bool fetch_for_apply(Handle h, std::function<void(const Value&)>& out_setter,
                         VarMeta& out_meta, VarKind& out_kind) const {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = by_handle_.find(h);
        if (it == by_handle_.end()) return false;
        if (it->second.meta.read_only) return false;
        out_setter = it->second.setter;
        out_meta   = it->second.meta;
        out_kind   = it->second.kind;
        return true;
    }

    /// Refresh last_seen from the getter after an applied write so the
    /// change-detection pass does not re-emit it.
    void refresh_last_seen(Handle h) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = by_handle_.find(h);
        if (it != by_handle_.end() && it->second.getter) {
            it->second.last_seen = it->second.getter();
        }
    }

    /// Snapshot all entries (for replay on (re)connect).
    std::vector<Entry> snapshot() const {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<Entry> out;
        out.reserve(by_handle_.size());
        for (auto& [h, e] : by_handle_) out.push_back(e);
        return out;
    }

    /// Prime change detection: refresh last_seen from getters and mark every
    /// entry as published.
    void prime_all_published() {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& [h, e] : by_handle_) {
            if (e.getter) e.last_seen = e.getter();
            e.published = true;
        }
    }

    /// Detect getter-side changes since the last publish; advances last_seen
    /// and returns the (name, value) pairs that changed.
    std::vector<std::pair<std::string, Value>> collect_changes() {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<std::pair<std::string, Value>> outs;
        for (auto& [h, e] : by_handle_) {
            if (!e.getter || !e.published) continue;
            Value cur = e.getter();
            if (!cur.equals(e.last_seen)) {
                e.last_seen = cur;
                outs.emplace_back(e.name, cur);
            }
        }
        return outs;
    }

private:
    mutable std::mutex                      mtx_;
    std::unordered_map<Handle, Entry>       by_handle_;
    std::unordered_map<std::string, Handle> by_name_;
    Handle                                  next_handle_ = 1;
};

} // namespace ergo::bind::detail
