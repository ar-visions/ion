cmake_minimum_required(VERSION 3.20)
project(moltenvk)

set(TARGET_OS "macos")

execute_process(
    COMMAND sh "fetchDependencies" "--${TARGET_OS}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE sh_res)

set(dbg "")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(dbg "-debug")
endif()

get_filename_component(INSTALL_DIR ${CMAKE_SOURCE_DIR}/../install ABSOLUTE)
set(PREFIX_DIR  ${INSTALL_DIR})
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/MoltenVK/include)

file(MAKE_DIRECTORY ${PREFIX_DIR})
set(ENV{PREFIX} ${PREFIX_DIR})

execute_process(
    COMMAND make "${TARGET_OS}${dbg}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE make_res)

execute_process(
    COMMAND make install "${TARGET_OS}${dbg}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE make_res)

if(NOT make_res EQUAL 0)
    message(FATAL_ERROR "make res is not 0, exiting with a non-zero myself")
endif()