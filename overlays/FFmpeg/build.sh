#!/bin/bash

## all shell should have a function
check_install() {
    package=$1
    ## install package if it does not exist
    if ! pacman -Qq ${package} > /dev/null 2>&1; then
        pacman -S --noconfirm ${package}
        ## exit the script if this package fails
        ## this triggers the CI python user to return exit 1
        if [ ! $? -eq 0 ]; then
            exit 1
        fi
        echo "${package}: installed"
    else
        echo "${package}: already installed"
    fi
}

# Update the package database
pacman -Sy

# Install GCC, Make, and other development tools
check_install mingw-w64-x86_64-gcc
check_install mingw-w64-x86_64-make
check_install mingw-w64-x86_64-python3
check_install mingw-w64-x86_64-x264
check_install autoconf
check_install automake
check_install libtool
check_install yasm
check_install pkg-config

## make sure we are running in the extern dir
## could download msys2 in prefix; no reason why we cant do it in python
## we run it from there, and its directory is that of prefix install
cd "$(dirname "$0")"
./configure --prefix=/mingw64 \
    --enable-static --disable-shared \
    --enable-libx264 --enable-gpl --disable-debug
make -j10
