# eyerevealer
Real-time streaming software for wearable eye-trackers (tobii glasses 2, tobii glasses 3, pupil labs) and other sensor systems (Intel realsense, USB cameras, etc.)

MIT License. See sub-directories for licenses of other included source (glfw, json, etc.).

Copyright Richard Veale
2022 November 24 (last updated)

(privately: rteye2 lib)
Real-Time EYE-tracking Library (2)

Now renamed:
EyeRevealer (eyerevealer)

1) Provides interfaces to stream/save data from wearable eyetrackers:
Tobii Glasses 2
Tobii Glasses 3
Pupil Invisible
(Deprecated) SMI Glasses

2) Provides interfaces to stream/save data from other sensors/devices:
Intel RealSense depth/stereo cameras
Web Cameras (USB)
ITD stereo camera

3) Calibration/sensor-fusion between sensors

-Stream Video/Audio Scene
-Stream Video (Eye)
-Stream Gaze Position (pupil size?)
-Stream gyro/accelerometer data





Future: Can be extended to add other sensors.
Gyro/Accelerometers
New stereo cameras
Interface to get from stationary (remote) eyetrackers, e.g. SR Research EYELINK 1000+ etc.



Notes:

Current implementation is highly inefficient for compilation -- header files are included in main() cpp files.

Saving of data can only be done via "RAW" saving, which exhaustively records all information from a device, as written by the writer of the PARSER class where saving happens.

Main tool is tools/rteye2viewer.cpp.

GUI is accomplished via imgui, which in turn uses glfw etc. backend.
Decoding/Encoding is accomplished via FFMPEG.
Realsense library is necessary for using Intel realsense devices.
Internal image processing via opencv (>=3?)



Dependencies:
(see e.g. scripts/install_dependencies_ubuntu2204.sh)

1) ffmpeg (version >= 4.4) (see doc/knownissues.txt) (currently some encoding done via command-line OS pipes, so need ffmpeg exec)
   -> libavcodec, libavformat, libswresample, libswscale, AND OTHERS
2) libboost (version that includes beast/asio, i.e. network http related items)
3) libopencv
4) cmake
5) imgui dependencies (i.e. glfw3, libxinerama, libxcursor, libxi, librandr, etc.)
6) uuid (uuid-dev)

Semi-optional dependencies (needed for saving to disk using h264):
1) VAAPI-compatible driver and setup, and set env var DRI_RENDER_NODE
2) HDF5

Optional Dependencies:
1) librealsense (https://github.com/IntelRealSense/librealsense) (see scripts/install_realsense_ubuntu2204.sh)
2) ITD LAB libraries (ITD binocular/stereo camera -- proprietary) (https://itdlab.com/wordpress/)


Included Dependencies (or parts of):
1) json (nlohmann)
2) stduuid
3) imgui
4) glfw
5) glad
6) mdns_cpp


Compile via e.g.:

mkdir build

cd build

cmake ..

make


Or, with realsense:

mkdir build

cd build

cmake -DWITH_REALSENSE=true ..

make



With ITD (currently disabled in CMAKE):

cmake -DWITH_ITD=true ..





USAGE:

./rteye2viewer.exe


For raw saving via hardware, I have not yet implemented direct interfaces via libavcodec and libavformat yet, so it opens a pipe and executes ffmpeg. This requires (indeed it will probably still require in the future even after moving to native implementation) that VAAPI and DRI render nodes etc, are appropriately set, and that you have ensured that your driver for the corresponding VAAPI render node can support h264 with the input passed as nv12 format.


To set the dri render node, you may set (for example):

export DRI_RENDER_NODE=/dev/dri/renderD129

before running the program (the environmental variable is read via env())


Note the default (if nothing is set) is to use

/dev/dri/renderD128
