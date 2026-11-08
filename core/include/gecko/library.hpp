// SPDX-License-Identifier: MIT
// Game library scanning: walks the configured ROM directories and produces
// the list shown in the UI grid.

#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace gecko {

struct GameEntry {
    std::filesystem::path path;
    std::string title;         // prettified file name
    std::string extension;     // lowercase, no dot
    // Empty when no core is associated with the extension yet; the UI asks
    // the user to pick a core in that case.
    std::filesystem::path corePath;
    // Optional artwork found next to the ROM (same stem, .png/.jpg/.jpeg).
    std::filesystem::path artworkPath;
    uintmax_t fileSize = 0;
};

class Library {
public:
    // `directories` are scanned recursively (bounded depth).
    // `extensionCoreMap` maps lowercase extension -> core path.
    // `extensions` limits what is collected (lowercase, no dots).
    void scan(const std::vector<std::string>& directories,
              const std::map<std::string, std::string>& extensionCoreMap,
              const std::vector<std::string>& extensions,
              std::string* error = nullptr);

    const std::vector<GameEntry>& games() const { return games_; }
    size_t count() const { return games_.size(); }
    void clear() { games_.clear(); }

    // Turns "super_mario_world_(usa)" into "super mario world".
    static std::string prettifyTitle(const std::string& fileNameStem);

    // Looks for <stem>.png / .jpg / .jpeg / .webp in the same directory.
    static std::filesystem::path findArtwork(const std::filesystem::path& romPath);

    // Directory names skipped during recursion.
    static const std::vector<std::string>& ignoredDirectoryNames();

private:
    std::vector<GameEntry> games_;
};

} // namespace gecko
