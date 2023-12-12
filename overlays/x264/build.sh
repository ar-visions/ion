#!/bin/sh

echo "prefix=$1
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: x264
Description: H.264 (MPEG4 AVC) encoder library
Version: 0.164.3106
Libs: -L\${exec_prefix}/lib -lx264 -lpthread -lm -ldl
Libs.private: 
Cflags: -I\${prefix}/include" >> "$1/lib/pkgconfig/x264.pc"

./configure \
	--enable-static \
    --enable-debug \
    --prefix=$1 \
    && make -j16

# check if build succeeded
if [ $? -ne 0 ]; then
    echo "build failed"
    exit 1
fi


