#!/bin/bash
if [ ! -d server_build ]; then
	meson server_build -Dbuild_target=server -Dbuildtype=debug
fi

ninja -C server_build
if [ $? != 0 ]; then 
	exit 
fi
meson test -C server_build -v encodec_test