# Derive compile-time version macros from the top-level project() call.
# Release builds should pass `-DJTML_VERSION_SUFFIX=""`; the default is
# "dev" so local builds are unambiguous.
if(NOT DEFINED JTML_VERSION_SUFFIX)
    set(JTML_VERSION_SUFFIX "dev")
endif()

if(NOT JTML_VERSION_SUFFIX STREQUAL "" AND NOT JTML_VERSION_SUFFIX MATCHES "^[A-Za-z0-9-]+(\\.[A-Za-z0-9-]+)*$")
    message(FATAL_ERROR "JTML_VERSION_SUFFIX must be empty or dot-separated prerelease identifiers")
endif()
set(JTML_FULL_VERSION "${PROJECT_VERSION}")
if(NOT JTML_VERSION_SUFFIX STREQUAL "")
    string(APPEND JTML_FULL_VERSION "-${JTML_VERSION_SUFFIX}")
endif()

function(jtml_apply_version_defs target)
    target_compile_definitions(${target} PRIVATE
        JTML_VERSION_STRING="${PROJECT_VERSION}"
        JTML_VERSION_SUFFIX_STRING="${JTML_VERSION_SUFFIX}"
    )
endfunction()
