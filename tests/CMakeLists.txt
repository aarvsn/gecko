# Test suite: synthetic libretro core + host-side pipeline tests.

# The synthetic core (plain C, exports the libretro ABI).
add_library(gecko-testcore SHARED testcore/testcore.c)
set_source_files_properties(testcore/testcore.c PROPERTIES LANGUAGE CXX)
set_target_properties(gecko-testcore PROPERTIES
        OUTPUT_NAME gecko_testcore
        PREFIX ""
        C_STANDARD 99)
target_include_directories(gecko-testcore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core/include/gecko)

# Host-side tests (gecko-core + the real pipeline).
add_executable(gecko-host-tests host_tests/host_tests.cpp)
target_link_libraries(gecko-host-tests PRIVATE gecko::core)
target_compile_definitions(gecko-host-tests PRIVATE
        GECKO_CORE_SUFFIX="${GECKO_CORE_SUFFIX}"
        )
set_target_properties(gecko-host-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Put the core next to the test binary.
add_custom_command(TARGET gecko-testcore POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:gecko-testcore>
                ${CMAKE_BINARY_DIR}/bin/$<TARGET_FILE_NAME:gecko-testcore>)

# Register with CTest (run `ctest` from the build directory).
add_test(NAME host-tests COMMAND gecko-host-tests
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
