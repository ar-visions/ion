# getting random behavior in shine lib's configure -- says it cant pass compilation test with the same environment vars
# maybe force CC or C args in env from CMAKE_C/CXX_COMPILER
execute_process(COMMAND autoreconf "-i" RESULT_VARIABLE autoreconf WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
execute_process(COMMAND sh "./configure" "--prefix=${CMAKE_INSTALL_PREFIX}"
    RESULT_VARIABLE configure WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
if(NOT configure EQUAL 0)
    message(FATAL_ERROR "configure script failed")
endif()

execute_process(COMMAND make RESULT_VARIABLE make WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
if(NOT make EQUAL 0)
    message(FATAL_ERROR "make failed")
endif()

execute_process(COMMAND make install RESULT_VARIABLE make_install WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
if(NOT make_install EQUAL 0)
    message(FATAL_ERROR "install failed; prefix: ${CMAKE_INSTALL_PREFIX}")
endif()