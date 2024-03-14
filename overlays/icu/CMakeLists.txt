cmake_minimum_required(VERSION 3.0)
project(skia)

# cmake needs to make something so we can depend on it
add_executable(bootstrap bootstrap.c)
string(TOLOWER ${CMAKE_BUILD_TYPE} cfg_lower)

# our build script can be defined here
if(WIN32)
    # we copy the files in ninja.bat
    add_custom_target(build
        COMMAND sh "build.bat" ${SDK} ${cfg_lower}
        COMMENT "running icu build"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)

    file(GLOB LIB_FILES ${CMAKE_CURRENT_SOURCE_DIR}/out/${cfg_lower}/*.lib)
    list(REMOVE_ITEM LIB_FILES ${CMAKE_CURRENT_SOURCE_DIR}/out/${cfg_lower}/zlib.lib)
    install(FILES ${LIB_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
else()
    add_custom_target(build
        COMMAND sh "build.sh" ${SDK} ${cfg_lower}
        COMMENT "running icu build"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    
    file(GLOB LIB_FILES ${CMAKE_CURRENT_SOURCE_DIR}/out/${cfg_lower}/*.a)
    list(REMOVE_ITEM LIB_FILES ${CMAKE_CURRENT_SOURCE_DIR}/out/${cfg_lower}/libzlib.a)
    install(FILES ${LIB_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif()

# without the trailing slash it puts 'include' inside of skia
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/skia/)

# needed for svg header
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/modules
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/skia/)

# the bootstrap (made with all) depends on build to run
add_dependencies(bootstrap build)


