cmake_minimum_required(VERSION 3.20)
project(libspeexdsp)

include_directories(include)

add_library(
    speexdsp STATIC
    libspeexdsp/buffer.c
	libspeexdsp/fftwrap.c
	libspeexdsp/filterbank.c
	libspeexdsp/jitter.c
	libspeexdsp/kiss_fft.c
	libspeexdsp/kiss_fftr.c
	libspeexdsp/mdf.c
	libspeexdsp/preprocess.c
    libspeexdsp/resample.c
    libspeexdsp/scal.c
    libspeexdsp/smallft.c)

target_compile_definitions(speexdsp PRIVATE FIXED_POINT=1)
                         
install(
    TARGETS speexdsp
    LIBRARY DESTINATION lib  # for shared libs
    ARCHIVE DESTINATION lib) # for static libs

install(
    DIRECTORY       include
    DESTINATION     include
    FILES_MATCHING  PATTERN "*.h")
