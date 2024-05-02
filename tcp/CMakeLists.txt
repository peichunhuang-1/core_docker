cmake_minimum_required(VERSION 3.8)

project(tcp C CXX)
set (CMAKE_EXE_LINKER_FLAGS)
set (CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -fPIC")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
find_package (glog 0.6.0 REQUIRED)

add_library(core_tcp Server.cpp Client.cpp)
target_link_libraries(core_tcp glog::glog)

add_executable(core_tcp_server_test server_test.cpp)
target_link_libraries(core_tcp_server_test core_tcp glog::glog)

add_executable(core_tcp_client_test client_test.cpp)
target_link_libraries(core_tcp_client_test core_tcp glog::glog)