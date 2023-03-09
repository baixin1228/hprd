#!/bin/bash
if [ ! -d build ]; then
	meson build
fi

ninja -C build
if [ $? != 0 ]; then 
	exit 
fi
# meson test -C build -v hprd_client_test
./build/src/client