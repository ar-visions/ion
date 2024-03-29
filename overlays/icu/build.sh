#!/bin/sh
cd icu4c/source && ./configure CFLAGS="-fPIC" CXXFLAGS="-fPIC" --enable-static --disable-shared && make -j16 install prefix=$INSTALL_PREFIX
