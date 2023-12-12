#!/bin/sh

./configure \
	--pkgconfigdir=$1/lib/pkgconfig \
	--pkg-config-flags="--static" \
	--arch=x86_64 \
	--enable-static \
	--disable-shared \
	--disable-doc \
	--disable-everything \
	--enable-gpl \
	--enable-libx264 \
	--enable-encoder=libx264 \
	--enable-decoder=h264 \
	--enable-muxer=mp4 \
	--enable-muxer=matroska \
	--enable-demuxer=mov \
	--enable-demuxer=matroska \
	--enable-parser=h264 \
	--enable-protocol=file \
	--prefix="$1" \
	--enable-debug \
	--enable-libopus \
	--enable-libfdk-aac \
	--enable-encoder=libopus \
	--enable-encoder=libfdk_aac \
	--enable-decoder=opus \
	--enable-decoder=aac \
	--enable-parser=opus \
	--enable-nonfree && make -j16 && make install

# check if build succeeded
if [ $? -ne 0 ]; then
    echo "build failed"
    exit 1
fi
