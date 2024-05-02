#include "IO-Timer/Server.h"
#include "IO-Timer/Rate.h"
#include <chrono>
#include <iostream>
#include <functional>
int main(int argc, char* argv[])
{
    FLAGS_log_dir = "../log/";
    google::InitGoogleLogging(argv[0]);
    int ret = 0;
    core_tcp::Server srv("127.0.0.1", 1234);
    float freq = 1000.0;
    core_timer::Rate rate(freq);
    while (!srv.stop) {
        // auto start = std::chrono::high_resolution_clock::now();
        srv.event_handler(0);
        rate.sleep();
        // auto end = std::chrono::high_resolution_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        // std::cout << "程序执行时间: " << duration.count() << " 微秒" << std::endl;
        // rate.vtask_loop(std::bind(&core_tcp::Server::event_handler, &srv, 0));
    }
out:
    return ret;
}