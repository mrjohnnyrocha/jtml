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
    test_parser.cpp
    test_diagnostics.cpp
    test_c_api.cpp
    test_fix.cpp
    test_formatter.cpp
    test_friendly.cpp
    test_linter.cpp
    test_module_resolver.cpp
    test_refactor.cpp
    test_tutorial.cpp
    test_friendly_formatter.cpp
)

add_executable(jtml_tests ${JTML_TEST_SOURCES})
target_link_libraries(jtml_tests PRIVATE
    jtml_core
    jtml_c
    GTest::gtest_main
)
target_include_directories(jtml_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

include(GoogleTest)
# `gtest_discover_tests` runs the binary once at build time to enumerate cases
# into CTest. DISCOVERY_TIMEOUT is generous because the binary may link to a
# large number of static symbols on first run.
gtest_discover_tests(jtml_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS "unit"
)
