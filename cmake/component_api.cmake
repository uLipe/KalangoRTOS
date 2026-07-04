# cmake/component_api.cmake
# Component registration and scan API.
# Full specification: docs/component_spec.md

# Internal accumulators — do not use directly.
set(_ULMK_COMPONENTS       "" CACHE INTERNAL "")
set(_ULMK_ROOT_THREAD_COMP "" CACHE INTERNAL "")

# ---------------------------------------------------------------------------
# ulmk_component_register
#
# Called from a component's CMakeLists.txt to register itself with the build.
#
# Each enabled component is compiled as a separate static library
# (ulmk_comp_<name>) so that its code and data land in dedicated linker
# sections, isolated from the kernel archive (libulmk_kernel.a).
#
# The component library is compiled with:
#   -DULMK_MODULE_NAME=<name>  so ULMK_PRIVATE places data in the right domain
#
# A data domain (domain_data.ld.in snippet) is automatically registered for
# every enabled component via ulmk_add_domain().
# ---------------------------------------------------------------------------
function(ulmk_component_register)
    cmake_parse_arguments(
        ARG
        "ROOT_THREAD"
        "NAME;ENABLED;LINKER_FRAGMENT"
        "SOURCES;INCLUDE_DIRS;REQUIRES"
        ${ARGN}
    )

    if(NOT DEFINED ARG_NAME)
        message(FATAL_ERROR "ulmk_component_register: NAME is required")
    endif()
    if(NOT DEFINED ARG_ENABLED)
        message(FATAL_ERROR
            "ulmk_component_register: ENABLED is required (component: ${ARG_NAME})")
    endif()

    if(NOT ARG_ENABLED)
        message(STATUS "  [component] ${ARG_NAME} DISABLED")
        set_property(GLOBAL PROPERTY "ULMK_COMP_${ARG_NAME}_ENABLED" 0)
        return()
    endif()

    # REQUIRES: error if any dependency was explicitly registered as DISABLED.
    foreach(_dep IN LISTS ARG_REQUIRES)
        get_property(_dep_state GLOBAL PROPERTY "ULMK_COMP_${_dep}_ENABLED")
        if(DEFINED _dep_state AND "${_dep_state}" STREQUAL "0")
            message(FATAL_ERROR
                "Component '${ARG_NAME}' requires '${_dep}' which is DISABLED")
        endif()
    endforeach()

    message(STATUS "  [component] ${ARG_NAME} ENABLED")
    set_property(GLOBAL PROPERTY "ULMK_COMP_${ARG_NAME}_ENABLED" 1)
    set(tmp "${_ULMK_COMPONENTS}")
    list(APPEND tmp "${ARG_NAME}")
    set(_ULMK_COMPONENTS "${tmp}" CACHE INTERNAL "")

    # Create a dedicated static library for this component.
    set(_tgt "ulmk_comp_${ARG_NAME}")
    add_library("${_tgt}" STATIC)

    foreach(_src IN LISTS ARG_SOURCES)
        target_sources("${_tgt}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    endforeach()

    foreach(_dir IN LISTS ARG_INCLUDE_DIRS)
        target_include_directories("${_tgt}" PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}")
    endforeach()

    # Inherit public include dirs from the kernel (include/, arch/, generated/).
    # PRIVATE linkage: include dirs propagate to this target's compilation only;
    # no circular dependency is created at archive level.
    target_link_libraries("${_tgt}" PRIVATE ulmk_kernel)

    # Compile-time defines: module name (for ULMK_PRIVATE) and app name (for
    # UL_APP_TEXT / section routing in the linker script).
    target_compile_definitions("${_tgt}" PRIVATE
        ULMK_MODULE_NAME=${ARG_NAME}
        ULMK_APP_NAME=${ARG_NAME})

    # Propagate board-specific compile flags (e.g. -DULMK_ARCH_QEMU_VIRT_CONSOLE).
    if(DEFINED ULMK_BOARD_CFLAGS)
        target_compile_options("${_tgt}" PRIVATE ${ULMK_BOARD_CFLAGS})
    endif()

    # Auto-register a data domain so generate_ld.py emits a domain_data snippet.
    ulmk_add_domain("${ARG_NAME}" REGION KERNEL_RAM)

    if(ARG_ROOT_THREAD)
        if(NOT "${_ULMK_ROOT_THREAD_COMP}" STREQUAL "")
            message(FATAL_ERROR
                "Multiple components declare ROOT_THREAD: "
                "${_ULMK_ROOT_THREAD_COMP} and ${ARG_NAME}")
        endif()
        set(_ULMK_ROOT_THREAD_COMP "${ARG_NAME}" CACHE INTERNAL "")
    endif()

    if(DEFINED ARG_LINKER_FRAGMENT)
        message(STATUS
            "  [component] ${ARG_NAME}: linker fragment ${ARG_LINKER_FRAGMENT}")
        # Linker fragment accumulation — future use.
    endif()
endfunction()

# ---------------------------------------------------------------------------
# ulmk_components_finalize
#
# Call after all component scans to validate the ROOT_THREAD invariant
# and print a summary.  Called from the top-level CMakeLists.txt.
# ---------------------------------------------------------------------------
function(ulmk_components_finalize)
    if("${_ULMK_ROOT_THREAD_COMP}" STREQUAL "")
        message(STATUS
            "  [component] No ROOT_THREAD component found — "
            "stub/root_thread_stub.c will be used")
    else()
        message(STATUS
            "  [component] ROOT_THREAD: ${_ULMK_ROOT_THREAD_COMP}")
    endif()
endfunction()
