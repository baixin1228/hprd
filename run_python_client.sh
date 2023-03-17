#!/bin/bash
if [ ! -d client_build ]; then
	meson client_build -Dbuild_target=client
fi

ninja -C client_build
if [ $? != 0 ]; then 
	exit 
fi

cd src/clients/pyqt
./main.py $*