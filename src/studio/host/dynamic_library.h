#pragma once
/// Shared-library handle (LoadLibrary / dlopen). Internal to ergo_studio_host.
#include <filesystem>
#include <memory>
#include <string>

namespace ergo::studio::host {

class DynamicLibrary {
    struct Key {   ///< only open() can make one, so only open() constructs
        explicit Key() = default;
    };

public:
    /// nullptr with `err` set when the library cannot be loaded. Dependencies
    /// next to the library are found (Windows: LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR).
    static std::shared_ptr<DynamicLibrary> open(const std::filesystem::path& file, std::string& err);
    DynamicLibrary(Key, void* handle) : handle_(handle) {}
    ~DynamicLibrary();
    DynamicLibrary(const DynamicLibrary&)            = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    /// nullptr when the library does not export `name`.
    void* symbol(const char* name) const;

private:
    void* handle_;
};

} // namespace ergo::studio::host
