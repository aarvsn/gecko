# Native build for the Gecko Android app: JNI bridge + gecko-core.
cmake_minimum_required(VERSION 3.22)
project(gecko_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to the shared gecko-core library, relative to this file:
# android/app/src/main/jni -> gecko root
set(GECKO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)
# (android/app/src/main/jni -> android/app/src/main -> android/app -> android -> gecko)
get_filename_component(GECKO_ROOT "${GECKO_ROOT}" ABSOLUTE)

set(GECKO_CORE_SOURCES
        ${GECKO_ROOT}/core/src/audio_sink.cpp
        ${GECKO_ROOT}/core/src/config.cpp
        ${GECKO_ROOT}/core/src/core_discovery.cpp
        ${GECKO_ROOT}/core/src/dynamic_library.cpp
        ${GECKO_ROOT}/core/src/emulator.cpp
        ${GECKO_ROOT}/core/src/input_source.cpp
        ${GECKO_ROOT}/core/src/json.cpp
        ${GECKO_ROOT}/core/src/library.cpp
        ${GECKO_ROOT}/core/src/log.cpp
        ${GECKO_ROOT}/core/src/runner.cpp
        ${GECKO_ROOT}/core/src/video_sink.cpp
        )

add_library(gecko_native SHARED
        gecko_jni.cpp
        ${GECKO_CORE_SOURCES}
        )

target_include_directories(gecko_native PRIVATE
        ${GECKO_ROOT}/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

find_library(log-lib log)
find_library(android-lib android)
target_link_libraries(gecko_native PRIVATE ${log-lib} ${android-lib})

# Strip in release; keep symbols for debug builds automatically.
target_compile_options(gecko_native PRIVATE -Wall)
