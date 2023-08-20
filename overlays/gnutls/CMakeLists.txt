cmake_minimum_required(VERSION 3.20)
project(gnutls)

include_directories(include)

add_library(
    gnutls STATIC
    libspeexdsp/buffer.c)

target_compile_definitions(speexdsp PUBLIC FLOATING_POINT=1)
         
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64") 
    target_compile_definitions(speexdsp PUBLIC USE_SSE=1)
endif()

install(
    TARGETS speexdsp
    LIBRARY DESTINATION lib  # for shared libs
    ARCHIVE DESTINATION lib) # for static libs

install(
    DIRECTORY       include
    DESTINATION     include
    FILES_MATCHING  PATTERN "*.h")

