# cmake/component_api.cmake
# Component registration and scan API.
# Full specification: docs/component_spec.md

# Internal accumulators — do not use directly.
set(_UL_COMPONENTS       "" CACHE INTERNAL "")
set(_UL_ROOT_THREAD_COMP "" CACHE INTERNAL "")

# ---------------------------------------------------------------------------
# ul_component_register
#
# Called from a component's CMakeLists.txt to register itself with the build.
# Sources and include directories are accumulated into the ulipe_kernel target.
# ---------------------------------------------------------------------------
function(ul_component_register)
    cmake_parse_arguments(
        ARG
        "ROOT_THREAD"
        "NAME;ENABLED;LINKER_FRAGMENT"
        "SOURCES;INCLUDE_DIRS;REQUIRES"
        ${ARGN}
    )

    if(NOT DEFINED ARG_NAME)
        message(FATAL_ERROR "ul_component_register: NAME is required")
    endif()
    if(NOT DEFINED ARG_ENABLED)
        message(FATAL_ERROR
            "ul_component_register: ENABLED is required (component: ${ARG_NAME})")
    endif()

    if(NOT ARG_ENABLED)
        message(STATUS "  [component] ${ARG_NAME} DISABLED")
        set_property(GLOBAL PROPERTY "UL_COMP_${ARG_NAME}_ENABLED" 0)
        return()
    endif()

    # REQUIRES: error if any dependency was explicitly registered as DISABLED.
    # (Dependencies not yet seen are silently skipped — resolved at link time.)
    foreach(_dep IN LISTS ARG_REQUIRES)
        get_property(_dep_state GLOBAL PROPERTY "UL_COMP_${_dep}_ENABLED")
        if(DEFINED _dep_state AND "${_dep_state}" STREQUAL "0")
            message(FATAL_ERROR
                "Component '${ARG_NAME}' requires '${_dep}' which is DISABLED")
        endif()
    endforeach()

    message(STATUS "  [component] ${ARG_NAME} ENABLED")
    set_property(GLOBAL PROPERTY "UL_COMP_${ARG_NAME}_ENABLED" 1)
    set(_UL_COMPONENTS "${_UL_COMPONENTS};${ARG_NAME}" CACHE INTERNAL "")

    foreach(_src IN LISTS ARG_SOURCES)
        target_sources(ulipe_kernel PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    endforeach()

    foreach(_dir IN LISTS ARG_INCLUDE_DIRS)
        target_include_directories(ulipe_kernel PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}")
    endforeach()

    if(ARG_ROOT_THREAD)
        if(NOT "${_UL_ROOT_THREAD_COMP}" STREQUAL "")
            message(FATAL_ERROR
                "Multiple components declare ROOT_THREAD: "
                "${_UL_ROOT_THREAD_COMP} and ${ARG_NAME}")
        endif()
        set(_UL_ROOT_THREAD_COMP "${ARG_NAME}" CACHE INTERNAL "")
    endif()

    if(DEFINED ARG_LINKER_FRAGMENT)
        message(STATUS
            "  [component] ${ARG_NAME}: linker fragment ${ARG_LINKER_FRAGMENT}")
        # Linker fragment accumulation — future use.
        # For now, the fragment must be manually included in generate_ld.py.
    endif()
endfunction()

# ---------------------------------------------------------------------------
# ul_components_finalize
#
# Call after all component scans to validate the ROOT_THREAD invariant
# and print a summary.  Called from the top-level CMakeLists.txt.
# ---------------------------------------------------------------------------
function(ul_components_finalize)
    if("${_UL_ROOT_THREAD_COMP}" STREQUAL "")
        message(STATUS
            "  [component] No ROOT_THREAD component found — "
            "stub/root_thread_stub.c will be used")
    else()
        message(STATUS
            "  [component] ROOT_THREAD: ${_UL_ROOT_THREAD_COMP}")
    endif()
endfunction()
