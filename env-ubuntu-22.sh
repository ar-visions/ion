#!/bin/sh

# this script is here for support purposes on ubuntu 22.04

# add vulkan sdk package repo as trusted (requires sudo)
# it may be useful to be called prefix/host

if apt-key list | grep -q lunarg.asc; then
else
	wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
	sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.3.261-jammy.list https://packages.lunarg.com/vulkan/1.3.261/lunarg-vulkan-1.3.261-jammy.list
fi

sudo apt update
sudo apt-get install build-essential v4l-utils libv4l-dev ffmpeg git libtool libx11-dev \
	libxrandr-dev libxext-dev cmake vulkan-sdk libmagic-dev python3 \
	python3-pip libxinerama-dev libxcursor-dev libxi-dev gobjc++ clang clang++ libxkbcommon-x11-dev

pip3 install python-magic

