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
# Core library ownership groups. v0.2 production hardening keeps jtml_core as
# the stable aggregate ABI, but compiles each architecture slice through its
# own object target so browser production work cannot keep hiding inside a
# catch-all runtime bucket.
# ---------------------------------------------------------------------------
set(JTML_SYNTAX_SOURCES
    src/ast.cpp
    src/lexer.cpp
    src/parser.cpp
    src/friendly.cpp
    src/friendly_formatter.cpp
    src/expression_source.cpp
    src/expression_ir.cpp
    src/formatter.cpp
)

set(JTML_SEMANTIC_SOURCES
    src/attribute_classifier.cpp
    src/semantic.cpp
    src/semantic_project_json.cpp
    src/jtml/semantic/module_graph.cpp
)

set(JTML_BROWSER_PRODUCTION_SOURCES
    src/transpiler.cpp
    src/browser_runtime_emitter.cpp
    src/browser_runtime_assets.cpp
    src/client_manifest_emitter.cpp
    src/runtime_plan.cpp
    src/runtime_plan_json.cpp
    src/jtml/runtime/static_update_plan_emitter.cpp
)

set(JTML_LIVE_RUNTIME_SOURCES
    src/interpreter.cpp
)

set(JTML_LEGACY_RUNTIME_SOURCES
    src/jtml/legacy_runtime/environment.cpp
    src/jtml/legacy_runtime/array.cpp
    src/jtml/legacy_runtime/dict.cpp
    src/jtml/legacy_runtime/value.cpp
)

set(JTML_TOOLING_SOURCES
    src/diagnostic.cpp
    src/fix.cpp
    src/language_catalog.cpp
    src/module_resolver.cpp
    src/linter.cpp
    src/refactor.cpp
)

set(JTML_RUNTIME_SOURCES
    ${JTML_BROWSER_PRODUCTION_SOURCES}
    ${JTML_LIVE_RUNTIME_SOURCES}
    ${JTML_LEGACY_RUNTIME_SOURCES}
)

set(JTML_CORE_SOURCES
    ${JTML_SYNTAX_SOURCES}
    ${JTML_SEMANTIC_SOURCES}
    ${JTML_RUNTIME_SOURCES}
    ${JTML_TOOLING_SOURCES}
)

function(jtml_configure_core_target target_name)
    set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(${target_name} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(${target_name} PUBLIC
        Boost::system
        Threads::Threads
        jtml_third_party
    )
endfunction()

add_library(jtml_syntax_obj OBJECT ${JTML_SYNTAX_SOURCES})
jtml_configure_core_target(jtml_syntax_obj)

add_library(jtml_semantic_obj OBJECT ${JTML_SEMANTIC_SOURCES})
jtml_configure_core_target(jtml_semantic_obj)

add_library(jtml_browser_production_obj OBJECT ${JTML_BROWSER_PRODUCTION_SOURCES})
jtml_configure_core_target(jtml_browser_production_obj)

add_library(jtml_live_runtime_obj OBJECT ${JTML_LIVE_RUNTIME_SOURCES})
jtml_configure_core_target(jtml_live_runtime_obj)

add_library(jtml_legacy_runtime_obj OBJECT ${JTML_LEGACY_RUNTIME_SOURCES})
jtml_configure_core_target(jtml_legacy_runtime_obj)

add_library(jtml_tooling_obj OBJECT ${JTML_TOOLING_SOURCES})
jtml_configure_core_target(jtml_tooling_obj)

add_library(jtml_core STATIC
    $<TARGET_OBJECTS:jtml_syntax_obj>
    $<TARGET_OBJECTS:jtml_semantic_obj>
    $<TARGET_OBJECTS:jtml_browser_production_obj>
    $<TARGET_OBJECTS:jtml_live_runtime_obj>
    $<TARGET_OBJECTS:jtml_legacy_runtime_obj>
    $<TARGET_OBJECTS:jtml_tooling_obj>
)
jtml_configure_core_target(jtml_core)

add_library(jtml_browser_production STATIC
    $<TARGET_OBJECTS:jtml_syntax_obj>
    $<TARGET_OBJECTS:jtml_semantic_obj>
    $<TARGET_OBJECTS:jtml_browser_production_obj>
    $<TARGET_OBJECTS:jtml_tooling_obj>
)
jtml_configure_core_target(jtml_browser_production)

set(JTML_CLI_COMMON_SOURCES
    cli/util.cpp
    cli/module_loader.cpp
)

add_library(jtml_cli_common STATIC ${JTML_CLI_COMMON_SOURCES})
jtml_configure_core_target(jtml_cli_common)

add_library(jtml_browser_build_lib STATIC
    cli/browser_build.cpp
)
target_link_libraries(jtml_browser_build_lib PUBLIC
    jtml_browser_production
    jtml_cli_common
)

if(JTML_BUILD_TESTS)
    add_executable(jtml_browser_production_linkcheck
        tests/browser_production_linkcheck.cpp
    )
    target_link_libraries(jtml_browser_production_linkcheck PRIVATE
        jtml_browser_production
    )

    add_executable(jtml_browser_build_linkcheck
        tests/browser_build_linkcheck.cpp
    )
    target_link_libraries(jtml_browser_build_linkcheck PRIVATE
        jtml_browser_build_lib
    )
endif()

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
set(JTML_CLI_LIB_SOURCES
    cli/package_manager.cpp
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

add_library(jtml_cli_lib STATIC ${JTML_CLI_LIB_SOURCES})
target_link_libraries(jtml_cli_lib PUBLIC
    jtml_core
    jtml_cli_common
    jtml_browser_build_lib
)

add_executable(jtml_cli cli/main.cpp)
set_target_properties(jtml_cli PROPERTIES OUTPUT_NAME jtml)
target_link_libraries(jtml_cli PRIVATE jtml_cli_lib)
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
