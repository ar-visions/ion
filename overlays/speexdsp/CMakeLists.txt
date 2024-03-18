cmake_minimum_required(VERSION 3.20)
project(libspeexdsp)

include_directories(include)

# this still needs the configure script to be there (need better solution for this lib)

if (EXISTS "${CMAKE_SOURCE_DIR}/configure")
    # set(autoreconf "0")
elseif (EXISTS "${CMAKE_SOURCE_DIR}/autogen.sh")
    execute_process(COMMAND sh "autogen.sh" RESULT_VARIABLE autoreconf WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
elseif(NOT WIN32)
    execute_process(COMMAND autoreconf "-i" RESULT_VARIABLE autoreconf WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# [sync] run configure
if(EXISTS "./configure")
    execute_process(COMMAND sh "./configure" "--prefix=${CMAKE_INSTALL_PREFIX}"
    RESULT_VARIABLE configure WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    # lets try not configuring on windows
    if(NOT configure EQUAL 0)
        message(FATAL_ERROR "configure script failed")
    endif()
endif()

add_library(
    speexdsp STATIC
    libspeexdsp/buffer.c
	libspeexdsp/filterbank.c
	libspeexdsp/jitter.c
	libspeexdsp/kiss_fft.c
	libspeexdsp/kiss_fftr.c
	libspeexdsp/mdf.c
	libspeexdsp/preprocess.c
    libspeexdsp/resample.c
    libspeexdsp/scal.c
    libspeexdsp/smallft.c)

target_compile_definitions(speexdsp PUBLIC FLOATING_POINT=1)
         
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64") 
    target_compile_definitions(speexdsp PUBLIC USE_SSE=1)
endif()

install(
    TARGETS speexdsp
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib  # for shared libs
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib) # for static libs

install(
    DIRECTORY       include/
    DESTINATION     ${CMAKE_INSTALL_PREFIX}/include
    FILES_MATCHING  PATTERN "*.h")
