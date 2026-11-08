# Gecko — a cross-platform libretro frontend (Qt6 desktop, Android, Windows, macOS, Linux).
cmake_minimum_required(VERSION 3.21)

project(Gecko
        VERSION 1.0.0
        DESCRIPTION "Gecko — cross-platform libretro frontend"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(GECKO_BUILD_CORE    "Build gecko-core (the libretro host engine)" ON)
option(GECKO_BUILD_TESTS   "Build tests and the synthetic test core" ON)
option(GECKO_BUILD_DESKTOP "Build the Qt6 desktop frontend" ON)
option(GECKO_WITH_SDL2     "Enable SDL2 gamepad support in the desktop frontend (if found)" ON)
option(GECKO_BUILD_GA330_CORE
       "Also build the vendored GA330 libretro core (requires Unicorn; see vendor/)" OFF)

# Shared library suffix used by the test binary to locate the test core.
if(WIN32)
    set(GECKO_CORE_SUFFIX ".dll")
elseif(APPLE)
    set(GECKO_CORE_SUFFIX ".dylib")
else()
    set(GECKO_CORE_SUFFIX ".so")
endif()

find_package(Threads REQUIRED)

enable_testing()

if(GECKO_BUILD_CORE)
    add_subdirectory(core)
endif()

if(GECKO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(GECKO_BUILD_DESKTOP)
    find_package(Qt6 6.4 QUIET COMPONENTS Widgets)
    if(Qt6_FOUND)
        find_package(Qt6 6.4 QUIET COMPONENTS Multimedia)
        if(TARGET Qt6::Multimedia)
            set(Qt6Multimedia_FOUND TRUE)
            message(STATUS "Qt6 Multimedia found — desktop audio enabled")
        else()
            message(STATUS "Qt6 Multimedia NOT found — desktop app builds without audio")
        endif()
        add_subdirectory(desktop)
    else()
        message(WARNING "Qt6 (>= 6.4, Widgets) not found — desktop frontend will NOT be built. "
                        "See BUILDING.md for Qt6 install instructions, or pass -DGECKO_BUILD_DESKTOP=OFF.")
    endif()
endif()

if(GECKO_BUILD_GA330_CORE)
    add_subdirectory(vendor/ga330-libretro EXCLUDE_FROM_ALL)
endif()
