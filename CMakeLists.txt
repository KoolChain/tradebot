cmake_minimum_required(VERSION 3.16)

# Setting the VERSION on root project() will populate CMAKE_PROJECT_VERSION
# see: https://cmake.org/cmake/help/v3.13/variable/CMAKE_PROJECT_VERSION.html
project(Tradebot VERSION "${BUILD_VERSION}")

# Setup cmake helpers in 'cmake' submodule
include ("cmake/include.cmake")


##
## Build options
##

# Common build settings
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

##
## Include components
##

add_subdirectory(src)

include(cmc-install)
# Generates and install the top level XxxConfig.cmake file
# with the logic to find sub-components
cmc_install_root_component_config()
