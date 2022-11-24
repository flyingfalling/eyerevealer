##FOR REALSENSE:

sudo apt-get install libusb-dev libusb1.0.0-dev libglu1-mesa-dev

git clone https://github.com/IntelRealSense/librealsense.git
cd librealsense
mkdir build
cd build
#REV: not clear if this is true...works either way?
#REV: build type release or no optimizations...
#REV: using RSUSB causes issues with power or something...
#cmake -DFORCE_RSUSB_BACKEND=true -DCMAKE_BUILD_TYPE=Release ..
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j12
sudo make install

#REV: then copy over the udev rules...
sudo sh scripts/setup_udev_rules.sh
