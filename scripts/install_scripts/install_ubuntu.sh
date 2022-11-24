#!/bin/bash

#REV: basic installs
sudo apt-get install cmake
sudo apt-get install build-essential

#REV: opencv for cv::Mat etc.
sudo apt-get install libopencv-dev

#REV: check version? Should be 4.5...?
#REV: this is the tools for the ghetto C++->command line call via system...
sudo apt-get install ffmpeg

# REV: maybe use for saving e.g. depth...
sudo apt-get install libhdf5-dev

# FFMPEG libraries for decoding etc.
sudo apt-get install libavdevice-dev
sudo apt-get install libavfilter-dev
sudo apt-get install libavformat-dev
sudo apt-get install libavcodec-dev
sudo apt-get install libavutil-dev

#REV: need for glfw etc.
sudo apt-get install libvulkan-dev
sudo apt-get install xorg-dev

#REV: need for network IO (ASIO)
sudo apt-get install libboost-all-dev

sudo apt-get install intel-media-va-drivers

#REV: may need gstreamer (for codecs?)

#REV: OPTIONAL (some video viewers...)
#sudo apt-get install vlc mpv

#REV: an editor...
#sudo apt-get install emacs

#REV: below is non-free driver
# i965-va-driver
# Non-free:
# i965-va-driver-shaders

# intel-media-va-driver
# Non-free:
# intel-media-va-driver-non-free


# Non-free are needed for encoding. Need to activate in your repo.
# Note, ubuntu 22.04 intel media drivers are broken/buggy

#REV: note for intel (QSV) need intel-media-driver intel-media-sdk?
# intel-gpu-tools, gives intel_gpu_top

#REV: for nvidia, ofcourse nvidia.. (not much decode/encode support though)

#REV: on my old laptop with 8th gen i7, 11% to decode 1920x1080 and 1024x256 at same time... CPU decoding doesn't increase anything at all :(

# sudo apt-get install gstreamer1.0-vaapi
# sudo apt-get install intel-media-va-driver-non-free 



# REV: major issue is that FFMPEG is not guaranteed to have good access to all types of GPU encoders...
# REV: file size and write speed are a major issue, and memory bandwidth.


# REV: realsense
# 1) clone git repo
# 2) install libusb-1.0-0-dev
# 3) libglfw3-dev libglu1-mesa-dev freeglut-dev
#    libusb-dev?
#   cmake .. and make


#sudo cp config/99-realsense-libusb.rules /etc/udev/rules.d/
#sudo udevadm control --reload-rules
#sudo udevadm trigger
