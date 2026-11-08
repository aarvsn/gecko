// SPDX-License-Identifier: MIT
#include "gecko/library.hpp"

#include "gecko/log.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <set>

namespace gecko {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

const std::vector<std::string>& Library::ignoredDirectoryNames() {
    static const std::vector<std::string> names = {
        "saves", "states", "screenshots", ".git", "__macosx", "system", "bios",
    };
    return names;
}

std::string Library::prettifyTitle(const std::string& stem) {
    // Strip region/revision tags: "game (USA) [!]" -> "game", then turn
    // underscores/dots into spaces and collapse runs of whitespace.
    std::string stripped;
    stripped.reserve(stem.size());

    size_t i = 0;
    while (i < stem.size()) {
        const char c = stem[i];
        if (c == '(' || c == '[') {
            const char closing = (c == '(') ? ')' : ']';
            int depth = 0;
            size_t j = i;
            for (; j < stem.size(); ++j) {
                if (stem[j] == c) ++depth;
                else if (stem[j] == closing) {
                    --depth;
                    if (depth == 0) break;
                }
            }
            i = (j < stem.size()) ? j + 1 : stem.size();
            continue;
        }
        stripped.push_back(c);
        ++i;
    }

    std::string out;
    out.reserve(stripped.size());
    bool lastWasSpace = true;
    for (char c : stripped) {
        if (c == '_' || c == '.') c = ' ';
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!lastWasSpace) {
                out.push_back(' ');
                lastWasSpace = true;
            }
        } else {
            out.push_back(c);
            lastWasSpace = false;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::filesystem::path Library::findArtwork(const std::filesystem::path& romPath) {
    const std::filesystem::path stem = romPath.stem();
    const std::filesystem::path dir = romPath.parent_path();
    for (const char* ext : {".png", ".jpg", ".jpeg", ".webp"}) {
        std::error_code ec;
        std::filesystem::path candidate = dir / (stem.string() + ext);
        if (std::filesystem::exists(candidate, ec)) return candidate;
        candidate = dir / "media" / (stem.string() + ext);
        if (std::filesystem::exists(candidate, ec)) return candidate;
    }
    return {};
}

void Library::scan(const std::vector<std::string>& directories,
                   const std::map<std::string, std::string>& extensionCoreMap,
                   const std::vector<std::string>& extensions,
                   std::string* error) {
    games_.clear();
    if (directories.empty()) return;

    std::set<std::string> exts(extensions.begin(), extensions.end());
    constexpr int kMaxDepth = 4;
    std::set<std::string> seenPaths;

    for (const auto& dirString : directories) {
        std::error_code ec;
        std::filesystem::path dir(dirString);
        if (!std::filesystem::exists(dir, ec)) continue;

        std::function<void(const std::filesystem::path&, int)> walk =
            [&](const std::filesystem::path& current, int depth) {
                std::error_code iterEc;
                std::filesystem::directory_iterator it(current, iterEc);
                if (iterEc) return;
                for (const auto& entry : it) {
                    if (entry.is_directory()) {
                        if (depth + 1 <= kMaxDepth) {
                            const std::string name = toLower(entry.path().filename().string());
                            const auto& ignored = ignoredDirectoryNames();
                            if (std::find(ignored.begin(), ignored.end(), name) == ignored.end())
                                walk(entry.path(), depth + 1);
                        }
                        continue;
                    }
                    if (!entry.is_regular_file()) continue;

                    std::string ext = toLower(entry.path().extension().string());
                    if (!ext.empty() && ext.front() == '.') ext.erase(ext.begin());
                    if (exts.find(ext) == exts.end()) continue;

                    const std::string abs = std::filesystem::weakly_canonical(entry.path()).string();
                    if (!seenPaths.insert(abs).second) continue;

                    GameEntry game;
                    game.path = entry.path();
                    game.extension = ext;
                    game.title = prettifyTitle(entry.path().stem().string());
                    game.artworkPath = findArtwork(entry.path());
                    game.fileSize = entry.file_size(ec);
                    auto coreIt = extensionCoreMap.find(ext);
                    if (coreIt != extensionCoreMap.end() && !coreIt->second.empty())
                        game.corePath = coreIt->second;
                    games_.push_back(std::move(game));
                }
            };

        walk(dir, 0);
        if (ec && error && error->empty()) *error = ec.message();
    }

    std::sort(games_.begin(), games_.end(), [](const GameEntry& a, const GameEntry& b) {
        return a.title < b.title;
    });
}

} // namespace gecko
