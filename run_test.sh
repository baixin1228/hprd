#!/bin/bash
if [ ! -d build/build.ninja ]; then
	meson build
fi
# meson test -C build
# meson test -C build -v
# meson test -C build -v  hprd_client_test $*
# meson test -C build --gdb hprd_server_test
#meson test -C build --gdb --print-errorlogs --suite x11_extensions_test
ninja -C build
meson test -C build -v hprd_server_test