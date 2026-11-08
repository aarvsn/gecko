// SPDX-License-Identifier: MIT
// Persistent configuration for Gecko. Keys are dotted paths ("video.filter")
// stored as nested JSON objects. The file is UTF-8 JSON, pretty-printed.

#pragma once

#include "json.hpp"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace gecko {

class Config {
public:
    explicit Config(std::filesystem::path filePath);

    // Loads the file if it exists (missing file is not an error). Returns
    // false only when the file exists but cannot be parsed; in that case the
    // previous content is kept and `error` is filled.
    bool load(std::string* error = nullptr);

    // Atomically writes the configuration (write to temp + rename).
    bool save() const;

    // --- typed getters with defaults ---
    std::string getString(const std::string& key, const std::string& fallback = "") const;
    bool getBool(const std::string& key, bool fallback = false) const;
    int64_t getInt(const std::string& key, int64_t fallback = 0) const;
    double getDouble(const std::string& key, double fallback = 0.0) const;
    std::vector<std::string> getStringList(const std::string& key) const;

    // --- setters (update in-memory tree; call save() to persist) ---
    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, const char* value);
    void set(const std::string& key, bool value);
    void set(const std::string& key, int64_t value);
    void set(const std::string& key, double value);
    void setStringList(const std::string& key, const std::vector<std::string>& values);
    void remove(const std::string& key);

    // Raw access for advanced callers (const only).
    const json::Value& root() const { return root_; }

    const std::filesystem::path& filePath() const { return filePath_; }

    // Default root path that holds the config file on this platform:
    //   Linux:   ~/.config/gecko
    //   Windows: %APPDATA%/gecko
    //   macOS:   ~/Library/Application Support/gecko
    static std::filesystem::path defaultConfigDirectory();
    static std::filesystem::path defaultConfigFile();

private:
    const json::Value* find(const std::string& key) const;
    json::Value& ensure(const std::string& key);

    mutable std::mutex mutex_;
    std::filesystem::path filePath_;
    json::Value root_ = json::Value::object();
};

// Default list of ROM extensions the library scanner recognises.
std::vector<std::string> defaultRomExtensions();

} // namespace gecko
