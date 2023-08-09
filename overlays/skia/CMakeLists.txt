cmake_minimum_required(VERSION 3.0)
project(skia)

# cmake needs to make something so we can depend on it
add_executable(bootstrap bootstrap.c)

# our build script can be defined here
add_custom_target(build
    COMMAND sh "build.sh"
    COMMENT "running skia build"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM)

# the bootstrap (made with all) depends on build to run
add_dependencies(bootstrap build)

# install the libs that it makes; it wont install the bootstrap
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/out/Vulkan/libskia.a
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

# without the trailing slash it puts 'include' inside of skia
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/skia)
