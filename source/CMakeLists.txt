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
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_EXE}
                --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
                -p ${CMAKE_BINARY_DIR}
                ${MORDOR_ALL_CODE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on source/include"
        VERBATIM
    )
endif()

install(TARGETS mordor RUNTIME DESTINATION bin)
