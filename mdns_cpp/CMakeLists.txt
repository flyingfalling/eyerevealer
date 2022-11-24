cmake_minimum_required(VERSION 3.11)
project(
  mdns_cpp
  VERSION 0.1.0
  LANGUAGES CXX)

# Set the output of the libraries and executables.
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# ##############################################################################
# library
# ##############################################################################

add_library(${PROJECT_NAME})

#REV: added /src to allow me to access types...
target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    		  	 $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
                         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_sources(
  ${PROJECT_NAME}
  PRIVATE include/mdns_cpp/macros.hpp
          include/mdns_cpp/defs.hpp
          include/mdns_cpp/logger.hpp
          src/logger.cpp
          src/mdns.h
          src/mdns.cpp
          include/mdns_cpp/mdns.hpp
          src/utils.cpp
          include/mdns_cpp/utils.hpp)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(
    ${PROJECT_NAME}
    PRIVATE -Wall -Wextra -pedantic
            # mdns.h uses static functions in the header file
            -Wno-unused-function)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} INTERFACE iphlpapi ws2_32)
endif()
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# ##############################################################################
# example
# ##############################################################################

find_package(Threads REQUIRED)

# ##############################################################################
# install
# ##############################################################################

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

