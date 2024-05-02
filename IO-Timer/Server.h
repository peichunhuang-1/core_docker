#ifndef CORE_SERVER_TCP_H
#define CORE_SERVER_TCP_H

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

    enum client_slot_state {
        CLIENT_STATE_USED=0,
        CLIENT_STATE_ABORT=1
    };
    
    struct client_slot {
        client_slot()                   = default;
        client_slot(client_slot&& other) noexcept;
        int                             client_fd;
        char                            src_ip[sizeof("xxx.xxx.xxx.xxx")];
        uint16_t                        src_port;
        client_slot_state               state;
        vector<char>                    buffer;
        mutex                           buffer_mutex;
        void                            thread_safe_push_buffer(const char* buffer_once, size_t siz);
        void                            thread_safe_reuse(const char* ip, uint32_t port);
        void                            thread_safe_print();
    };

    class Server {
        public:
        Server(string ip="0.0.0.0", uint32_t port=0); // Done
        int                             event_handler(int timeout);
        bool                            stop;
        int                             find_fd(string ip, uint32_t port);

        private:
        int                             tcp_srv_fd;
        int                             epoll_fd;
        string                          tcp_srv_ip;
        uint32_t                        tcp_srv_port;
        unordered_map<int, client_slot> client_map;
        static const int                maxevents = 128;
        struct epoll_event              events[maxevents];

        const char                      *convert_addr_ntop(struct sockaddr_in *addr, char *src_ip_buf); // Done
        void                            handle_client_event(int client_fd, uint32_t revents); // Done
        int                             add_epoll_event(int fd, uint32_t events); // Done
        int                             delete_epoll_event(int fd); // Done
        int                             init_epoll(); // Done
        int                             init_tcp_srv(); // Done
        int                             accept_new_client(); // Done
    };
}

#endif