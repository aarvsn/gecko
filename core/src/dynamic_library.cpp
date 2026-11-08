// SPDX-License-Identifier: MIT
#include "gecko/dynamic_library.hpp"

#include "gecko/log.hpp"

#include <cstdlib>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace gecko {

DynamicLibrary::~DynamicLibrary() {
    // Intentionally does NOT close: libretro cores register global state and
    // atexit handlers that make dlclose() unsafe. Releasing the OS handle
    // would keep the library mapped anyway; leaking by design (RetroArch
    // behaves the same). Use close() explicitly when you know it is safe.
}

bool DynamicLibrary::open(const std::filesystem::path& path, std::string* error) {
#if defined(_WIN32)
    handle_ = static_cast<void*>(LoadLibraryW(path.wstring().c_str()));
    if (!handle_ && error) {
        *error = "LoadLibrary failed with error code " + std::to_string(GetLastError());
    }
#else
    handle_ = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle_ && error) {
        const char* msg = dlerror();
        *error = msg ? std::string(msg) : "dlopen failed";
    }
#endif
    if (handle_) {
        path_ = path;
    }
    return handle_ != nullptr;
}

void DynamicLibrary::close() {
    if (!handle_) return;
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
    handle_ = nullptr;
    path_.clear();
}

void* DynamicLibrary::symbol(const char* name) const {
    if (!handle_) return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
    return dlsym(handle_, name);
#endif
}

} // namespace gecko
