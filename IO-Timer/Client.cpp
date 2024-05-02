#include "IO-Timer/Client.h"

namespace core_tcp {
    client_member::client_member(client_member&& other) noexcept {
        client_fd = other.client_fd;
        std::copy(std::begin(other.src_ip), std::end(other.src_ip), std::begin(src_ip));
        src_port = other.src_port;
        std::copy(std::begin(other.srv_ip), std::end(other.srv_ip), std::begin(srv_ip));
        srv_port = other.srv_port;
        state = other.state;
        freq = other.freq;
        buffer = std::move(other.buffer);
    }
    void client_member::thread_safe_push_buffer(const char* buffer_once, size_t siz) {
        lock_guard<mutex> lock(buffer_mutex);
        buffer.insert(buffer.end(), buffer_once, buffer_once + siz);
    }
    void client_member::thread_safe_reuse(const char* src_ip_, uint32_t src_port_, const char* srv_ip_, uint32_t srv_port_, float freq_) {
        memcpy(src_ip, src_ip_, sizeof("xxx.xxx.xxx.xxx"));
        src_port = src_port_;
        memcpy(srv_ip, srv_ip_, sizeof("xxx.xxx.xxx.xxx"));
        srv_port = srv_port_;
        state = CLIENT_MEMBER_USED;
        freq = freq_;
        lock_guard<mutex> lock(buffer_mutex);
        buffer = vector<char>();
    }

    Client_Club::Client_Club() {
        init_epoll();
        LOG(INFO) << "Create client club";
    }
    int Client_Club::init_epoll() {
        epoll_fd = epoll_create(255);
        if (epoll_fd < 0) {
            int err = errno;
            LOG(ERROR) << "Failed to init epoll: " << err;
            return -1;
        }
        return 0;
    }
    int Client_Club::add_epoll_event(int fd, uint32_t events) {
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
    int Client_Club::delete_epoll_event(int fd) {
        int err;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
            err = errno;
            LOG(ERROR) << "Failed to delete epoll event: " << err;
            return -1;
        }
        return 0;
    }
    int Client_Club::write_to_socket(int timeout) {
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
            handle_write_event(fd, events[i].events);
        }
        return ret;
    }
    void Client_Club::handle_write_event(int fd, uint32_t revents) {
        if (client_members.find(fd) == client_members.end()) {
            LOG(WARNING) << "No descriptor number " << fd << " is created, or descriptor is closed";
            return;
        }

        const uint32_t err_mask = EPOLLERR | EPOLLHUP;
        struct client_member *client = &(client_members[fd]);

        int nwrite, siz = client->buffer.size();
        int n = siz;
        const char* buffer = client->buffer.data();

        if (revents & err_mask) {
            if (errno != EAGAIN) {
                LOG(ERROR) << "Broken pipe: failed connect to server";
                goto close_conn;
            }
            else {
                LOG(INFO) << "Connection Progressing ...";
                return;
            }
        }
        if (client->state == CLIENT_MEMBER_PROGRESS) {
            struct sockaddr_in local_addr;
            socklen_t addr_len = sizeof(local_addr);
            if (getsockname(fd, (struct sockaddr*)&local_addr, &addr_len) == -1) {
                LOG(ERROR) << "Cannot get socket name";
                goto close_conn;
            }
            uint16_t src_port;
            const char *src_ip;
            char src_ip_buf[sizeof("xxx.xxx.xxx.xxx")];
            src_ip   = convert_addr_ntop(&local_addr, src_ip_buf);
            if (!src_ip) {
                LOG(ERROR) << "Cannot parse source address";
                goto close_conn;
            }
            client->state = CLIENT_MEMBER_USED;
            memcpy(client->src_ip, src_ip_buf, sizeof(src_ip_buf));
            client->src_port = ntohs(local_addr.sin_port);
        }
        while (n > 0) {
            nwrite = write(fd, buffer + siz - n, n);
            if (nwrite < n) {
                if (nwrite == -1 && errno != EAGAIN) {
                    LOG(ERROR) << "Failed to write to socket";
                    goto close_conn;
                }
                break;
            }
            n -= nwrite;
        }
        client->buffer_mutex.lock();
        client->buffer.erase(client->buffer.begin(), client->buffer.begin() + siz - n);
        client->buffer_mutex.unlock();
        return;
        close_conn:
        LOG(INFO) << "Client " << client->src_ip << ":" << client->src_port << " has closed its connection" ;
        delete_epoll_event(fd);
        close(fd);
        client->state = CLIENT_MEMBER_ABORT;
        return;
    }
    int Client_Club::push_buffer(const char* buffer_once, size_t siz, int fd) {
        if (client_members.find(fd) == client_members.end()) {
            LOG(WARNING) << "Push failed: invalid file descriptor " << fd;
            return 0;
        }
        client_members[fd].thread_safe_push_buffer(buffer_once, siz);
        return siz;
    }

    const char *Client_Club::convert_addr_ntop(struct sockaddr_in *addr, char *src_ip_buf) {
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

    int Client_Club::add_client(string srv_ip, uint32_t srv_port, int &fd, float freq) {
        fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
        int ret = 0;
        int err;
        if (fd < 0) {
            err = errno;
            LOG(ERROR) << "Failed to create client " << err ;
            return -1;
        }

        ret = add_epoll_event(fd, EPOLLOUT | EPOLLPRI | EPOLLERR);
        if (ret < 0) return ret;

        struct sockaddr_in dest;
        bzero(&dest, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(srv_port);
        if ( inet_pton(AF_INET, srv_ip.c_str(), &dest.sin_addr.s_addr) == 0 ) {
            goto out;
        }
        
        if ( connect(fd, (struct sockaddr*)&dest, sizeof(dest)) != 0 ) {
            err = errno;
            if(err != EINPROGRESS) {
                goto out;
            }
        }
        uint16_t src_port;
        const char *src_ip;
        char src_ip_buf[sizeof("xxx.xxx.xxx.xxx")];
        src_ip   = convert_addr_ntop(&dest, src_ip_buf);
        if (!src_ip) {
            goto out;
        }
        if (client_members.find(fd) == client_members.end()) {
            struct client_member client;
            client.client_fd = fd;
            client.freq = freq;
            client.state = (err == EINPROGRESS? CLIENT_MEMBER_PROGRESS: CLIENT_MEMBER_USED);
            client.buffer = vector<char>();
            client.srv_port = srv_port;
            memcpy(client.srv_ip, srv_ip.c_str(), sizeof(src_ip_buf));
            client.src_port = ntohs(dest.sin_port);
            memcpy(client.src_ip, src_ip_buf, sizeof(src_ip_buf));
            client_members.emplace(piecewise_construct, forward_as_tuple(fd), forward_as_tuple(move(client)));
        }
        else {
            if (client_members[fd].state == CLIENT_MEMBER_USED) {
                LOG(WARNING) << "Undefined behavior: file descriptor " << fd << " is used, client is not accepted";
                goto out;
            }
            else {
                LOG(INFO) << "File descriptor " << fd << " is abort, reuse descriptor with new client " << src_ip << ":" << src_port;
                client_members[fd].thread_safe_reuse(src_ip_buf, ntohs(dest.sin_port), srv_ip.c_str(), srv_port, freq);
            }
        }
        return 0;
        out: 
        err = errno;
        LOG(ERROR) << "Error occur " << err << ", delete event on " << fd;
        ret = -1;
        delete_epoll_event(fd);
        return ret;
    }
}
