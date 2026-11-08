// SPDX-License-Identifier: MIT
// Core discovery: scans a directory for libretro cores and queries each one
// for its identity (library name, version, supported extensions).

#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace gecko {

struct CoreInfo {
    std::filesystem::path path;
    std::string fileName;      // "snes9x" (no extension)
    std::string libraryName;   // e.g. "GA330", reported by the core itself
    std::string libraryVersion;
    std::string validExtensions; // space separated, without dots
    bool supportsNoGame = false;
    bool needFullPath = false;
    bool blockExtract = false;
};

class CoreDiscovery {
public:
    // Scans `directory` for shared libraries that export retro_api_version.
    // Cores are dlopen()ed but intentionally never unloaded — many cores
    // keep static state and crash on dlclose (RetroArch does the same).
    // Results are cached per absolute path within the process lifetime.
    std::vector<CoreInfo> scanDirectory(const std::filesystem::path& directory);

    // Cached lookup by path (scans the file directly if not cached).
    CoreInfo infoForCore(const std::filesystem::path& corePath);

    // Splits a core's valid_extensions string into lowercase extensions
    // ("smc", "sfc", ...).
    static std::vector<std::string> splitExtensions(const std::string& validExtensions);

    // Platform-appropriate core file suffixes ("so", "dll", "dylib").
    static std::vector<std::string> coreFileSuffixes();

private:
    std::map<std::string, CoreInfo> cache_;
};

} // namespace gecko
