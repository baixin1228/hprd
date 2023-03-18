[icon]

# Name

[Badges]

## Introduction

### Summary

### Features

## Requirements
For Deepin V20 and Debian:
```
#server side
sudo apt install meson ninja-build build-essential
sudo apt install libavcodec-dev libavformat-dev libswscale-dev

#client side
# clone and build openh264 package
git clone https://salsa.debian.org/debian/openh264
dpkg-buildpackage -uc -us -Jauto
# install openh264 debs after build it.
sudo apt install ./libopenh264-7_2.3.1+dfsg-3_amd64.deb
sudo apt install ./libopenh264-dev_2.3.1+dfsg-3_amd64.deb
# install other dependency
sudo apt install libdrm-dev
```

## Configuration

## Installation

## Usage
Run Server:
./run_server.sh

Run Client:
./run_client.sh
./run_client.sh --ip 192.168.0.1

./run_python_client.sh
./run_python_client.sh --ip 192.168.0.1 -s

## Development

## Changelog

## FAQ

## Support

### Dos

### Contact

## Authors and acknowledgment

## License