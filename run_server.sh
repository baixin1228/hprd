#!/bin/bash
if [ ! -f server_build/build.ninja ]; then
	meson server_build -Dbuild_target=server -Dbuildtype=debug
fi

ninja -C server_build
if [ $? != 0 ]; then 
	exit 
fi

cd $(dirname "$0")

ret=`ps -aux | grep Xorg | grep -v grep`
user=`echo $ret | awk '{print $1}'`
[[ "$ret" =~ -auth\ ([a-zA-Z0-9/:]*) ]] \
&& sudo XAUTHORITY=${BASH_REMATCH[1]} runuser $user -g $user -c "./server_build/src/server $*"
