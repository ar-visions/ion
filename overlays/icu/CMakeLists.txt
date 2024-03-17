cmake_minimum_required(VERSION 3.0)
project(icu4c)

# cmake needs to make something so we can depend on it
add_executable(bootstrap bootstrap.c)
string(TOLOWER ${CMAKE_BUILD_TYPE} cfg_lower)

if(WIN32)
    add_custom_target(build
        COMMAND "build.bat" ${SDK} ${cfg_lower}
        COMMENT "running icu build"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
else()
    add_custom_target(build
        COMMAND sh "build.sh" ${SDK} ${cfg_lower}
        COMMENT "running icu build"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
endif()

# the bootstrap (made with all) depends on build to run
add_dependencies(bootstrap build)


