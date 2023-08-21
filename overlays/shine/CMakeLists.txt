cmake_minimum_required(VERSION 3.20)
project(libshine)

include_directories(src src/lib src/bin)

add_library(
    shine STATIC
    src/lib/bitstream.c src/lib/huffman.c
    src/lib/l3bitstream.c
    src/lib/l3loop.c 
    src/lib/l3mdct.c 
    src/lib/l3subband.c
    src/lib/layer3.c
    src/lib/reservoir.c
    src/lib/tables.c)
                         
install(
    TARGETS shine
    LIBRARY DESTINATION lib  # for shared libs
    ARCHIVE DESTINATION lib) # for static libs

install(
    DIRECTORY       src/lib/
    DESTINATION     include/shine
    FILES_MATCHING  PATTERN "*.h")

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
