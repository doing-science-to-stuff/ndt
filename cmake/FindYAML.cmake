set(CMAKE_PREFIX_PATH "/opt/chef/embedded/")    # for AWS
find_path(YAML_INCLUDE_DIR yaml.h)
find_library(YAML_LIBRARY libyaml)
find_library(YAML_LIBRARY libyaml.so)
find_library(YAML_LIBRARY libyaml.dylib)
if( NOT ${YAML_LIBRARY} MATCHES "-NOTFOUND" )
    set(YAML_FOUND TRUE)
endif()
