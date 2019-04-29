#
# CMakeLists.txt
# ndt: n-dimensional tracer
#
# Copyright (c) 2019 Bryan Franklin. All rights reserved.
#
cmake_minimum_required(VERSION 3.7)
project (ndt)

# The version number.
set (NDT_VERSION_MAJOR 0)
set (NDT_VERSION_MINOR 0)
set (NDT_VERSION_PATCH 1)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/ndt_config.h.in"
  "${PROJECT_BINARY_DIR}/ndt_config.h"
  )

set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
include_directories("${PROJECT_BINARY_DIR}")

#if(${CMAKE_C_COMPILER_ID} MATCHES "GNU")
#    # this allows dlopen'ed files to find symbols in the executable
#    set (CMAKE_EXE_LINKER_FLAGS "-export-dynamic")
#endif()

# this allows dlopen'ed files to find symbols in the executable
set(CMAKE_ENABLE_EXPORTS TRUE)

add_subdirectory (objects)
add_subdirectory (scenes)

AUX_SOURCE_DIRECTORY(. SOURCE_FILES)
add_executable(ndt ${SOURCE_FILES})
set_property(TARGET ndt PROPERTY C_STANDARD 99)

# create a make clean rule
add_custom_command(OUTPUT clean
                   COMMAND find . -name \*.o | xargs rm -v
                   COMMAND find . -name \*.so | xargs rm -v
                   )

# add common system libraries
#target_link_libraries(ndt ${CMAKE_DL_LIBS})
target_link_libraries(ndt dl)
target_link_libraries(ndt m)
target_link_libraries(ndt pthread)

# check for optional libraries
find_package(PNG REQUIRED)
if( PNG_FOUND )
    include_directories(${PNG_INCLUDE_DIR})
    target_link_libraries(ndt ${PNG_LIBRARY})
    add_definitions(-DWITH_PNG)
endif()

find_package(JPEG)
if( JPEG_FOUND )
    include_directories(${JPEG_INCLUDE_DIR})
    target_link_libraries(ndt ${JPEG_LIBRARY})
    add_definitions(-DWITH_JPEG)
endif()

# verify that at least one image library was found
if( NOT JPEG_FOUND AND NOT PNG_FOUND )
    message( FATAL_ERROR "Unable to find a usable image library.  Either libpng or libjpeg must be installed.")
endif()

find_package(YAML)
if( YAML_FOUND )
    include_directories(${YAML_INCLUDE_DIR})
    target_link_libraries(ndt ${YAML_LIBRARY})
    add_definitions(-DWITH_YAML)
endif()

