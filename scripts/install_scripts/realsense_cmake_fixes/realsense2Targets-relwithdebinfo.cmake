#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "realsense2::realsense-file" for configuration "RelWithDebInfo"
#REV: again, comment out -file references?
#set_property(TARGET realsense2::realsense-file APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
#set_target_properties(realsense2::realsense-file PROPERTIES
#  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C;CXX"
#  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib64/librealsense-file.a"
#  )

#REV: commented out -file related
#list(APPEND _cmake_import_check_targets realsense2::realsense-file )
#list(APPEND _cmake_import_check_files_for_realsense2::realsense-file "${_IMPORT_PREFIX}/lib64/librealsense-file.a" )

# Import target "realsense2::realsense2" for configuration "RelWithDebInfo"
set_property(TARGET realsense2::realsense2 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(realsense2::realsense2 PROPERTIES
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib64/librealsense2.so.2.51.1"
  IMPORTED_SONAME_RELWITHDEBINFO "librealsense2.so.2.51"
  )

list(APPEND _cmake_import_check_targets realsense2::realsense2 )
list(APPEND _cmake_import_check_files_for_realsense2::realsense2 "${_IMPORT_PREFIX}/lib64/librealsense2.so.2.51.1" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
