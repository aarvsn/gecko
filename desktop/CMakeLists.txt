# Gecko Qt6 desktop frontend (Windows / macOS / Linux).

set(GECKO_DESKTOP_SOURCES
        src/main.cpp
        src/app_context.cpp
        src/audio_bridge.cpp
        src/core_manager_dialog.cpp
        src/emulator_view.cpp
        src/emulator_window.cpp
        src/input_manager.cpp
        src/input_mapper_dialog.cpp
        src/library_model.cpp
        src/mainwindow.cpp
        src/overlay_menu.cpp
        src/settings_dialog.cpp
        src/video_filters.cpp
        )

set(GECKO_DESKTOP_HEADERS
        src/app_context.hpp
        src/audio_bridge.hpp
        src/core_manager_dialog.hpp
        src/emulator_view.hpp
        src/emulator_window.hpp
        src/frame_buffer.hpp
        src/input_manager.hpp
        src/input_mapper_dialog.hpp
        src/library_model.hpp
        src/mainwindow.hpp
        src/overlay_menu.hpp
        src/settings_dialog.hpp
        src/video_filters.hpp
        )

set(GECKO_DESKTOP_RES
        resources/gecko.qrc
        )

qt_add_executable(gecko WIN32
        ${GECKO_DESKTOP_SOURCES}
        ${GECKO_DESKTOP_HEADERS}
        ${GECKO_DESKTOP_RES}
        )

set_target_properties(gecko PROPERTIES AUTOMOC ON AUTORCC ON)

set_target_properties(gecko PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Gecko"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.gecko-emu.Gecko"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        )

target_include_directories(gecko PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gecko PRIVATE gecko::core Qt6::Widgets)

if(Qt6Multimedia_FOUND)
    target_compile_definitions(gecko PRIVATE GECKO_WITH_QTMULTIMEDIA=1)
    target_link_libraries(gecko PRIVATE Qt6::Multimedia)
endif()

# Optional SDL2 gamepad support.
if(GECKO_WITH_SDL2)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        message(STATUS "SDL2 found — gamepad support enabled")
        target_compile_definitions(gecko PRIVATE GECKO_WITH_SDL2=1)
        target_link_libraries(gecko PRIVATE SDL2::SDL2)
    else()
        message(STATUS "SDL2 not found — gamepad support disabled (keyboard only)")
    endif()
endif()

if(MSVC)
    target_compile_options(gecko PRIVATE /W3)
else()
    target_compile_options(gecko PRIVATE -Wall -Wextra)
endif()

# Windows application icon (compiled from resources/gecko.rc when present).
if(WIN32)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/gecko.rc)
        target_sources(gecko PRIVATE resources/gecko.rc)
    endif()
endif()
