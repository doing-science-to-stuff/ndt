#
# CMakeLists.txt
# ndt: n-dimensional tracer
#
# Copyright (c) 2019 Bryan Franklin. All rights reserved.
#
cmake_minimum_required(VERSION 3.7)
project (ndt)

# remove the 'lib' prefix from modules
set(CMAKE_SHARED_MODULE_PREFIX "")

if(${CMAKE_C_COMPILER_ID} MATCHES "Clang" )
    add_link_options("SHELL:-undefined dynamic_lookup")
endif()

AUX_SOURCE_DIRECTORY(. SOURCE_FILES)
foreach(SRC ${SOURCE_FILES})
    get_filename_component(BASE ${SRC} NAME_WE)
    add_library(${BASE} MODULE ${SRC})
    set_property(TARGET ${BASE} PROPERTY C_STANDARD 99)
endforeach(SRC)

# check for libyaml, for use in the yaml loading scene
set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(YAML)
if( YAML_FOUND )
    include_directories(${YAML_INCLUDE_DIR})
    add_definitions(-DWITH_YAML)
endif()
