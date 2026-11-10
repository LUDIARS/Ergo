#include "dynamic_library.h"

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace ergo::studio::host {

std::shared_ptr<DynamicLibrary> DynamicLibrary::open(const std::filesystem::path& file, std::string& err) {
    const auto abs = std::filesystem::absolute(file);
#ifdef _WIN32
    // A broken pack must not open a modal "bad image" box that blocks startup.
    DWORD prev_mode = 0;
    SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &prev_mode);
    HMODULE h = LoadLibraryExW(abs.c_str(), nullptr,
                               LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    const DWORD load_err = GetLastError();
    SetThreadErrorMode(prev_mode, nullptr);
    if (!h) {
        err = "LoadLibrary failed (error " + std::to_string(load_err) + ")";
        return nullptr;
    }
    return std::make_shared<DynamicLibrary>(Key{}, reinterpret_cast<void*>(h));
#else
    void* h = dlopen(abs.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        const char* e = dlerror();
        err = e ? e : "dlopen failed";
        return nullptr;
    }
    return std::make_shared<DynamicLibrary>(Key{}, h);
#endif
}

DynamicLibrary::~DynamicLibrary() {
#ifdef _WIN32
    FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
}

void* DynamicLibrary::symbol(const char* name) const {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle_), name));
#else
    return dlsym(handle_, name);
#endif
}

} // namespace ergo::studio::host
