cmake_minimum_required(VERSION 3.22)

project(mordor
    VERSION 0.1.0
    LANGUAGES C CXX
)

# ---------------------------------------------------------------------------
# Global standards
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED   ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS   OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

# Out-of-source builds only.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Run cmake from a build/ subdirectory.")
endif()

# Export compile commands for clangd and static analysers.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional static analysis integration.
option(MORDOR_ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)
option(MORDOR_ENABLE_CPPCHECK "Run cppcheck during compilation" OFF)

if(MORDOR_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_C_CLANG_TIDY
            ${CLANG_TIDY_EXE}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
        )
        set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY_EXE}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
        )
        message(STATUS "clang-tidy enabled for C/C++: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "MORDOR_ENABLE_CLANG_TIDY=ON but clang-tidy was not found; disabling")
        set(MORDOR_ENABLE_CLANG_TIDY OFF CACHE BOOL "Run clang-tidy during compilation" FORCE)
    endif()
endif()

if(MORDOR_ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES cppcheck)
    if(CPPCHECK_EXE)
        set(CMAKE_C_CPPCHECK
            ${CPPCHECK_EXE}
            --enable=warning,performance,portability
            --inline-suppr
            --suppress=missingIncludeSystem
        )
        set(CMAKE_CXX_CPPCHECK
            ${CPPCHECK_EXE}
            --enable=warning,performance,portability
            --inline-suppr
            --suppress=missingIncludeSystem
        )
        message(STATUS "cppcheck enabled for C/C++: ${CPPCHECK_EXE}")
    else()
        message(WARNING "MORDOR_ENABLE_CPPCHECK=ON but cppcheck was not found; disabling")
        set(MORDOR_ENABLE_CPPCHECK OFF CACHE BOOL "Run cppcheck during compilation" FORCE)
    endif()
endif()

# Build type default.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Build type not set — defaulting to Debug")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# ---------------------------------------------------------------------------
# Targets
# Add further subdirectories here when new libraries or tools are introduced.
# ---------------------------------------------------------------------------
add_subdirectory(source)
