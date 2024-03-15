#!/bin/sh
cd icu4c/source && ./configure --enable-static --disable-shared && make -j16 install prefix=$INSTALL_PREFIX
