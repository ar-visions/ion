cmake_minimum_required(VERSION 3.26)
project(libspeexdsp)

execute_process(COMMAND sh "./autogen.sh" RESULT_VARIABLE autogen   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
execute_process(COMMAND sh "./configure" "--prefix=${CMAKE_INSTALL_PREFIX}"
    RESULT_VARIABLE configure WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
execute_process(COMMAND make              RESULT_VARIABLE make      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
execute_process(COMMAND make install      RESULT_VARIABLE make      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
