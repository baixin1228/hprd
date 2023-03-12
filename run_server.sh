#!/bin/bash
if [ ! -d server_build ]; then
	meson server_build -Dbuild_target=server
fi

ninja -C server_build
if [ $? != 0 ]; then 
	exit 
fi
# meson test -C server_build -v hprd_server_test

./server_build/src/server
