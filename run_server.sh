#!/bin/bash
if [ ! -f server_build/build.ninja ]; then
	meson server_build -Dbuild_target=server -Dbuildtype=debug
fi

ninja -C server_build
if [ $? != 0 ]; then 
	exit 
fi
# meson test -C server_build -v server_test

if [ -f /var/run/lightdm/root/:0 ]
then
	XAUTHORITY=/var/run/lightdm/root/:0 sudo ./server_build/src/server $*
else
	./server_build/src/server $*
fi
