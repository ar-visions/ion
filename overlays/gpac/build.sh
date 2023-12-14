#!/bin/sh

export PKG_CONFIG_PATH="$1/lib/pkgconfig"

./configure \
	--prefix="$1" \
	--static-build \
	--enable-debug \
	--isomedia-only && \
	make -j16 && make install

# check if build succeeded
if [ $? -ne 0 ]; then
    echo "build failed"
    exit 1
fi
