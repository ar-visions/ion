#!/bin/sh

./configure \
	--enable-static \
    --enable-debug \
    --prefix=$1 \
    && make -j16 && make install

# check if build succeeded
if [ $? -ne 0 ]; then
    echo "build failed"
    exit 1
fi


