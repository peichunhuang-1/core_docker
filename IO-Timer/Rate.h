#ifndef RATE_H
#define RATE_H

#include <sys/timerfd.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <glog/logging.h>

namespace core_timer {
    class Rate {
    public:
        Rate(float freq) {
            timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
            if (timer_fd == -1) {
                LOG(ERROR) << "Failed to create timer file descriptor !";
                return;
            }
            interval_ns = 1e9 / freq;

            struct itimerspec interval;
            interval.it_interval.tv_sec = interval_ns / 1000000000;
            interval.it_interval.tv_nsec = interval_ns % 1000000000;
            interval.it_value.tv_sec = interval.it_interval.tv_sec;
            interval.it_value.tv_nsec = interval.it_interval.tv_nsec;

            if (timerfd_settime(timer_fd, 0, &interval, NULL) == -1) {
                LOG(ERROR) << "Failed to set time file descriptor: " << errno;
                close(timer_fd);
                return;
            }
            epoll_fd = epoll_create(1);
            if (epoll_fd == -1) {
                LOG(ERROR) << "Epoll event error: " << errno;
                close(timer_fd);
                return;
            }
            ev.events = EPOLLIN;
            ev.data.fd = timer_fd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &ev) == -1) {
                LOG(ERROR) << "Failed to add epoll event: " << errno;
                close(epoll_fd);
                close(timer_fd);
                return;
            }
            return;
        }

        bool sleep() {
            struct epoll_event events;
            int nfds = epoll_wait(epoll_fd, &events, 1, -1);
            if (nfds == -1) {
                LOG(ERROR) << "Epoll event error: " << errno;
                return false;
            }
            if (events.data.fd == timer_fd) {
                uint64_t expirations;
                ssize_t s = read(timer_fd, &expirations, sizeof(expirations));
                if (s != sizeof(expirations)) {
                    LOG(ERROR) << "Failed to read timer expirations: " << errno;
                    return false;
                }
                return false;
            }
            return true;
        }
        bool vtask_loop() {return sleep();}
        template<typename function, typename... functions>
        bool vtask_loop(function&& func, functions&&... funcs) {
            struct epoll_event events;
            int nfds = epoll_wait(epoll_fd, &events, 1, 0);
            if (nfds == -1) {
                LOG(ERROR) << "Epoll event error: " << errno;
                return false;
            }
            if (nfds == 0) {
                func();
                return vtask_loop(std::forward<functions>(funcs)...);
            }
            else {
                if (events.data.fd == timer_fd) {
                    uint64_t expirations;
                    ssize_t s = read(timer_fd, &expirations, sizeof(expirations));
                    if (s != sizeof(expirations)) {
                        LOG(ERROR) << "Failed to read timer expirations: " << errno;
                        return false;
                    }
                    return false;
                }
                else return true;
            }
        }

        ~Rate() {
            close(timer_fd);
            close(epoll_fd);
        }

    private:
        int                 timer_fd;
        long                interval_ns;
        int                 epoll_fd;
        epoll_event         ev;
    };
}
#endif