project(gpac)
if(WIN32)
    execute_process(
        COMMAND "build.bat" "${CMAKE_INSTALL_PREFIX}"
            RESULT_VARIABLE configure WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    execute_process(
        COMMAND sh "./build.sh" "${CMAKE_INSTALL_PREFIX}"
            RESULT_VARIABLE configure WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
add_custom_target(run_make ALL COMMAND make "-j16" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

install(CODE "execute_process(COMMAND make install WORKING_DIRECTORY \"${CMAKE_SOURCE_DIR}\")")
#