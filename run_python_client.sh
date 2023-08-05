#!/bin/bash
if [ ! -d client_build/build.ninja ]; then
	meson client_build -Dbuild_target=client
fi

ninja -C client_build
if [ $? != 0 ]; then 
	exit 
fi

dpkg -l python3-pyqt5 2>&1 > /dev/null
if [ $? != 0 ]; then 
	echo "not find python3-pyqt5."
	exit
fi

cd src/clients/pyqt
python3 ./main.py $*