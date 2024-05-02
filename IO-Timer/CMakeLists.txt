add_library(core_tcp Server.cpp Client.cpp)
target_link_libraries(core_tcp glog::glog)