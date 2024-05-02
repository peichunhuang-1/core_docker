cmake_minimum_required(VERSION 3.8)

project(tcp C CXX)
set (CMAKE_EXE_LINKER_FLAGS)
set (CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -fPIC")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
find_package (glog 0.6.0 REQUIRED)

include_directories("${CMAKE_CURRENT_LIST_DIR}")
add_subdirectory(IO-Timer)
# add_subdirectory(RPC)