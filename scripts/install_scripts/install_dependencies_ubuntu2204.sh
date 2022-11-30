##REV: generally useful
sudo apt-get install git subversion


##CMAKE
sudo apt-get install cmake

##OPENCV
sudo apt-get install libopencv-dev

##FFMPEG and LIBAV*
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libswscale-dev libavfilter-dev libavdevice-dev
sudo apt-get install ffmpeg

##Boost-related (asio networking stuff)
sudo apt-get install libboost-all-dev libasio-dev

##For IMGUI
##Piece-by-piece includes: libegl-dev libegl1-mesa-dev libffi-dev libgl-dev libgl1-mesa-dev libgles-dev libgles1 libglfw3 libglvnd-core-dev libglvnd-dev libglx-dev libopengl-dev libpthread-stubs0-dev libvulkan-dev libwayland-bin libwayland-dev libx11-dev libxau-dev libxcb1-dev libxdmcp-dev libxext-dev libxrandr-dev libxrender-dev x11proto-dev xorg-sgml-doctools xtrans-dev
sudo apt-get install libglfw3-dev
sudo apt-get install libxinerama-dev 
sudo apt-get install libxcursor-dev
sudo apt-get install libxi-dev

##Needed for STDUUID
sudo apt-get install uuid-dev


## REV: for h264 vaapi:
sudo apt-get install mesa-va-drivers vainfo


##Optional, e.g. for opencv hdf5 depth writing...
sudo apt-get install libhdf5-dev 


##UBUNTU, optional if you want better (newer) mesa drivers...
sudo add-apt-repository ppa:kisak/kisak-mesa
sudo apt update
sudo apt-get upgrade
sudo apt-get install mesa-va-drivers mesa-vulkan-drivers mesa-common-dev mesa-utils

#REV: undo the ppa...
#sudo apt install ppa-purge
#sudo ppa-purge ppa:kisak/kisak-mesa

##Gstreamer (needed?)
sudo apt-get install gstreamer1.0-plugins-bad  gstreamer1.0-plugins-bad-apps  gstreamer1.0-plugins-base gstreamer1.0-plugins-base-apps  gstreamer1.0-plugins-good gstreamer1.0-plugins-rtp  gstreamer1.0-plugins-ugly


#REV: had to switch from /dev/dri/renderD129 (from D128...)
#REV: was trying to use nouveau? Can I autodetect better?
#REV: Also, switch to mp4 because gallium driver is shit for MKV?
#REV: note ffmpeg is 4.4...
