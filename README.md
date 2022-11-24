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

1) ffmpeg (version >= 4.5?) (see doc/knownissues.txt) (currently some encoding done via command-line OS pipes, so need ffmpeg exec)
   -> libavcodec, libavformat, libswresample, libswscale, AND OTHERS
2) libboost (version that includes beast, i.e. network http related items)
3) libopencv
4) cmake


Nested Dependencies:
1) Dependencies for IMGUI (glfw, glad, opengl?)


Optional Dependencies:
1) librealsense (https://github.com/IntelRealSense/librealsense)
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

Or with realsense:

mkdir build
cd build
cmake -DWITH_REALSENSE=true ..
make


With ITD (currently disabled in CMAKE):
cmake -DWITH_ITD=true ..

