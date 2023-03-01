#!/bin/bash
if [ ! -d build ]; then
	meson build
fi

ninja -C build
meson test -C build -v hprd_client_test