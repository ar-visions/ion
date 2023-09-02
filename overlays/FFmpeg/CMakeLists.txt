cmake_minimum_required(VERSION 3.0)
project(FFmpeg)

# set build script location
set(build_script "${CMAKE_SOURCE_DIR}/overlays/${PROJECT_NAME}/build.sh")

# cmake needs to make something so we can depend on it
add_executable(bootstrap bootstrap.c)

# this we may need generically included
function(convert_msys INPUT_PATH OUTPUT_PATH)
    string(REPLACE "\\" "/" UNIX_STYLE_PATH ${INPUT_PATH})
    string(REPLACE "C:" "/c" MSYS_PATH ${UNIX_STYLE_PATH})
    set(${OUTPUT_PATH} ${MSYS_PATH} PARENT_SCOPE)
endfunction()

# our build script is defined here
if(WIN32)
    convert_msys(${build_script} msys_style)
    set(cmd C:/msys64/usr/bin/bash.exe -c "${msys_style}")
else()
    set(cmd sh "build.sh")
endif()

# add a custom target which runs the environment script
add_custom_target(build
    COMMAND ${cmd}
    COMMENT "running ${PROJECT_NAME} build"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM)

# set extensions for lib/dll
if(WIN32)
    set(lib "a")
    set(dll "so")
elseif(APPLE)
    set(lib "a")
    set(dll "dylib")
else()
    set(lib "a")
    set(dll "so")
endif()

# install the libs that it makes; it wont install the bootstrap
install(
    FILES       ${CMAKE_CURRENT_SOURCE_DIR}/libavcodec/*.${lib}
    FILES       ${CMAKE_CURRENT_SOURCE_DIR}/libavcodec/*.${dll}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

# the bootstrap (made with all) depends on build to run
add_dependencies(bootstrap build)
