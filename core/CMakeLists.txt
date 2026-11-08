# gecko-core: platform-agnostic libretro host library (no UI dependencies).
# Used by the Qt6 desktop frontend, the Android JNI bridge and the tests.

add_library(gecko-core STATIC
        src/audio_sink.cpp
        src/config.cpp
        src/core_discovery.cpp
        src/dynamic_library.cpp
        src/emulator.cpp
        src/input_source.cpp
        src/json.cpp
        src/library.cpp
        src/log.cpp
        src/runner.cpp
        src/video_sink.cpp
        )

add_library(gecko::core ALIAS gecko-core)

target_include_directories(gecko-core
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        )

target_compile_features(gecko-core PUBLIC cxx_std_17)

if(WIN32)
    # nothing extra: LoadLibrary/FreeLibrary live in kernel32
elseif(APPLE)
    target_link_libraries(gecko-core PUBLIC ${CMAKE_DL_LIBS})
else()
    target_link_libraries(gecko-core PUBLIC ${CMAKE_DL_LIBS} Threads::Threads)
endif()

if(MSVC)
    target_compile_options(gecko-core PRIVATE /W3)
else()
    target_compile_options(gecko-core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
