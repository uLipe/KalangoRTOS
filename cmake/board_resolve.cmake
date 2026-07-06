# cmake/board_resolve.cmake — resolve board + arch before project().
# Included from toolchain-*.cmake (CMAKE_TOOLCHAIN_FILE runs pre-project).

if(CMAKE_SOURCE_DIR MATCHES "CMakeScratch")
	return()
endif()

get_filename_component(_ULMK_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT DEFINED ULMK_CHIP_DIR)
	set(ULMK_CHIP_DIR "${_ULMK_REPO_ROOT}/boards/qemu_tc3xx"
	    CACHE PATH "Path to board directory containing memory.ld")
endif()

get_filename_component(ULMK_CHIP_DIR "${ULMK_CHIP_DIR}" ABSOLUTE
	BASE_DIR "${CMAKE_SOURCE_DIR}")

if(EXISTS "${ULMK_CHIP_DIR}/board.cmake")
	include("${ULMK_CHIP_DIR}/board.cmake")
else()
	message(FATAL_ERROR "board.cmake not found in ${ULMK_CHIP_DIR}")
endif()

include("${_ULMK_REPO_ROOT}/cmake/arch.cmake")
