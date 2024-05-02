#include "Server.h"
namespace core_tcp {
    client_slot::client_slot(client_slot&& other) noexcept {
        client_fd = other.client_fd;
        std::copy(std::begin(other.src_ip), std::end(other.src_ip), std::begin(src_ip));
        src_port = other.src_port;
        state = other.state;
        buffer = std::move(other.buffer);
    }
    void client_slot::thread_safe_push_buffer(const char* buffer_once, size_t siz) {
        lock_guard<mutex> lock(buffer_mutex);
        buffer.insert(buffer.end(), buffer_once, buffer_once + siz);
    }
    void client_slot::thread_safe_reuse(const char* ip, uint32_t port) {
        memcpy(src_ip, ip, sizeof("xxx.xxx.xxx.xxx"));
        src_port = port;
        state = CLIENT_STATE_USED;
        lock_guard<mutex> lock(buffer_mutex);
        buffer = vector<char>();
    }
    void client_slot::thread_safe_print() {
        lock_guard<mutex> lock(buffer_mutex);
        int siz = buffer.size();
        for (int i = 0; i < siz; i++) 
            printf("%c", buffer[i]);
        printf("\n");
        buffer = vector<char>();
    }
    Server::Server(string ip, uint32_t port) {
        stop = false;
        tcp_srv_ip = ip;
        tcp_srv_port = port;
        init_epoll();
        init_tcp_srv();
        LOG(INFO) << "Create Acceptor on " << tcp_srv_ip << ":" << tcp_srv_port;
    }
    int Server::init_epoll() {
        epoll_fd = epoll_create(255);
        if (epoll_fd < 0) {
            int err = errno;
            LOG(ERROR) << "Failed to init epoll: " << err;
            return -1;
        }
        return 0;
    }
    int Server::init_tcp_srv() {
        tcp_srv_fd = -1;
        struct sockaddr_in addr;
        int err;
        socklen_t addr_len = sizeof(addr);
        tcp_srv_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
        if (tcp_srv_fd < 0) {
            err = errno;
            LOG(ERROR) << "Failed to create socket: " << err;
            return -1;
        }
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(tcp_srv_port);
        addr.sin_addr.s_addr = inet_addr(tcp_srv_ip.c_str());
        int ret = bind(tcp_srv_fd, (struct sockaddr *)&addr, addr_len);
        if (ret < 0) {
            ret = -1;
            err = errno;
            LOG(ERROR) << "Failed to bind acceptor socket: " << err;
            goto out;
        }
        ret = listen(tcp_srv_fd, 10);
        if (ret < 0) {
            ret = -1;
            err = errno;
            LOG(ERROR) << "Failed to liston on acceptor socket: " << err;
            goto out;
        }
        ret = add_epoll_event(tcp_srv_fd, EPOLLIN | EPOLLPRI | EPOLLERR);
        if (ret < 0) {
            goto out;
        }
        return 0;
        out: 
        close(tcp_srv_fd);
        return -1;
    }
    
