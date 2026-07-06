# cmake/component_api.cmake
# Component registration and scan API.
# Full specification: docs/component_spec.md

# Internal accumulators — do not use directly.
set(_ULMK_COMPONENTS       "" CACHE INTERNAL "")
set(_ULMK_ALL_COMPONENTS   "" CACHE INTERNAL "")

# ---------------------------------------------------------------------------
# ulmk_component_register
#
# Called from a component's CMakeLists.txt to register itself with the build.
#
# Each enabled component is compiled as a separate static library
# (ulmk_comp_<name>) so that its code and data land in dedicated linker
# sections, isolated from the kernel archive (libulmk_kernel.a).
#
# ENABLED in the manifest is the default when no ULMK_COMP_<name>_ENABLED cache
# entry exists.  dev.py passes -DULMK_COMP_<name>_ENABLED=ON|OFF to override.
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

    set(_cache_var "ULMK_COMP_${ARG_NAME}_ENABLED")
    if(NOT DEFINED ${_cache_var})
        if(ARG_ENABLED)
            set(${_cache_var} ON CACHE BOOL "Enable component ${ARG_NAME}")
        else()
            set(${_cache_var} OFF CACHE BOOL "Enable component ${ARG_NAME}")
        endif()
    endif()

    set(tmp "${_ULMK_ALL_COMPONENTS}")
    if(NOT "${ARG_NAME}" IN_LIST tmp)
        list(APPEND tmp "${ARG_NAME}")
        set(_ULMK_ALL_COMPONENTS "${tmp}" CACHE INTERNAL "")
    endif()

    if(ARG_REQUIRES)
        set_property(GLOBAL PROPERTY "ULMK_COMP_${ARG_NAME}_REQUIRES"
                     "${ARG_REQUIRES}")
    endif()

    if(ARG_ROOT_THREAD)
        set_property(GLOBAL PROPERTY "ULMK_COMP_${ARG_NAME}_ROOT_THREAD" 1)
    endif()

    if(ARG_ENABLED)
        set_property(GLOBAL PROPERTY "ULMK_COMP_${ARG_NAME}_MANIFEST" 1)
    else()
        set_property(GLOBAL PROPERTY "ULMK_COMP_${ARG_NAME}_MANIFEST" 0)
    endif()

    if(NOT ${${_cache_var}})
        message(STATUS "  [component] ${ARG_NAME} DISABLED")
        set_property(GLOBAL PROPERTY "ULMK_COMP_${ARG_NAME}_ENABLED" 0)
        return()
    endif()

    message(STATUS "  [component] ${ARG_NAME} ENABLED")
    set_property(GLOBAL PROPERTY "ULMK_COMP_${ARG_NAME}_ENABLED" 1)
    set(tmp "${_ULMK_COMPONENTS}")
    list(APPEND tmp "${ARG_NAME}")
    set(_ULMK_COMPONENTS "${tmp}" CACHE INTERNAL "")

    set(_tgt "ulmk_comp_${ARG_NAME}")
    add_library("${_tgt}" STATIC)

    foreach(_src IN LISTS ARG_SOURCES)
        target_sources("${_tgt}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    endforeach()

    foreach(_dir IN LISTS ARG_INCLUDE_DIRS)
        target_include_directories("${_tgt}" PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}")
    endforeach()

    target_link_libraries("${_tgt}" PRIVATE ulmk_kernel)

    target_compile_definitions("${_tgt}" PRIVATE
        ULMK_MODULE_NAME=${ARG_NAME}
        ULMK_APP_NAME=${ARG_NAME})

    if(DEFINED ULMK_BOARD_CFLAGS)
        target_compile_options("${_tgt}" PRIVATE ${ULMK_BOARD_CFLAGS})
    endif()

    ulmk_add_domain("${ARG_NAME}" REGION KERNEL_RAM)

    if(DEFINED ARG_LINKER_FRAGMENT)
        message(STATUS
            "  [component] ${ARG_NAME}: linker fragment ${ARG_LINKER_FRAGMENT}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# ulmk_components_finalize
#
# Validates REQUIRES / ROOT_THREAD among enabled components and wires
# inter-component library dependencies.
# ---------------------------------------------------------------------------
function(ulmk_components_finalize)
    set(_root_thread_comp "")

    foreach(_name IN LISTS _ULMK_ALL_COMPONENTS)
        get_property(_enabled GLOBAL PROPERTY "ULMK_COMP_${_name}_ENABLED")
        if(NOT _enabled)
            continue()
        endif()

        get_property(_requires GLOBAL PROPERTY "ULMK_COMP_${_name}_REQUIRES")
        if(_requires)
            foreach(_dep IN LISTS _requires)
                get_property(_dep_known GLOBAL PROPERTY
                             "ULMK_COMP_${_dep}_MANIFEST" SET)
                if(NOT _dep_known)
                    message(FATAL_ERROR
                        "Component '${_name}' requires unknown component "
                        "'${_dep}'.")
                endif()

                get_property(_dep_en GLOBAL PROPERTY
                             "ULMK_COMP_${_dep}_ENABLED")
                if(NOT _dep_en)
                    message(FATAL_ERROR
                        "Component '${_name}' requires '${_dep}', but "
                        "'${_dep}' is DISABLED.\n"
                        "  Enable it:  python3 tools/dev.py components "
                        "enable ${_dep}\n"
                        "  Or build:   python3 tools/dev.py build "
                        "--component ${_dep}\n"
                        "  Or remove REQUIRES / ${_dep}_init() from "
                        "${_name}.")
                endif()

                target_link_libraries("ulmk_comp_${_name}" PRIVATE
                                      "ulmk_comp_${_dep}")
            endforeach()
        endif()

        get_property(_rt GLOBAL PROPERTY "ULMK_COMP_${_name}_ROOT_THREAD")
        if(_rt)
            if(NOT "${_root_thread_comp}" STREQUAL "")
                message(FATAL_ERROR
                    "Multiple enabled components declare ROOT_THREAD: "
                    "${_root_thread_comp} and ${_name}")
            endif()
            set(_root_thread_comp "${_name}")
        endif()
    endforeach()

    if("${_root_thread_comp}" STREQUAL "")
        message(STATUS
            "  [component] No ROOT_THREAD component enabled — "
            "stub/root_thread_stub.c will be used")
    else()
        message(STATUS
            "  [component] ROOT_THREAD: ${_root_thread_comp}")
    endif()

    set(_ULMK_ROOT_THREAD_COMP "${_root_thread_comp}" CACHE INTERNAL "")
endfunction()
