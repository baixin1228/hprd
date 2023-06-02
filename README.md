![HPRD](res/logo_128.png)

# High Performance Remote Desktop

## Introduction
Hprd is a remote desktop that enables users to stream their desktops and games to other devices. Stream/share gameplay moments and remotely control your desktop using custom hotkeys.Extremely small bandwidth occupation, about 1~10Mbps, can run on the public network.Hprd is under development, so there will be many problems to be solved.

### Features
1.X11 Capture  
2.X11 Render  
3.KCP protocol  
4.H264/H265 encodec and decodec  
5.Share clipboard  

## Installation
Install Server:  
```sh
sudo dpkg -i hprd_server.deb
```

Install Client:  
```sh
sudo dpkg -i hprd_client.deb
```
##### Usage
Run Server:  
```sh
sudo systemctl start hprd_server
```

Run Client:  
```sh
hprd_client
```

## Development

##### For Deepin V20、Debian and Ubuntu 20.04:

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

##### For Ubuntu 22.04:

server side
```sh
sudo apt install meson libavcodec-dev libavformat-dev libswscale-dev libglib2.0-dev
```

client side
```sh
sudo apt install libopenh264-dev libdrm-dev libsdl2-dev libx11-dev libxext-dev libglib2.0-dev
```

##### Running
Run Server:  
```sh
./run_server.sh
```

Run Client:  
```sh
#SDL client
./run_client.sh
#or
./run_client.sh --ip 192.168.0.1

#python client
./run_python_client.sh
#or
./run_python_client.sh --ip 192.168.0.1 -s
```

### Todo
1.File transfer  
2.Audio support  
3.USB protocol transparent transmission  
4.Video codec hardware acceleration  
5.Gamepad adaptation  
6.Encrypted transmission  
7.Windows client  
8.Windows server  
9.Android client  

## License
GPL2.0, see the LICENSE for details.