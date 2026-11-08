// SPDX-License-Identifier: MIT
#include "app_context.hpp"

#include "gecko/library.hpp"
#include "gecko/log.hpp"

#include <ctime>

namespace gecko {

AppContext::AppContext(std::filesystem::path configPath) : config_(std::move(configPath)) {
    std::string error;
    if (!config_.load(&error)) {
        logMessage(LogLevel::Warning, "config: %s (continuing with defaults)", error.c_str());
    }
}

std::vector<std::string> AppContext::libraryDirectories() const {
    auto dirs = config_.getStringList("library.directories");
    if (dirs.empty()) {
        // Sensible first-run default: ~/Games and ~/ROMs when they exist.
        const char* home = std::getenv("HOME");
        if (home) {
            const std::filesystem::path h(home);
            for (const char* candidate : {"Games", "ROMs", "Emulation/ROMs"}) {
                std::error_code ec;
                if (std::filesystem::exists(h / candidate, ec))
                    dirs.push_back((h / candidate).string());
            }
        }
    }
    return dirs;
}

void AppContext::setLibraryDirectories(const std::vector<std::string>& dirs) {
    config_.setStringList("library.directories", dirs);
    config_.save();
}

std::filesystem::path AppContext::coresDirectory() const {
    const std::string v = config_.getString("paths.cores");
    if (!v.empty()) return v;
    // Default: <config dir>/cores — the frontend also works as a portable
    // build when a "cores" directory sits next to the executable.
    std::error_code ec;
    const auto portable = std::filesystem::current_path(ec) / "cores";
    if (std::filesystem::exists(portable, ec)) return portable;
    return Config::defaultConfigDirectory() / "cores";
}

std::filesystem::path AppContext::systemDirectory() const {
    const std::string v = config_.getString("paths.system");
    return v.empty() ? Config::defaultConfigDirectory() / "system" : std::filesystem::path(v);
}

std::filesystem::path AppContext::savesDirectory() const {
    const std::string v = config_.getString("paths.saves");
    return v.empty() ? Config::defaultConfigDirectory() / "saves" : std::filesystem::path(v);
}

std::filesystem::path AppContext::screenshotsDirectory() const {
    const std::string v = config_.getString("paths.screenshots");
    return v.empty() ? Config::defaultConfigDirectory() / "screenshots"
                     : std::filesystem::path(v);
}

void AppContext::setCoresDirectory(const std::filesystem::path& p) {
    config_.set("paths.cores", p.string());
    config_.save();
}
void AppContext::setSystemDirectory(const std::filesystem::path& p) {
    config_.set("paths.system", p.string());
    config_.save();
}
void AppContext::setSavesDirectory(const std::filesystem::path& p) {
    config_.set("paths.saves", p.string());
    config_.save();
}
void AppContext::setScreenshotsDirectory(const std::filesystem::path& p) {
    config_.set("paths.screenshots", p.string());
    config_.save();
}

void AppContext::ensureDirectories() {
    for (const auto& dir :
         {coresDirectory(), systemDirectory(), savesDirectory(), screenshotsDirectory()}) {
        std::error_code ec;
        if (!dir.empty() && !std::filesystem::exists(dir, ec))
            std::filesystem::create_directories(dir, ec);
    }
}

std::vector<CoreInfo> AppContext::scanCores() {
    return coreDiscovery_.scanDirectory(coresDirectory());
}

std::map<std::string, std::string> AppContext::extensionCoreMap() const {
    std::map<std::string, std::string> map;
    const json::Value* node = config_.root().find("library");
    if (!node || !node->isObject()) return map;
    const json::Value* mapNode = node->find("core_map");
    if (!mapNode || !mapNode->isObject()) return map;
    for (const auto& [ext, path] : mapNode->members())
        if (path.isString()) map[ext] = path.asString();
    return map;
}

void AppContext::setCoreForExtension(const std::string& extension,
                                     const std::filesystem::path& corePath) {
    config_.set("library.core_map." + extension, corePath.string());
    config_.save();
}

std::filesystem::path AppContext::coreForExtension(const std::string& extension) const {
    const std::string v = config_.getString("library.core_map." + extension);
    return v.empty() ? std::filesystem::path() : std::filesystem::path(v);
}

std::vector<std::string> AppContext::romExtensions() const {
    auto exts = config_.getStringList("library.extensions");
    if (exts.empty()) exts = defaultRomExtensions();
    return exts;
}

void AppContext::rescanLibrary() {
    games_.clear();
    Library lib;
    lib.scan(libraryDirectories(), extensionCoreMap(), romExtensions());
    games_ = lib.games();
}

std::filesystem::path AppContext::statePath(const std::string& gameTitle, int slot) const {
    return savesDirectory() / (gameTitle + ".state" + std::to_string(slot));
}

std::filesystem::path AppContext::screenshotPath(const std::string& gameTitle) const {
    std::time_t now = std::time(nullptr);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
    return screenshotsDirectory() / (gameTitle + "-" + stamp + ".png");
}

} // namespace gecko
