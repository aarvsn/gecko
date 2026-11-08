// SPDX-License-Identifier: MIT
// Gecko — a cross-platform libretro frontend.
// Version information for the gecko-core host library.

#pragma once

#define GECKO_VERSION_MAJOR 1
#define GECKO_VERSION_MINOR 0
#define GECKO_VERSION_PATCH 0
#define GECKO_VERSION_STRING "1.0.0"
#define GECKO_PROJECT_NAME "Gecko"
#define GECKO_PROJECT_URL "https://github.com/gecko-emu/gecko"

namespace gecko {

inline const char* versionString() { return GECKO_VERSION_STRING; }

} // namespace gecko
