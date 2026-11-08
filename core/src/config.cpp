// SPDX-License-Identifier: MIT
#include "gecko/config.hpp"

#include "gecko/log.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <shlobj.h>
#elif defined(__APPLE__)
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

namespace gecko {

namespace {

[[maybe_unused]] std::string lowercase(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

} // namespace

std::vector<std::string> defaultRomExtensions() {
    return {
        // SNES
        "smc", "sfc", "swc", "fig",
        // NES / Famicom
        "nes", "fds", "unf", "unif",
        // Genesis / Mega Drive, Sega CD, 32X
        "md", "gen", "smd", "mdx", "32x", "cue", "iso",
        // Game Boy family
        "gb", "gbc", "sgb", "gba", "agb",
        // Nintendo 64
        "n64", "z64", "v64", "u1", "ndd",
        // PlayStation
        "bin", "chd", "img", "pbp", "m3u",
        // Master System / Game Gear / SG-1000
        "sms", "gg", "sg",
        // PC Engine
        "pce", "sgx", "ccd",
        // Neo Geo Pocket
        "ngp", "ngc",
        // Atari
        "a26", "a78", "lnx", "col", "int",
        // Virtual Boy
        "vb",
        // Arcade / compressed archives
        "7z", "zip",
        // GA330 native applications (CCDL binaries)
        "ccdl",
        // WonderSwan
        "ws", "wsc",
    };
}

Config::Config(std::filesystem::path filePath) : filePath_(std::move(filePath)) {}

bool Config::load(std::string* error) {
    std::error_code ec;
    if (!std::filesystem::exists(filePath_, ec)) return true; // fresh install

    std::ifstream in(filePath_, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open " + filePath_.string();
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();

    std::string parseError;
    json::Value parsed = json::parse(text, &parseError);
    if (parseError.empty() && parsed.isObject()) {
        std::lock_guard<std::mutex> guard(mutex_);
        root_ = std::move(parsed);
        return true;
    }
    if (error) *error = "config parse error: " + parseError;
    return false;
}

bool Config::save() const {
    std::error_code ec;
    const auto dir = filePath_.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir, ec)) {
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            logMessage(LogLevel::Error, "cannot create config directory %s: %s",
                       dir.string().c_str(), ec.message().c_str());
            return false;
        }
    }

    const std::string text = json::serialize(root_, true);
    const std::filesystem::path tmp = filePath_.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            logMessage(LogLevel::Error, "cannot write %s", tmp.string().c_str());
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out.good()) {
            logMessage(LogLevel::Error, "short write to %s", tmp.string().c_str());
            return false;
        }
    }
    std::filesystem::rename(tmp, filePath_, ec);
    if (ec) {
        // Fall back to a direct write (some network filesystems dislike renames).
        std::ofstream out(filePath_, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return out.good();
    }
    return true;
}

const json::Value* Config::find(const std::string& key) const {
    const json::Value* node = &root_;
    size_t start = 0;
    while (start <= key.size()) {
        const size_t dot = key.find('.', start);
        const std::string part = key.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (part.empty()) return nullptr;
        if (!node->isObject()) return nullptr;
        node = node->find(part);
        if (!node) return nullptr;
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return node;
}

json::Value& Config::ensure(const std::string& key) {
    json::Value* node = &root_;
    if (!root_.isObject()) root_ = json::Value::object();
    size_t start = 0;
    while (start <= key.size()) {
        const size_t dot = key.find('.', start);
        const std::string part = key.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (part.empty()) break;
        node = &node->getOrCreate(part);
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return *node;
}

std::string Config::getString(const std::string& key, const std::string& fallback) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const json::Value* v = find(key);
    return v && v->isString() ? v->asString() : fallback;
}

bool Config::getBool(const std::string& key, bool fallback) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const json::Value* v = find(key);
    return v && v->isBoolean() ? v->asBool() : fallback;
}

int64_t Config::getInt(const std::string& key, int64_t fallback) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const json::Value* v = find(key);
    return v && v->isNumber() ? v->asInteger() : fallback;
}

double Config::getDouble(const std::string& key, double fallback) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const json::Value* v = find(key);
    return v && v->isNumber() ? v->asNumber() : fallback;
}

