project(x264)

# cmake needs to make something so we can depend on it
add_executable(bootstrap bootstrap.c)

if(WIN32)
    add_custom_target(build
        COMMAND "build.bat" ${CMAKE_INSTALL_PREFIX}
        COMMENT "running x264 build"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
else()
    add_custom_target(build
        COMMAND sh "build.sh" ${CMAKE_INSTALL_PREFIX}
        COMMENT "running x264 build"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/libx264.a
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/)
    
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/x264.h
                  ${CMAKE_CURRENT_SOURCE_DIR}/x264_config.h
                  ${CMAKE_CURRENT_SOURCE_DIR}/x264cli.h
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/)
endif()

add_dependencies(bootstrap build)
