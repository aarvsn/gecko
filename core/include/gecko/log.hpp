// SPDX-License-Identifier: MIT
// Logging interface for gecko-core. The UI layers install a sink; without one,
// messages go to stderr.

#pragma once

#include <string>

namespace gecko {

enum class LogLevel { Debug, Info, Warning, Error };

const char* logLevelName(LogLevel level);

using LogCallback = void (*)(LogLevel level, const char* message, size_t length, void* userData);

// Install a global log sink. Passing nullptr restores the stderr fallback.
void setLogCallback(LogCallback callback, void* userData);

void logMessage(LogLevel level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

} // namespace gecko
