cmake_minimum_required(VERSION 3.0)
project(skia)

# cmake needs to make something so we can depend on it
add_executable(bootstrap bootstrap.c)
string(TOLOWER ${CMAKE_BUILD_TYPE} cfg_lower)

# our build script can be defined here
if(WIN32)
    # we copy the files in ninja.bat
    add_custom_target(build
        COMMAND "gen.bat" ${SDK} ${cfg_lower}
        COMMAND "ninja.bat" ${SDK} ${cfg_lower}
        COMMENT "running skia build"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
else()
    add_custom_target(build
        COMMAND sh "build.sh" ${SDK} ${cfg_lower}
        COMMENT "running skia build"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    
    # install the libs that it makes; it wont install the bootstrap
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/out/${cfg_lower}/libskia.a
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

    # without the trailing slash it puts 'include' inside of skia
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/skia/)

    # needed for svg header
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/modules
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/skia/)
endif()

# the bootstrap (made with all) depends on build to run
add_dependencies(bootstrap build)


