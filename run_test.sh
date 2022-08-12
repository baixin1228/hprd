#!/bin/bash
# meson test -C build
# meson test -C build -v
meson test -C build -v  hprd_client_test $*
# meson test -C build --gdb hprd_server_test
#meson test -C build --gdb --print-errorlogs --suite x11_extensions_test
# meson test -C build -v x11_extensions_performance_test
