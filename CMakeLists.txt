#
# CMakeLists.txt
# ndt: n-dimensional tracer
#
# Copyright (c) 2019 Bryan Franklin. All rights reserved.
#
cmake_minimum_required(VERSION 2.8)
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

# add location of mpi compilers to path on AWS
LIST(APPEND CMAKE_PROGRAM_PATH "/usr/lib64/mpich/bin" "/usr/lib64/openmpi/bin")

# check for optional libraries
find_package(PNG REQUIRED)
find_package(JPEG)
find_package(YAML)
find_package(MPI)

# add the binary tree to the search path for include files
# so that we will find ndt_config.h
include_directories("${PROJECT_BINARY_DIR}")

# this allows dlopen'ed files to find symbols in the executable
set(CMAKE_ENABLE_EXPORTS TRUE)

add_subdirectory (objects)
add_subdirectory (scenes)

AUX_SOURCE_DIRECTORY(. SOURCE_FILES)
add_executable(ndt ${SOURCE_FILES})
set_property(TARGET ndt PROPERTY C_STANDARD 99)

# add a docker image building target
add_custom_target(docker
                  COMMAND docker build -t ndt .
                  )

# add a docker image run target
add_custom_target(docker-run
                  COMMAND docker run -it -v `pwd`/images:/app/images -v `pwd`/yaml:/app/yaml ndt
                  DEPENDS docker
                  )

# add a debug build target
add_custom_target(debug-build
                  COMMAND bash -c 'cmake -DCMAKE_BUILD_TYPE=Debug . && make'
                  )

# add a valgrind testing target
add_custom_target(valgrind
                  COMMAND valgrind --leak-check=full --show-reachable=yes --vgdb-error=0 -v ./ndt -r 60x45 -t 4 -k 8 -f 64:67:300 -w -d 5 -s scenes/random.so
                  DEPENDS debug-build
                  )

# add common system libraries
target_link_libraries(ndt dl)
target_link_libraries(ndt m)
target_link_libraries(ndt pthread)

# Deal with optional libaries
if( PNG_FOUND )
    include_directories(${PNG_INCLUDE_DIR})
    target_link_libraries(ndt ${PNG_LIBRARY})
    add_definitions(-DWITH_PNG)
endif()

if( JPEG_FOUND )
    include_directories(${JPEG_INCLUDE_DIR})
    target_link_libraries(ndt ${JPEG_LIBRARY})
    add_definitions(-DWITH_JPEG)
endif()

# verify that at least one image library was found
if( NOT JPEG_FOUND AND NOT PNG_FOUND )
    message( FATAL_ERROR "Unable to find a usable image library.  Either libpng or libjpeg must be installed.")
endif()

if( YAML_FOUND )
    include_directories(${YAML_INCLUDE_DIR})
    target_link_libraries(ndt ${YAML_LIBRARY})
    add_definitions(-DWITH_YAML)
endif()

if( MPI_FOUND AND YAML_FOUND )
    # see: https://cmake.org/pipermail/cmake/2011-June/045037.html
    include_directories(${MPI_CXX_INCLUDE_DIRS})
    include_directories(${MPI_CXX_INCLUDE_PATH})
    target_link_libraries(ndt ${MPI_CXX_LIBRARIES})
    add_definitions(-DWITH_MPI)

    if(MPI_COMPILE_FLAGS)
        set_target_properties(ndt PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
    endif()

    if(MPI_LINK_FLAGS)
        set_target_properties(ndt PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
    endif()
elseif( MPI_FOUND AND NOT YAML_FOUND )
    message("MPI support in ndt requires YAML, but YAML not found, leaving MPI disabled.")
endif()
