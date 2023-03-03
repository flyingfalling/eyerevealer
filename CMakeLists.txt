# REV: rteye2 cmake file. See tutorial (newer version, e.g.)
# https://cmake.org/cmake/help/v3.16/guide/tutorial/index.html

# REV: let user make shared libs (just for streaming from connected devices) -- for example will be used by the iphone interface
# REV: separately, there is e.g. rteye2viewer and other applications which use that same functionality...


cmake_minimum_required(VERSION 3.10)
project(rteye2)
project(rteye2
	VERSION 0.1
	DESCRIPTION "RTEYE2: Real-Time EYE-t2acking"
	LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

#include(FindFFMPEG.cmake)

# REV: specify the C++ standard (need c++20 for some of the libs)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
##https://www.internalpointers.com/post/modern-cmake-beginner-introduction
###target_compile_features(myApp PRIVATE cxx_std_20)
## find_library finds the lib, find_package does cmake specific stuff defined by other users and sets vars automatically
## external_project does not assume the libs are already compiled, so it will go and do them (with cmake).
## fetchcontent basically grabs it and treats as sub-project etc...


set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wshadow -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-narrowing -Wno-format -Wno-unused-but-set-variable")
#set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(WITH_ITD "Compile with ITD libraries (for ITD depth cameras)" OFF)
option(WITH_REALSENSE "Compile with REALSENSE (RS2) libraries (for intel realsense cameras)" OFF)

if(WITH_ITD)
	list(APPEND EXTRA_LIBS ftd3xx ISCLibxc ISCSDKLibxc)
	
	#REV: TODO -- make user set external location of ITD libs... (and make them compile it themselves?) -- since I can not
	#REV: legally include the binaries in this git package.
	#REV: assume these will be installed in default system location... let user select custom?
	#list(APPEND EXTRA_INCLUDES "${ITD_LIB_DIR}/itd/LINUX/ISCLibxc/include/")
	#include_directories(${CMAKE_SOURCE_DIR}/itd/LINUX/ISCLibxc/include/)
	#link_directories(${CMAKE_SOURCE_DIR}/itd/LINUX/ISCLibxc/lib/x86_64)
endif()

if(WITH_REALSENSE)
	find_package( realsense2 REQUIRED )
	list(APPEND EXTRA_INCLUDES ${realsense2_INCLUDE_DIRS})
	list(APPEND EXTRA_LIBS realsense2)
	#include_directories( ${realsense2_INCLUDE_DIRS})
endif()

#REV: what the fuck is findpackage vs findlibrary?

###### Required system libs... ############

### 0) CURL
#find_package( CURL REQUIRED )
#include_directories( ${CURL_INCLUDE_DIRS} )

### 1) OPENCV
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

### 2) LIBAV (ffmpeg)
#find_package( PkgConfig REQUIRED)
#pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
#    libavdevice
#    libavfilter
#    libavformat
#    libavcodec
#    libswresample
#    libswscale
#    libavutil
#)

find_package(FFMPEG 4.4.0 COMPONENTS avdevice avfilter avformat avcodec avutil swresample swscale)
#find_package(FFMPEG 5.1.2 COMPONENTS avdevice avfilter avformat avcodec avutil swresample swscale)

add_compile_definitions(FFMPEG_LIB_VERSION=${FFMPEG_VERSION})

include_directories( ${FFMPEG_INCLUDE_DIRS} )

add_compile_definitions(FFMPEG_LIB_VERSION=${FFMPEG_VERSION})

if( ${FFMPEG_VERSION} VERSION_GREATER_EQUAL 5.0 )
add_compile_definitions( FFMPEG_VERSION_GEQ_5 )
endif()

###### INCLUDED LOCAL LIBS ############

### 1) glad, imgui, glfw
#REV: should set this up with sub-dirs?
include_directories( ${PROJECT_SOURCE_DIR}/glad )
include_directories( ${PROJECT_SOURCE_DIR}/glad/glad )
include_directories( ${PROJECT_SOURCE_DIR}/glfw/include )
include_directories( ${PROJECT_SOURCE_DIR}/imgui )

#REV: add_subdirectory will make it compile those as well
add_subdirectory("${PROJECT_SOURCE_DIR}/glad")
add_subdirectory("${PROJECT_SOURCE_DIR}/imgui")
add_subdirectory("${PROJECT_SOURCE_DIR}/glfw")

## MDNS CPP
include_directories( ${PROJECT_SOURCE_DIR}/mdns_cpp )
add_subdirectory("${PROJECT_SOURCE_DIR}/mdns_cpp")

########## INCLUDE ROOT DIR SO THAT WE CAN FIND e.g. ./glad/xxx ##########
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR})

## COMPARE VERSION
include_directories( ${PROJECT_SOURCE_DIR}/CompareVersion )
set(SOURCES ${SOURCES} CompareVersion/CompareVersion.cpp)

## REV: ghetto ffmpeg private struct format...
##	include_directories(${PROJECT_SOURCE_DIR}/rteye2ffmpeg)


########## INCLUDE EXTRA DIRS.... ############
include_directories(${EXTRA_INCLUDES})


########## RTEYE VIEWER ###############
set(EXECUTABLE_NAME rteye2viewer.exe)

configure_file( include/rteye2_config.hpp.in rteye2_config.hpp )

# Add source to this project's executable.

set(SOURCES ${SOURCES} "tools/rteye2viewer.cpp")
add_executable (${EXECUTABLE_NAME} ${SOURCES})
# add_executable (${EXECUTABLE_NAME} "tools/rteye2viewer.cpp")

target_include_directories(${EXECUTABLE_NAME} PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           )

# Add tests and install targets if needed.
target_link_libraries (${EXECUTABLE_NAME} ${EXTRA_LIBS} uuid pthread ${OpenCV_LIBS} ${FFMPEG_LIBRARIES} glfw imgui mdns_cpp )

install(TARGETS ${EXECUTABLE_NAME} DESTINATION /usr/local/bin)


############## TOBII2 GAZE DECODER #################

set(EXECUTABLE_NAME2 tobii2_file_vidgaze_decoder.exe)
configure_file( include/rteye2_config.hpp.in rteye2_config.hpp )
set(SOURCES2 ${SOURCES2} "tools/tobii2_file_vidgaze_decoder.cpp")
add_executable (${EXECUTABLE_NAME2} ${SOURCES2})
target_include_directories(${EXECUTABLE_NAME2} PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           )
target_link_libraries (${EXECUTABLE_NAME2} ${EXTRA_LIBS} uuid pthread ${OpenCV_LIBS} ${FFMPEG_LIBRARIES} glfw imgui mdns_cpp )

install(TARGETS ${EXECUTABLE_NAME2} DESTINATION /usr/local/bin)


RETURN()


####################### REV OLD CODE ######################
#include_directories(${CMAKE_SOURCE_DIR}/itd/LINUX/ISCLibxc/include/)
#link_directories(${CMAKE_SOURCE_DIR}/itd/LINUX/ISCLibxc/lib/x86_64)
#include_directories(${CMAKE_SOURCE_DIR}/itd/LINUX/FTDI/include/)
#link_directories(${CMAKE_SOURCE_DIR}/itd/LINUX/FTDI/lib/x86_64)

