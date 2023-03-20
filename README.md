![HPRD](res/logo_128.png)

# High Performance Remote Desktop

[Badges]

## Introduction

### Summary

### Features

## Requirements

#### For Deepin V20、Debian and Ubuntu 20.04:

server side
```sh
sudo apt install meson libavcodec-dev libavformat-dev libswscale-dev libglib2.0-dev
```

client side
```sh
git clone https://salsa.debian.org/debian/openh264
cd openh264
dpkg-buildpackage -uc -us -Jauto
sudo apt install ../libopenh264-7_2.3.1+dfsg-3_amd64.deb
sudo apt install ../libopenh264-dev_2.3.1+dfsg-3_amd64.deb
sudo apt install libdrm-dev libsdl2-dev libx11-dev libxext-dev libglib2.0-dev
```

#### For Ubuntu 22.04:

server side
```sh
sudo apt install meson libavcodec-dev libavformat-dev libswscale-dev libglib2.0-dev
```

client side
```sh
sudo apt install libopenh264-dev libdrm-dev libsdl2-dev libx11-dev libxext-dev libglib2.0-dev
```

## Configuration

## Installation

## Usage
Run Server:  
```sh
./run_server.sh
```

Run Client:  
```sh
./run_client.sh
./run_client.sh --ip 192.168.0.1

./run_python_client.sh
./run_python_client.sh --ip 192.168.0.1 -s
```

## Development

## Changelog

## FAQ

## Support

### Dos

### Contact

## Authors and acknowledgment

## License
