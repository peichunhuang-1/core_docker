#ifndef CORE_CLIENT_TCP_H
#define CORE_CLIENT_TCP_H

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <deque>
#include <vector>
#include <mutex>
#include <iostream>
#include <unordered_map>
#include <cstdio>
#include <cstdarg>
#include <glog/logging.h>

namespace core_tcp {
    using namespace std;

    enum client_member_state {
        CLIENT_MEMBER_USED=0,
        CLIENT_MEMBER_ABORT=1,
        CLIENT_MEMBER_PROGRESS=2
    };
    struct client_member {
        client_member()                       = default;
        client_member(client_member&& other)    noexcept;
        vector<char>                            buffer;
        mutex                                   buffer_mutex;
        client_member_state                     state;
        char                                    src_ip[sizeof("xxx.xxx.xxx.xxx")];
        uint16_t                                src_port;
        char                                    srv_ip[sizeof("xxx.xxx.xxx.xxx")];
        uint16_t                                srv_port;
        int                                     client_fd;
        void                                    thread_safe_push_buffer(const char* buffer_once, size_t siz);
        void                                    thread_safe_reuse(const char* src_ip_, uint32_t src_port_, const char* srv_ip_, uint32_t srv_port_, float freq_);
        float                                   freq;
    };
    class Client_Club {
        public:
        Client_Club(); // Done
        int                                     add_client(string srv_ip, uint32_t srv_port, int &fd, float freq); // Done
        int                                     write_to_socket(int timeout); // Done
        int                                     push_buffer(const char* buffer_once, size_t siz, int fd); // Done
        private:
        static const int                        maxevents = 128;
        struct epoll_event                      events[maxevents];
        int                                     epoll_fd;
        unordered_map<int, client_member>       client_members;
        int                                     add_epoll_event(int fd, uint32_t events); // Done
        int                                     delete_epoll_event(int fd); // Done
        int                                     init_epoll(); // Done
        const char                              *convert_addr_ntop(struct sockaddr_in *addr, char *src_ip_buf); // Done
        void                                    handle_write_event(int fd, uint32_t revents);
    };
    
}

#endif