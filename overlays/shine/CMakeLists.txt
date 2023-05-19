cmake_minimum_required(VERSION 1.0.2)
project(libshine)

execute_process(COMMAND autoreconf "-i" RESULT_VARIABLE autoreconf WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
execute_process(COMMAND sh "./configure" "--prefix=${CMAKE_INSTALL_PREFIX}"
    RESULT_VARIABLE configure WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

if(NOT configure EQUAL 0)
    message(FATAL_ERROR "configure script failed")
endif()

execute_process(COMMAND make              RESULT_VARIABLE make         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
execute_process(COMMAND make install      RESULT_VARIABLE make_install WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