std::vector<std::string> Config::getStringList(const std::string& key) const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::vector<std::string> result;
    const json::Value* v = find(key);
    if (v && v->isArray()) {
        for (const json::Value& item : v->items())
            if (item.isString()) result.push_back(item.asString());
    }
    return result;
}

void Config::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> guard(mutex_);
    const size_t dot = key.rfind('.');
    const std::string parent = dot == std::string::npos ? std::string() : key.substr(0, dot);
    const std::string leaf = dot == std::string::npos ? key : key.substr(dot + 1);
    if (!parent.empty()) {
        json::Value& node = ensure(parent);
        node.set(leaf, json::Value(value));
    } else {
        root_.set(key, json::Value(value));
    }
}

void Config::set(const std::string& key, const char* value) {
    set(key, std::string(value));
}

void Config::set(const std::string& key, bool value) {
    std::lock_guard<std::mutex> guard(mutex_);
    const size_t dot = key.rfind('.');
    const std::string parent = dot == std::string::npos ? std::string() : key.substr(0, dot);
    const std::string leaf = dot == std::string::npos ? key : key.substr(dot + 1);
    json::Value& node = parent.empty() ? root_ : ensure(parent);
    node.set(leaf, json::Value(value));
}

void Config::set(const std::string& key, int64_t value) {
    std::lock_guard<std::mutex> guard(mutex_);
    const size_t dot = key.rfind('.');
    const std::string parent = dot == std::string::npos ? std::string() : key.substr(0, dot);
    const std::string leaf = dot == std::string::npos ? key : key.substr(dot + 1);
    json::Value& node = parent.empty() ? root_ : ensure(parent);
    node.set(leaf, json::Value(static_cast<double>(value)));
}

void Config::set(const std::string& key, double value) {
    std::lock_guard<std::mutex> guard(mutex_);
    const size_t dot = key.rfind('.');
    const std::string parent = dot == std::string::npos ? std::string() : key.substr(0, dot);
    const std::string leaf = dot == std::string::npos ? key : key.substr(dot + 1);
    json::Value& node = parent.empty() ? root_ : ensure(parent);
    node.set(leaf, json::Value(value));
}

void Config::setStringList(const std::string& key, const std::vector<std::string>& values) {
    std::lock_guard<std::mutex> guard(mutex_);
    const size_t dot = key.rfind('.');
    const std::string parent = dot == std::string::npos ? std::string() : key.substr(0, dot);
    const std::string leaf = dot == std::string::npos ? key : key.substr(dot + 1);
    json::Value list = json::Value::array();
    for (const auto& v : values) list.append(json::Value(v));
    json::Value& node = parent.empty() ? root_ : ensure(parent);
    node.set(leaf, std::move(list));
}

void Config::remove(const std::string& key) {
    std::lock_guard<std::mutex> guard(mutex_);
    const size_t dot = key.rfind('.');
    if (dot == std::string::npos) return; // removing top-level keys is not supported on purpose
    const std::string parent = key.substr(0, dot);
    const std::string leaf = key.substr(dot + 1);
    json::Value* node = parent.empty() ? &root_ : const_cast<json::Value*>(find(parent));
    if (node && node->isObject()) {
        auto& members = node->members();
        for (auto it = members.begin(); it != members.end(); ++it) {
            if (it->first == leaf) {
                members.erase(it);
                return;
            }
        }
    }
}

std::filesystem::path Config::defaultConfigDirectory() {
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    if (appData) return std::filesystem::path(appData) / "gecko";
    return std::filesystem::path(".") / "gecko";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home)
        return std::filesystem::path(home) / "Library" / "Application Support" / "gecko";
    return std::filesystem::path(".") / "gecko";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return std::filesystem::path(xdg) / "gecko";
    const char* home = std::getenv("HOME");
    if (home) return std::filesystem::path(home) / ".config" / "gecko";
    return std::filesystem::path(".") / "gecko";
#endif
}

std::filesystem::path Config::defaultConfigFile() {
    return defaultConfigDirectory() / "gecko.json";
}

} // namespace gecko
