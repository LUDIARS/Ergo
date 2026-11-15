#include "rule_module.h"
#include <windows.h>
#include <array>
#include <memory>
#include <system_error>
#include <utility>

namespace lesson_action_feel {
namespace {
struct ModuleImage {
    HMODULE handle = nullptr;
    std::array<wchar_t, MAX_PATH + 1> copy{};
    ~ModuleImage() {
        if (handle) FreeLibrary(handle);
        // Best-effort temporary-file cleanup after releasing the image mapping.
        if (copy[0]) DeleteFileW(copy.data());
    }
};
std::filesystem::path source_path() {
    std::array<wchar_t, 32768> path{};
    const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) throw std::runtime_error("Cannot resolve game executable directory");
    return std::filesystem::path(path.data()).parent_path() / L"lesson_rules.dll";
}
std::shared_ptr<ModuleImage> load_image() {
    auto image = std::make_shared<ModuleImage>();
    std::array<wchar_t, MAX_PATH + 1> temp{};
    const DWORD count = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    if (!count || count >= temp.size() || !GetTempFileNameW(temp.data(), L"elf", 0, image->copy.data()))
        throw std::runtime_error("Cannot create temporary rule snapshot");
    if (!CopyFileW(source_path().c_str(), image->copy.data(), FALSE))
        throw std::runtime_error("Cannot copy lesson_rules.dll; build the rule target first");
    image->handle = LoadLibraryExW(image->copy.data(), nullptr,
                                   LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!image->handle) throw std::runtime_error("Cannot load rule snapshot; previous rule kept");
    return image;
}
} // namespace
bool RuleModule::reload(std::string& error) {
    error.clear();
    try {
        auto image = load_image();
        using Version = unsigned (*)();
        using Evaluate = float (*)(unsigned, float, float);
        const auto version = reinterpret_cast<Version>(GetProcAddress(image->handle, "lesson_rule_version"));
        const auto evaluate = reinterpret_cast<Evaluate>(GetProcAddress(image->handle, "lesson_rule_evaluate"));
        if (!version || !evaluate || version() != 1) throw std::runtime_error("Incompatible rule module ABI; previous rule kept");
        RuleFunction next = [image, evaluate](RuleKind kind, float power, float baseline) {
            return evaluate(static_cast<unsigned>(kind), power, baseline);
        };
        // Reject obviously invalid teaching code before publishing it. Runtime calls are checked too.
        for (const float power : {0.0f, 2.0f, 3.0f, 6.0f, 9.0f}) {
            (void)evaluate_rule(next, RuleKind::HitStop, power, power / 24.0f);
            (void)evaluate_rule(next, RuleKind::Knockback, power, power * 6.0f);
        }
        previous_ = current_;
        current_ = std::move(next);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
bool RuleModule::rollback() {
    if (!current_) return false;
    current_ = std::move(previous_); // first reload rolls back to the built-in baseline
    previous_ = {};
    return true;
}
} // namespace lesson_action_feel
