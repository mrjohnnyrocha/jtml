# Derive compile-time version macros from the top-level project() call.
# Release builds should pass `-DJTML_VERSION_SUFFIX=""`; the default is
# "dev" so local builds are unambiguous.
if(NOT DEFINED JTML_VERSION_SUFFIX)
    set(JTML_VERSION_SUFFIX "dev")
endif()

function(jtml_apply_version_defs target)
    target_compile_definitions(${target} PRIVATE
        JTML_VERSION_STRING="${PROJECT_VERSION}"
        JTML_VERSION_SUFFIX_STRING="${JTML_VERSION_SUFFIX}"
    )
endfunction()
