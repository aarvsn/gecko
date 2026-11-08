// SPDX-License-Identifier: MIT
// Thin cross-platform wrapper around dynamic library loading
// (dlopen on Unix, LoadLibrary on Windows).

#pragma once

#include <filesystem>
#include <string>

namespace gecko {

class DynamicLibrary {
public:
    DynamicLibrary() = default;
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    ~DynamicLibrary();

    // Loads a shared library. Returns false and fills `error` on failure.
    bool open(const std::filesystem::path& path, std::string* error = nullptr);

    // Closes the library. Note: cores discovered by CoreDiscovery are never
    // closed (see core_discovery.hpp).
    void close();

    bool isOpen() const { return handle_ != nullptr; }

    // Returns nullptr when the symbol is missing.
    void* symbol(const char* name) const;

    template <typename Fn>
    Fn symbolAs(const char* name) const {
        return reinterpret_cast<Fn>(symbol(name));
    }

    const std::filesystem::path& path() const { return path_; }

private:
    void* handle_ = nullptr;
    std::filesystem::path path_;
};

} // namespace gecko
