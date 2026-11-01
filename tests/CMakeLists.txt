# tests/CMakeLists.txt
#
# Fetches GoogleTest at configure time so `cmake -B build -DJTML_BUILD_TESTS=ON`
# works on a fresh checkout with no extra setup. Each test binary links against
# jtml_core (and jtml_third_party transitively) so it has access to the full
# public API.

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Prevent GTest from clobbering the parent project's options.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(JTML_TEST_SOURCES
    test_lexer.cpp
    test_attribute_classifier.cpp
    test_parser.cpp
    test_interpreter_dependencies.cpp
    test_interpreter_components.cpp
    test_interpreter_logical.cpp
    test_interpreter_unary.cpp
    test_diagnostics.cpp
    test_c_api.cpp
    test_cli_commands.cpp
    test_local_server_security.cpp
    test_fix.cpp
    test_formatter.cpp
    test_friendly.cpp
    test_linter.cpp
    test_source_origin.cpp
    test_source_propagation.cpp
    test_module_resolver.cpp
    test_refactor.cpp
    test_semantic.cpp
    test_static_html.cpp
    test_studio_samples.cpp
    test_tutorial.cpp
    test_friendly_formatter.cpp
)

add_executable(jtml_tests ${JTML_TEST_SOURCES})
target_link_libraries(jtml_tests PRIVATE
    jtml_c
    jtml_cli_lib
    GTest::gtest_main
)
target_include_directories(jtml_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_compile_definitions(jtml_tests PRIVATE
    JTML_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

include(GoogleTest)
# `gtest_discover_tests` runs the binary once at build time to enumerate cases
# into CTest. DISCOVERY_TIMEOUT is generous because the binary may link to a
# large number of static symbols on first run.
gtest_discover_tests(jtml_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS "unit"
)

add_test(NAME BrowserProduction.LinkcheckWithoutLiveOrLegacy
    COMMAND jtml_browser_production_linkcheck
)
set_tests_properties(BrowserProduction.LinkcheckWithoutLiveOrLegacy
    PROPERTIES LABELS "unit;architecture"
)

add_test(NAME BrowserBuild.LinkcheckWithoutAggregateCore
    COMMAND jtml_browser_build_linkcheck
)
set_tests_properties(BrowserBuild.LinkcheckWithoutAggregateCore
    PROPERTIES LABELS "unit;architecture"
)

add_executable(jtml_function_call_probe interpreter_call_probe.cpp)
target_link_libraries(jtml_function_call_probe PRIVATE jtml_core)
foreach(mode simple nested recursion excessive independent concurrent)
    add_test(NAME InterpreterCalls.${mode} COMMAND jtml_function_call_probe ${mode})
    set_tests_properties(InterpreterCalls.${mode} PROPERTIES TIMEOUT 10 LABELS "unit;interpreter")
endforeach()

add_executable(jtml_async_boundary_probe interpreter_async_boundary_probe.cpp)
target_link_libraries(jtml_async_boundary_probe PRIVATE jtml_core)
add_test(NAME InterpreterAsync.boundary COMMAND jtml_async_boundary_probe)
set_tests_properties(InterpreterAsync.boundary PROPERTIES TIMEOUT 15 LABELS "unit;interpreter;async")

add_executable(jtml_async_probe interpreter_async_probe.cpp)
target_link_libraries(jtml_async_probe PRIVATE jtml_core)
foreach(mode execute cancel reset reload snapshots callbacks errors shutdown repeated early-stop cross-wait)
    add_test(NAME InterpreterAsync.${mode} COMMAND jtml_async_probe ${mode})
    set_tests_properties(InterpreterAsync.${mode} PROPERTIES TIMEOUT 20 LABELS "unit;interpreter;async")
endforeach()

add_executable(jtml_async_c_api_probe c_api_async_probe.cpp)
target_link_libraries(jtml_async_c_api_probe PRIVATE jtml_c)
add_test(NAME InterpreterAsync.c_api_lifetime COMMAND jtml_async_c_api_probe)
set_tests_properties(InterpreterAsync.c_api_lifetime PROPERTIES TIMEOUT 20 LABELS "unit;interpreter;async")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(jtml_startup_logging_probe server_startup_logging_probe.cpp)
target_link_libraries(jtml_startup_logging_probe PRIVATE jtml_cli_lib)
target_include_directories(jtml_startup_logging_probe PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME LocalServerStartup.BannerSynchronization
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_server_startup_logging.py
            $<TARGET_FILE:jtml_startup_logging_probe>)
set_tests_properties(LocalServerStartup.BannerSynchronization PROPERTIES TIMEOUT 50 LABELS "unit;interpreter;security")

add_executable(jtml_conformance_probe conformance_probe.cpp)
target_link_libraries(jtml_conformance_probe PRIVATE jtml_cli_lib)
target_include_directories(jtml_conformance_probe PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME SemanticConformance.OracleControls
    COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/conformance/self_test.cjs)
set_tests_properties(SemanticConformance.OracleControls PROPERTIES TIMEOUT 15 LABELS "unit;conformance")

add_test(NAME Release.VersionMapping COMMAND ${CMAKE_COMMAND}
    -DSOURCE=${CMAKE_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/test_release_version.cmake)
add_test(NAME Release.ArchiveReproducibility COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_release_archive.py ${CMAKE_SOURCE_DIR}/scripts/create_release_archive.py)
set_tests_properties(Release.VersionMapping Release.ArchiveReproducibility
    PROPERTIES TIMEOUT 15 LABELS "unit;release")
