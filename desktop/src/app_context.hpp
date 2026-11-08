// SPDX-License-Identifier: MIT
// AppContext — shared application state for the desktop frontend: config,
// paths, core discovery and library scanning.

#pragma once

#include "gecko/config.hpp"
#include "gecko/core_discovery.hpp"
#include "gecko/library.hpp"

#include <filesystem>
#include <vector>

namespace gecko {

class AppContext {
public:
    explicit AppContext(std::filesystem::path configPath = Config::defaultConfigFile());

    Config& config() { return config_; }
    CoreDiscovery& coreDiscovery() { return coreDiscovery_; }

    // --- standard directories (persisted in config, with defaults) ---
    std::vector<std::string> libraryDirectories() const;
    void setLibraryDirectories(const std::vector<std::string>& dirs);

    std::filesystem::path coresDirectory() const;
    std::filesystem::path systemDirectory() const;
    std::filesystem::path savesDirectory() const;
    std::filesystem::path screenshotsDirectory() const;

    void setCoresDirectory(const std::filesystem::path& p);
    void setSystemDirectory(const std::filesystem::path& p);
    void setSavesDirectory(const std::filesystem::path& p);
    void setScreenshotsDirectory(const std::filesystem::path& p);

    // Creates the standard directories if missing.
    void ensureDirectories();

    // --- core scanning & extension mapping -----------------------------
    std::vector<CoreInfo> scanCores();
    // Lowercase extension -> core path, from config "library.core_map".
    std::map<std::string, std::string> extensionCoreMap() const;
    void setCoreForExtension(const std::string& extension, const std::filesystem::path& corePath);
    std::filesystem::path coreForExtension(const std::string& extension) const;

    // --- library ---------------------------------------------------------
    void rescanLibrary();
    const std::vector<GameEntry>& games() const { return games_; }

    // Extensions accepted by the library scanner (config, with default).
    std::vector<std::string> romExtensions() const;

    // --- save state / screenshot paths ----------------------------------
    std::filesystem::path statePath(const std::string& gameTitle, int slot) const;
    std::filesystem::path screenshotPath(const std::string& gameTitle) const;

private:
    Config config_;
    CoreDiscovery coreDiscovery_;
    std::vector<GameEntry> games_;
};

} // namespace gecko
