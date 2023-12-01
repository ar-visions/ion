cmake_minimum_required (VERSION 2.6)
project (yuv_rgb)

if(MSVC)
        add_compile_options(/MT)
        set(CMAKE_C_COMPILER   "clang-cl")
	set(CMAKE_CXX_COMPILER "clang-cl")
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall -Wextra -pedantic -std=c99")
endif()

include_directories ("${PROJECT_SOURCE_DIR}")

add_library(yuv2rgb yuv_rgb.c)
install(TARGETS yuv2rgb
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT libraries)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/yuv_rgb.h
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/)