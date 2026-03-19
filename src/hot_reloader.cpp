#include "ergo/compose/hot_reloader.h"

#include <filesystem>
#include <algorithm>

namespace ergo::compose {

HotReloader::HotReloader() = default;

HotReloader::~HotReloader() {
    stop();
}

void HotReloader::start(const std::string& watchPath) {
    if (running_.load()) return;

    watchPath_ = watchPath;
    running_.store(true);
    state_.store(HotReloadState::Idle);

    watchThread_ = std::thread(&HotReloader::watchLoop, this);
}

void HotReloader::stop() {
    running_.store(false);
    if (watchThread_.joinable()) {
        watchThread_.join();
    }
    state_.store(HotReloadState::Idle);
}

bool HotReloader::isRunning() const {
    return running_.load();
}

void HotReloader::addWatchTarget(const std::string& filePath, const std::string& actorName) {
    std::lock_guard<std::mutex> lock(mutex_);

    FileDiffInfo diffInfo;
    diffInfo.sourcePath = filePath;
    if (std::filesystem::exists(filePath)) {
        diffInfo.lastModified = std::chrono::steady_clock::now();
    }

    watchTargets_[filePath] = {actorName, diffInfo};
    editCounts_[filePath] = 0;
}

void HotReloader::removeWatchTarget(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    watchTargets_.erase(filePath);
    editCounts_.erase(filePath);
}

void HotReloader::setCallback(HotReloadCallback callback) {
    callback_ = std::move(callback);
}

HotReloadState HotReloader::getState() const {
    return state_.load();
}

std::vector<std::string> HotReloader::getChangedFiles() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto result = std::move(changedFiles_);
    changedFiles_.clear();
    return result;
}

void HotReloader::setSpeculativeConfig(const SpeculativeCompileConfig& config) {
    specConfig_ = config;
}

std::vector<TypeScriptSource> HotReloader::getSpeculativeCandidates() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<TypeScriptSource> candidates;
    auto now = std::chrono::steady_clock::now();

    for (const auto& [filePath, targetInfo] : watchTargets_) {
        const auto& [actorName, diffInfo] = targetInfo;

        // 編集頻度が閾値以下かチェック
        auto editIt = editCounts_.find(filePath);
        uint32_t editCount = (editIt != editCounts_.end()) ? editIt->second : 0;

        if (editCount > specConfig_.editFrequencyThreshold) {
            continue; // 頻繁に編集されるファイルはスキップ
        }

        // 最終更新からアイドル閾値を超えているかチェック
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - diffInfo.lastModified);

        if (elapsed < specConfig_.idleThreshold) {
            continue; // まだアイドル時間が足りない
        }

        // まだC++に変換されていないTSファイルが対象
        if (!diffInfo.compiledPath.empty() &&
            std::filesystem::exists(diffInfo.compiledPath)) {
            continue; // 既にコンパイル済み
        }

        TypeScriptSource source;
        source.filePath = filePath;
        source.actorName = actorName;
        source.status = TranspileStatus::Pending;
        source.diffInfo = diffInfo;
        candidates.push_back(std::move(source));
    }

    return candidates;
}

void HotReloader::runSpeculativeCompile() {
    if (!specConfig_.enabled) return;

    // 投機的コンパイル対象を取得し、Transpilerで変換
    // 実際の変換はComposeSystemから呼び出される
    // ここでは対象ファイルの状態を更新する
    auto candidates = getSpeculativeCandidates();
    // ComposeSystem がこのリストを取得して transpiler に渡す
}

void HotReloader::recordEdit(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    editCounts_[filePath]++;

    auto it = watchTargets_.find(filePath);
    if (it != watchTargets_.end()) {
        it->second.second.lastModified = std::chrono::steady_clock::now();
    }
}

void HotReloader::watchLoop() {
    namespace fs = std::filesystem;

    // ファイルの最終更新時刻キャッシュ
    std::unordered_map<std::string, fs::file_time_type> lastWriteTimes;

    // 初期状態を記録
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [filePath, _] : watchTargets_) {
            if (fs::exists(filePath)) {
                lastWriteTimes[filePath] = fs::last_write_time(filePath);
            }
        }
    }

    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& [filePath, targetInfo] : watchTargets_) {
            if (!fs::exists(filePath)) continue;

            auto currentTime = fs::last_write_time(filePath);
            auto it = lastWriteTimes.find(filePath);

            if (it == lastWriteTimes.end()) {
                lastWriteTimes[filePath] = currentTime;
                continue;
            }

            if (currentTime != it->second) {
                // ファイル変更を検出
                it->second = currentTime;
                state_.store(HotReloadState::Detected);
                changedFiles_.push_back(filePath);

                const auto& [actorName, diffInfo] = targetInfo;

                if (callback_) {
                    callback_(actorName, HotReloadState::Detected);
                }
            }
        }

        if (changedFiles_.empty()) {
            state_.store(HotReloadState::Idle);
        }
    }
}

} // namespace ergo::compose
