#!/bin/bash
if [ ! -d client_build/build.ninja ]; then
	meson client_build -Dbuild_target=client
fi

ninja -C client_build
if [ $? != 0 ]; then 
	exit 
fi

cd src/clients/pyqt
python3 ./main.py $*