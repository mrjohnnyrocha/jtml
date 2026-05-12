cmake_minimum_required(VERSION 3.14)
project(jtml VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global build settings
# ---------------------------------------------------------------------------
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(JtmlVersion)

option(JTML_BUILD_PYTHON "Build the optional Python extension (jtml_engine)" ON)
option(JTML_BUILD_TESTS  "Build the unit test suite"                         OFF)

# ---------------------------------------------------------------------------
# External dependencies
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)
find_package(Boost   REQUIRED COMPONENTS system)

# ---------------------------------------------------------------------------
# Third-party headers vendored under third_party/ (nlohmann/json + cpp-httplib).
# WebSocket++ is provided by the system package manager.
# ---------------------------------------------------------------------------
add_library(jtml_third_party INTERFACE)
target_include_directories(jtml_third_party INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
if(EXISTS /opt/homebrew/include)
    target_include_directories(jtml_third_party INTERFACE /opt/homebrew/include)
endif()

# ---------------------------------------------------------------------------
# Core library (lexer, parser, AST, interpreter, transpiler, formatter, linter).
# Public headers live in `include/`; every .cpp lives in `src/`.
# ---------------------------------------------------------------------------
set(JTML_CORE_SOURCES
    src/ast.cpp
    src/lexer.cpp
    src/parser.cpp
    src/diagnostic.cpp
    src/fix.cpp
    src/friendly.cpp
    src/friendly_formatter.cpp
    src/module_resolver.cpp
    src/interpreter.cpp
    src/transpiler.cpp
    src/formatter.cpp
    src/linter.cpp
    src/refactor.cpp
    src/environment.cpp
    src/array.cpp
    src/dict.cpp
    src/value.cpp
)

add_library(jtml_core STATIC ${JTML_CORE_SOURCES})
set_target_properties(jtml_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(jtml_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(jtml_core PUBLIC
    Boost::system
    Threads::Threads
    jtml_third_party
)

# ---------------------------------------------------------------------------
# Stable C ABI for embedding JTML from Python, Node native addons, Go, Rust,
# Swift, C#, Java, IDEs, and other hosts with an FFI boundary.
# ---------------------------------------------------------------------------
add_library(jtml_c SHARED src/c_api.cpp)
set_target_properties(jtml_c PROPERTIES
    OUTPUT_NAME jtml
    POSITION_INDEPENDENT_CODE ON
)
target_link_libraries(jtml_c PUBLIC jtml_core)
jtml_apply_version_defs(jtml_c)

# ---------------------------------------------------------------------------
# CLI binary — each command group has its own translation unit under `cli/`,
# `main.cpp` does only dispatch.
# ---------------------------------------------------------------------------
set(JTML_CLI_SOURCES
    cli/main.cpp
    cli/util.cpp
    cli/diagnostic_json.cpp
    cli/cmd_basic.cpp
    cli/cmd_fix.cpp
    cli/cmd_fmt_lint.cpp
    cli/cmd_lsp.cpp
    cli/cmd_refactor.cpp
    cli/cmd_serve.cpp
    cli/cmd_tutorial.cpp
    cli/studio_shell.cpp
    cli/tutorial_shell.cpp
)

add_executable(jtml_cli ${JTML_CLI_SOURCES})
set_target_properties(jtml_cli PROPERTIES OUTPUT_NAME jtml)
target_link_libraries(jtml_cli PRIVATE jtml_core)
jtml_apply_version_defs(jtml_cli)

# ---------------------------------------------------------------------------
# Optional Python extension
# ---------------------------------------------------------------------------
if(JTML_BUILD_PYTHON)
    find_package(pybind11 CONFIG QUIET)
    if(pybind11_FOUND)
        pybind11_add_module(jtml_engine scripts/jtml_bindings.cpp)
        target_link_libraries(jtml_engine PRIVATE jtml_core)
    else()
        message(STATUS
            "pybind11 not found; skipping jtml_engine. "
            "Configure with -DJTML_BUILD_PYTHON=OFF to silence this.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Unit tests (GoogleTest, fetched at configure time). Opt in with
# `-DJTML_BUILD_TESTS=ON`; CTest picks up all test binaries automatically.
# ---------------------------------------------------------------------------
if(JTML_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install rules (basic; refined in Phase 5 of the roadmap)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS jtml_cli jtml_c
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES include/jtml/c_api.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jtml
)
install(DIRECTORY examples tutorial
    DESTINATION ${CMAKE_INSTALL_DATADIR}/jtml
)
install(DIRECTORY site
    DESTINATION ${CMAKE_INSTALL_DATADIR}/jtml
)
install(FILES README.md ROADMAP.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
install(DIRECTORY docs
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

set(CPACK_PACKAGE_NAME "jtml")
set(CPACK_PACKAGE_VENDOR "JTML")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tiny reactive HTML language and CLI")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "TGZ")
include(CPack)
