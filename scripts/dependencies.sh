#!/bin/bash

#gcc etc.
#librealsense2 (from github)
sudo pacman -S pkgconfig
sudo pacman -S opencv
sudo pacman -S cmake
sudo pacman -S boost
sudo pacman -S vtk
sudo pacman -S pugixml

#REV: for VAAPI stuff?
# vainfo.
# vainfo --display drm --device /dev/dri/renderD129
# ls -l /dev/dri/by-path  will show which PCI port points to each.
sudo pacman -S mesa-utils
sudo pacman -S libva-utils
sudo pacman -S libva


#REV: make install realsense...
git clone https://github.com/IntelRealSense/librealsense.git
cd librealsense
mkdir build
cmake -DCMAKE_BUILD_TYPE=RELEASE ..
make -j4
sudo make install

sudo cp ~/.99-realsense-libusb.rules /etc/udev/rules.d/99-realsense-libusb.rules

sudo echo "/usr/local/lib" > /etc/ld.so.conf.d/realsense
sudo ldconfig

#itd libs (ISC)
#ftd3xx libs (proprietary FTDI -- in the itd dir)
