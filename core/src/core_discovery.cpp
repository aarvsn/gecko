// SPDX-License-Identifier: MIT
#include "gecko/core_discovery.hpp"

#include "gecko/dynamic_library.hpp"
#include "gecko/libretro.h"
#include "gecko/log.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <sstream>

namespace gecko {

namespace {

// Cores that were dlopen()ed for discovery. They are intentionally never
// closed: many libretro cores are not dlclose-safe (static state, atexit
// hooks), and RetroArch follows the same policy.
std::mutex& registryMutex() {
    static std::mutex m;
    return m;
}
std::vector<std::unique_ptr<DynamicLibrary>>& registry() {
    static std::vector<std::unique_ptr<DynamicLibrary>> v;
    return v;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool hasCoreSuffix(const std::filesystem::path& p) {
    const std::string ext = toLower(p.extension().string());
    for (const auto& suffix : CoreDiscovery::coreFileSuffixes())
        if (ext == "." + suffix) return true;
    return false;
}

// Queries a core through a (possibly cached) open handle.
bool probeCore(const std::filesystem::path& path, CoreInfo* out) {
    std::lock_guard<std::mutex> guard(registryMutex());

    for (const auto& lib : registry()) {
        if (std::filesystem::path(lib->path()) == path) {
            // Already probed; the caller uses the cache_ in CoreDiscovery.
            return true;
        }
    }

    auto lib = std::make_unique<DynamicLibrary>();
    std::string error;
    if (!lib->open(path, &error)) {
        logMessage(LogLevel::Warning, "core scan: cannot open %s (%s)",
                   path.string().c_str(), error.c_str());
        return false;
    }

    auto apiVersion = lib->symbolAs<unsigned (*)()>("retro_api_version");
    auto getSystemInfo = lib->symbolAs<void (*)(retro_system_info*)>("retro_get_system_info");
    if (!apiVersion || !getSystemInfo) {
        logMessage(LogLevel::Debug, "core scan: %s is not a libretro core", path.string().c_str());
        return false;
    }
    if (apiVersion() != RETRO_API_VERSION) {
        logMessage(LogLevel::Warning, "core scan: %s has ABI %u, expected %u",
                   path.string().c_str(), apiVersion(), RETRO_API_VERSION);
        return false;
    }

    retro_system_info info{};
    getSystemInfo(&info);

    out->path = path;
    out->fileName = path.stem().string();
    out->libraryName = info.library_name ? info.library_name : out->fileName;
    out->libraryVersion = info.library_version ? info.library_version : "";
    out->validExtensions = info.valid_extensions ? info.valid_extensions : "";
    out->needFullPath = info.need_fullpath;
    out->blockExtract = info.block_extract;

    // supports_no_game is only reported through the environment callback; we
    // would have to run retro_set_environment to learn it, which some cores
    // require before anything else. Leave it false here — the Emulator sets
    // the authoritative value once the core is actually loaded.
    out->supportsNoGame = false;

    registry().push_back(std::move(lib));
    return true;
}

} // namespace

std::vector<std::string> CoreDiscovery::coreFileSuffixes() {
#if defined(_WIN32)
    return {"dll"};
#elif defined(__APPLE__)
    return {"dylib", "so"};
#else
    return {"so"};
#endif
}

std::vector<std::string> CoreDiscovery::splitExtensions(const std::string& validExtensions) {
    std::vector<std::string> result;
    std::string current;
    std::istringstream stream(validExtensions);
    while (std::getline(stream, current, '|')) {
        current = toLower(current);
        // Accept both "ext" and ".ext" spellings.
        if (!current.empty() && current.front() == '.') current.erase(current.begin());
        if (!current.empty()) result.push_back(current);
    }
    return result;
}

std::vector<CoreInfo> CoreDiscovery::scanDirectory(const std::filesystem::path& directory) {
    std::vector<CoreInfo> found;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) return found;

    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (!entry.is_regular_file()) continue;
        const auto& path = entry.path();
        if (!hasCoreSuffix(path)) continue;

        const std::string key = std::filesystem::absolute(path).string();
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            found.push_back(it->second);
            continue;
        }
        CoreInfo info;
        if (probeCore(path, &info)) {
            cache_.emplace(key, info);
            found.push_back(std::move(info));
        }
    }
    if (ec) {
        logMessage(LogLevel::Warning, "core scan: %s: %s", directory.string().c_str(),
                   ec.message().c_str());
    }

    std::sort(found.begin(), found.end(), [](const CoreInfo& a, const CoreInfo& b) {
        return a.libraryName < b.libraryName;
    });
    return found;
}

CoreInfo CoreDiscovery::infoForCore(const std::filesystem::path& corePath) {
    const std::string key = std::filesystem::absolute(corePath).string();
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;

    CoreInfo info;
    if (probeCore(corePath, &info)) {
        cache_.emplace(key, info);
        return info;
    }
    info.path = corePath;
    info.fileName = corePath.stem().string();
    info.libraryName = info.fileName;
    return info;
}

} // namespace gecko