    int Server::add_epoll_event(int fd, uint32_t events) {
        int err;
        struct epoll_event event;

        memset(&event, 0, sizeof(struct epoll_event));

        event.events  = events;
        event.data.fd = fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
            err = errno;
            LOG(ERROR) << "Failed to add epoll event: " << err;
            return -1;
        }
        return 0;
    }
    int Server::delete_epoll_event(int fd) {
        int err;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
            err = errno;
            LOG(ERROR) << "Failed to delete epoll event: " << err;
            return -1;
        }
        return 0;
    }
    
    void Server::handle_client_event(int client_fd, uint32_t revents) {
        if (client_map.find(client_fd) == client_map.end()) {
            LOG(WARNING) << "No descriptor number " << client_fd << " is created, or descriptor is closed";
            return;
        }
        int err;
        ssize_t recv_ret;
        char buffer_once[1024];
        const uint32_t err_mask = EPOLLERR | EPOLLHUP;
        struct client_slot *client = &(client_map[client_fd]);
        if (revents & err_mask)
            goto close_conn;
        recv_ret = recv(client_fd, buffer_once, sizeof(buffer_once), 0);
        if (recv_ret == 0)
            goto close_conn;
        if (recv_ret < 0) {
            err = errno;
            if (err == EAGAIN) {
                return;
            }
            LOG(ERROR) << "Error receiving data " << err;
            goto close_conn;
        }
        else {
            if (client->state == CLIENT_STATE_USED) client->thread_safe_push_buffer(buffer_once, recv_ret);
        }
        return;
        close_conn:
        LOG(INFO) << "Client " << client->src_ip << ":" << client->src_port << " has closed its connection" ;
        delete_epoll_event(client_fd);
        close(client_fd);
        client->state = CLIENT_STATE_ABORT;
        return;
    }
    
    const char* Server::convert_addr_ntop(struct sockaddr_in *addr, char *src_ip_buf) {
        const char *ret;
        in_addr_t saddr = addr->sin_addr.s_addr;
        ret = inet_ntop(AF_INET, &saddr, src_ip_buf, sizeof("xxx.xxx.xxx.xxx"));
        if (ret == NULL) {
            int err = errno;
            err = err ? err : EINVAL;
            LOG(ERROR) << "Cannot parse source address";
            return NULL;
        }
        return ret;
    }

    int Server::accept_new_client() {
        int client_fd;
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        uint16_t src_port;
        const char *src_ip;
        char src_ip_buf[sizeof("xxx.xxx.xxx.xxx")];
        memset(&addr, 0, sizeof(addr));
        client_fd = accept(tcp_srv_fd, (struct sockaddr *)&addr, &addr_len);
        if (client_fd < 0) {
            int err = errno;
            if (err == EAGAIN)
                return 0;
            LOG(ERROR) << "Failed to accept client " << err ;
            return -1;
        }
        src_port = ntohs(addr.sin_port);
        src_ip   = convert_addr_ntop(&addr, src_ip_buf);
        if (!src_ip) {
            goto close;
        }
        if (client_map.find(client_fd) == client_map.end()) {
            struct client_slot client;
            client.client_fd = client_fd;
            memcpy(client.src_ip, src_ip_buf, sizeof(src_ip_buf));
            client.src_port = src_port;
            client.state = CLIENT_STATE_USED;
            client.buffer = vector<char>();
            add_epoll_event(client_fd, EPOLLIN | EPOLLPRI);
            LOG(INFO) << "Client " << src_ip << ":" << src_port << " has been accepted on descriptor " << client_fd;
            client_map.emplace(piecewise_construct, forward_as_tuple(client_fd), forward_as_tuple(move(client)));
            return 0;
        }
        else {
            if (client_map[client_fd].state == CLIENT_STATE_USED) {
                LOG(WARNING) << "Undefined behavior: file descriptor " << client_fd << " is used, client is not accepted";
                return -1;
            }
            else {
                LOG(INFO) << "File descriptor " << client_fd << " is abort, reuse descriptor with new client " << src_ip << ":" << src_port;
                client_map[client_fd].thread_safe_reuse(src_ip_buf, src_port);
                add_epoll_event(client_fd, EPOLLIN | EPOLLPRI);
                return 0;
            }
        }
        close:
        close(client_fd);
        return 0;
    }

    int Server::find_fd(string ip, uint32_t port) {
        for (const auto& pair : client_map) {
            const client_slot& slot = pair.second;
            if (strcmp(slot.src_ip, ip.c_str()) == 0 && slot.src_port == port) return pair.first; 
        }
        LOG(WARNING) << "Descriptor not found: no matching addr. " << ip << ":" << port;
        return -1;
    }
    
    int Server::event_handler(int timeout) {
        int ret = 0;
        int epoll_ret = epoll_wait(epoll_fd, events, maxevents, timeout);
        if (epoll_ret == 0) return 0;

        if (epoll_ret == -1) {
            int err = errno;
            if (err == EINTR) {
                LOG(WARNING) << "Interrupt by SIGINT";
            }
            ret = -1;
            LOG(ERROR) << "Failed to handle event";
        }
        for (int i = 0; i < epoll_ret; i++) {
            int fd = events[i].data.fd;
            if (fd == tcp_srv_fd) {
                if (accept_new_client() < 0) ret = -1;
                continue;
            }
            handle_client_event(fd, events[i].events);
        }
        return ret;
    }
}