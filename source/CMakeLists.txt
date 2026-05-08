# ---------------------------------------------------------------------------
# Mordor — main executable
#
# Source discovery uses GLOB_RECURSE + CONFIGURE_DEPENDS so new .cpp/.c files
# under src/ are picked up automatically at the next configure step.
# Only edit this file when introducing a NEW target (library, tool, test runner).
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(FetchContent)
find_package(OpenGL QUIET)

# Logging — quill (decision: docs/decisions/0001-logging-library.md)
# Engine code must not include quill headers directly; use mordor/log.hpp only.
# To upgrade: change QUILL_VERSION and clear the build cache.
set(QUILL_VERSION "v11.1.0")
FetchContent_Declare(quill
    GIT_REPOSITORY https://github.com/odygrd/quill.git
    GIT_TAG        ${QUILL_VERSION}
    GIT_SHALLOW    TRUE
)
# Disable quill's own tests, benchmarks, and examples to keep build fast.
set(QUILL_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(QUILL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(quill)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# On Linux, GLFW requires at least one platform backend (X11 or Wayland).
# Detect which are available and enable only those.  On Windows and macOS
# GLFW selects the native backend automatically so we always fetch it.
set(MORDOR_HAS_GLFW 1)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 QUIET)
    find_package(Wayland QUIET)
    if(NOT X11_FOUND AND NOT Wayland_FOUND)
        message(WARNING
            "Neither X11 nor Wayland dev packages found on Linux; "
            "renderer window bootstrap will run in stub mode")
        set(MORDOR_HAS_GLFW 0)
    else()
        if(X11_FOUND)
            message(STATUS "X11 dev packages found; enabling GLFW X11 backend")
            set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
        else()
            set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
        endif()
        if(Wayland_FOUND)
            message(STATUS "Wayland dev packages found; enabling GLFW Wayland backend")
            set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
        else()
            set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        endif()
    endif()
endif()

if(MORDOR_HAS_GLFW)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glfw)
endif()

# Suppress warnings from quill's own headers — they shadow their own members
# under -Wshadow. quill::quill is an ALIAS so we target the real target.
get_target_property(_quill_includes quill INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(quill PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_quill_includes}"
)

file(GLOB_RECURSE MORDOR_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

file(GLOB_RECURSE MORDOR_HEADERS
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

set(MORDOR_ALL_CODE_FILES
    ${MORDOR_SOURCES}
    ${MORDOR_HEADERS}
)

add_executable(mordor ${MORDOR_SOURCES})

target_include_directories(mordor
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(mordor
    PRIVATE
        quill::quill
)

if(MORDOR_HAS_GLFW)
    target_link_libraries(mordor PRIVATE glfw)
endif()

if(OpenGL_FOUND AND MORDOR_HAS_GLFW)
    target_link_libraries(mordor PRIVATE OpenGL::GL)
    target_compile_definitions(mordor PRIVATE MORDOR_HAS_OPENGL=1)
else()
    message(WARNING "OpenGL or GLFW dependencies missing; renderer will build in stub mode")
    target_compile_definitions(mordor PRIVATE MORDOR_HAS_OPENGL=0)
endif()
target_compile_definitions(mordor PRIVATE MORDOR_HAS_GLFW=${MORDOR_HAS_GLFW})

# Expose the CMake build type as a compile-time string constant.
target_compile_definitions(mordor
    PRIVATE
        MORDOR_BUILD_TYPE="$<CONFIG>"
)

if(MSVC)
    target_compile_options(mordor PRIVATE /W4 /WX)
else()
    target_compile_options(mordor PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        $<$<CONFIG:Release>:-O2>
    )
endif()

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${MORDOR_ALL_CODE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on source/include"
        VERBATIM
    )
else()
    message(WARNING "clang-format not found: 'format' target will be a no-op")
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found; skipping format step"
        COMMENT "No-op format target (clang-format unavailable)"
        VERBATIM
    )
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_EXE}
                --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
                -p ${CMAKE_BINARY_DIR}
                ${MORDOR_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on sources"
        VERBATIM
    )
else()
    message(WARNING "clang-tidy not found: 'lint' target will be a no-op")
    add_custom_target(lint
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found; skipping lint step"
        COMMENT "No-op lint target (clang-tidy unavailable)"
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    add_executable(mordor_unit_tests
        tests/unit_tests.cpp
        src/ability_pipeline.cpp
        src/components.cpp
        src/fog_of_war.cpp
        src/hearing.cpp
        src/inventory_pipeline.cpp
        src/interactions.cpp
        src/key_switch.cpp
        src/occupancy.cpp
        src/party_commands.cpp
        src/perception_debug.cpp
        src/visibility.cpp
    )

    target_include_directories(mordor_unit_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    if(MSVC)
        target_compile_options(mordor_unit_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(mordor_unit_tests PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
            $<$<CONFIG:Release>:-O2>
        )
    endif()

    add_test(NAME mordor_unit_tests COMMAND mordor_unit_tests)
endif()

install(TARGETS mordor RUNTIME DESTINATION bin)
