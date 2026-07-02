cmake_minimum_required(VERSION 3.20)

file(GLOB _drv_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(_dir IN LISTS _drv_dirs)
	if(IS_DIRECTORY "${_dir}" AND EXISTS "${_dir}/CMakeLists.txt")
		get_filename_component(_dname "${_dir}" NAME)
		add_subdirectory("${_dir}" "${CMAKE_BINARY_DIR}/drivers/${_dname}")
	endif()
endforeach()
