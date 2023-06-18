cmake_minimum_required(VERSION 3.20)
project(moltenvk)

set(TARGET_OS "macos")

# fetch dependencies (its extern)
execute_process(COMMAND sh "fetchDependencies" "--${TARGET_OS}" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" RESULT_VARIABLE sh_res)

# create suffix for debug target
set(dbg "")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(dbg "-debug")
endif()

# make
add_custom_target(run_make ALL COMMAND make "${TARGET_OS}${dbg}" "PREFIX=${CMAKE_INSTALL_PREFIX}" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# make install
install(CODE "execute_process(COMMAND make install \"${TARGET_OS}${dbg}\" \"PREFIX=${CMAKE_INSTALL_PREFIX}\" WORKING_DIRECTORY \"${CMAKE_SOURCE_DIR}\")")