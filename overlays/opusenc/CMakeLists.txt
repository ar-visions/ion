cmake_minimum_required(VERSION 3.10)
project(libopusenc VERSION 0.21 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(opusenc
    src/opusenc.c
    src/opus_header.c
    src/ogg_packer.c
    src/picture.c
    src/unicode_support.c)

target_compile_definitions(opusenc PUBLIC PACKAGE_NAME="opusenc" PACKAGE_VERSION="0.21")

target_include_directories(opusenc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_INSTALL_PREFIX}/include/opus
    ${CMAKE_INSTALL_PREFIX}/include/speex
    ${CMAKE_INSTALL_PREFIX}/include)

target_link_libraries(opusenc PRIVATE opus opusfile)

file(GLOB_RECURSE INC_FILES "${CMAKE_SOURCE_DIR}/include/*.h")
if (NOT INC_FILES)
  message(FATAL_ERROR "no header files found in ${CMAKE_SOURCE_DIR}/include/") # somehow this isnt installing whats there in opusenc
endif()

install(FILES   ${INC_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/opusenc)
install(TARGETS opusenc      DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_compile_options(/MT)
