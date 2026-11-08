// SPDX-License-Identifier: MIT
#include "gecko/log.hpp"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace gecko {

namespace {
LogCallback g_callback = nullptr;
void* g_userData = nullptr;
} // namespace

const char* logLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warning: return "warning";
        case LogLevel::Error: return "error";
    }
    return "info";
}

void setLogCallback(LogCallback callback, void* userData) {
    g_callback = callback;
    g_userData = userData;
}

void logMessage(LogLevel level, const char* fmt, ...) {
    char stackBuffer[512];
    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(stackBuffer, sizeof(stackBuffer), fmt, args);
    va_end(args);

    std::vector<char> heapBuffer;
    char* text = stackBuffer;
    if (needed < 0) {
        text = stackBuffer; // encoding error; print what we have
        needed = 0;
    } else if (static_cast<size_t>(needed) >= sizeof(stackBuffer)) {
        heapBuffer.resize(static_cast<size_t>(needed) + 1);
        va_start(args, fmt);
        vsnprintf(heapBuffer.data(), heapBuffer.size(), fmt, args);
        va_end(args);
        text = heapBuffer.data();
    }

    if (g_callback) {
        g_callback(level, text, static_cast<size_t>(needed > 0 ? needed : 0), g_userData);
    } else {
        std::fprintf(stderr, "[gecko:%s] %s\n", logLevelName(level), text);
    }
}

} // namespace gecko
