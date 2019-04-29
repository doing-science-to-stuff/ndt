cmake_minimum_required(VERSION 3.7)
project (ndt)

#option(BUILD_SHARED "cause add_library() to create shared libraries if on" ON)

set(CMAKE_SHARED_MODULE_PREFIX "")

if(${CMAKE_C_COMPILER_ID} MATCHES "Clang" )
    #add_link_options("SHELL:-shared")
    add_link_options("SHELL:-undefined dynamic_lookup")
endif()

AUX_SOURCE_DIRECTORY(. SOURCE_FILES)
foreach(SRC ${SOURCE_FILES})
    get_filename_component(BASE ${SRC} NAME_WE)
    add_library(${BASE} MODULE ${SRC})
    #install (TARGETS ${BASE} DESTINATION .)
endforeach(SRC)

# check for libyaml, for use in the yaml loading scene
set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(YAML)
if( YAML_FOUND )
    include_directories(${YAML_INCLUDE_DIR})
    add_definitions(-DWITH_YAML)
endif()
