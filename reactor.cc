#include "reactor.h"

namespace reactor {
// Reactor
void Reactor::register_handler(EventHandler *eh, int events) {
    Handle handle = eh->get_handle();
    eh->events |= events;
    handlers[handle] = eh;

    demultiplexer->add(handle, events);
}

void Reactor::modify_handler(EventHandler *eh, int events) {
    Handle handle = eh->get_handle();
    eh->events = events;
    handlers[handle] = eh;

    demultiplexer->set(handle, events);
}

void Reactor::remove_handler(EventHandler *eh) {
    Handle handle = eh->get_handle();
    handlers.erase(handle);

    demultiplexer->del(handle);
}

void Reactor::handle_events(int timeout) { demultiplexer->wait(timeout); }

// SelectDemultiplexer
void SelectDemultiplexer::add(Handle handle, int events) {
    if (handle > FD_SETSIZE) {
        std::cout << "SelectDemultiplexer cannot add anymore" << std::endl;
        return;
    }

    if (handle > maxfd_) {
        maxfd_ = handle;
    }
}

void SelectDemultiplexer::set(Handle handle, int events) {
    std::cout << "SelectDemultiplexer set nothing" << std::endl;
}

void SelectDemultiplexer::del(Handle handle) {
    FD_CLR(handle, &rfds_);
    FD_CLR(handle, &wfds_);
    FD_CLR(handle, &efds_);
}

void SelectDemultiplexer::wait(int timeout) {
    int ret;

    for (;;) {
        FD_ZERO(&rfds_);
        FD_ZERO(&wfds_);
        FD_ZERO(&efds_);

        for (auto it : Reactor::get_instance().handlers) {
            int fd = it.first;
            int events = it.second->events;

            if (events & READ_EVENT) {
                FD_SET(fd, &rfds_);
            }

            if (events & WRITE_EVENT) {
                FD_SET(fd, &wfds_);
            }

            if (events & ERROR_EVENT) {
                FD_SET(fd, &efds_);
            }
        }

        struct timeval _timeout;
        _timeout.tv_sec = timeout / 1000;
        _timeout.tv_usec = (timeout % 1000) * 1000;

        ret = select(maxfd_ + 1, &rfds_, &wfds_, &efds_, &_timeout);
        if (ret < 0) {
            if (errno != EINTR) {
                std::cout << "SelectDemultiplexer wait error:" << errno
                          << std::endl;
                break;
            } else {
                continue;
            }
        } else if (ret == 0) {
            continue;
        } else {
            for (int i = 0; i <= maxfd_; i++) {
                if (Reactor::get_instance().handlers.count(i)) {
                    EventHandler *handler = Reactor::get_instance().handlers[i];

                    if (FD_ISSET(i, &rfds_)) {
                        handler->handle_read();
                    }

                    if (FD_ISSET(i, &wfds_)) {
                        handler->handle_write();
                    }

                    if (FD_ISSET(i, &efds_)) {
                        handler->handle_error();
                    }
                }
            }
        }
    }
}

// PollDemultiplexer
void PollDemultiplexer::add(Handle handle, int events) {
    if (nfds_ > size_) {
        std::cout << "PollDemultiplexer cannot add anymore" << std::endl;
        return;
    }

    fds_[nfds_].fd = handle;
    if (events & READ_EVENT) {
        fds_[nfds_].events |= POLLIN;
    }

    if (events & WRITE_EVENT) {
        fds_[nfds_].events |= POLLOUT;
    }

    if (events & ERROR_EVENT) {
        fds_[nfds_].events |= (POLLERR | POLLHUP);
    }

    nfds_++;
}

void PollDemultiplexer::set(Handle handle, int events) {
    std::cout << "PollDemultiplexer set nothing" << std::endl;
}

void PollDemultiplexer::del(Handle handle) {
    for (int i = 0; i < nfds_; i++) {
        if (fds_[i].fd == handle) {
            fds_[i].fd = 0;
            fds_[i].events = 0;
            break;
        }
    }
}

void PollDemultiplexer::wait(int timeout) {
    for (;;) {
        int ret = poll(fds_, nfds_, timeout);
        if (ret < 0) {
            if (errno != EINTR) {
                std::cout << "PollDemultiplexer wait error:" << errno
                          << std::endl;
                break;
            } else {
                continue;
            }
        } else if (ret == 0) {
            continue;
        } else {
            for (int i = 0; i < nfds_; i++) {
                int fd = fds_[i].fd;

                if (fd == 0) {
                    continue;
                }

                if (Reactor::get_instance().handlers.count(fd)) {
                    EventHandler *handler =
                        Reactor::get_instance().handlers[fd];
                    if (fds_[i].revents & POLLIN) {
                        handler->handle_read();
                    }

                    if (fds_[i].revents & POLLOUT) {
                        handler->handle_write();
                    }

                    if (fds_[i].revents & (POLLERR | POLLHUP)) {
                        handler->handle_error();
                    }
                }
            }
        }
    }
}

// EpollDemultiplexer
void EpollDemultiplexer::add(Handle handle, int events) {
    if (nfds_ > size_) {
        std::cout << "EpollDemultiplexer cannot add anymore" << std::endl;
        return;
    }

    struct epoll_event ev;
    ev.data.fd = handle;
    ev.events = 0;

    if (events & READ_EVENT) {
        ev.events |= EPOLLIN;
    }

    if (events & WRITE_EVENT) {
        ev.events |= EPOLLOUT;
    }

    if (events & ERROR_EVENT) {
        ev.events |= (EPOLLRDHUP | EPOLLHUP | EPOLLERR);
    }

    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, handle, &ev) < 0) {
        std::cout << "EpollDemultiplexer add error:" << errno << std::endl;
    } else {
        nfds_++;
    }
}

void EpollDemultiplexer::set(Handle handle, int events) {
    struct epoll_event ev;
    ev.data.fd = handle;
    ev.events = 0;

    if (events & READ_EVENT) {
        ev.events |= EPOLLIN;
    }

    if (events & WRITE_EVENT) {
        ev.events |= EPOLLOUT;
    }

    if (events & ERROR_EVENT) {
        ev.events |= (EPOLLRDHUP | EPOLLHUP | EPOLLERR);
    }

    if (epoll_ctl(epfd_, EPOLL_CTL_MOD, handle, &ev) < 0) {
        std::cout << "EpollDemultiplexer set error: " << errno << std::endl;
    }
}

void EpollDemultiplexer::del(Handle handle) {
    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, handle, nullptr) < 0) {
        std::cout << "EpollDemultiplexer del error: " << errno << std::endl;
    } else {
        nfds_--;
    }
}

void EpollDemultiplexer::wait(int timeout) {
    for (;;) {
        int ret = epoll_wait(epfd_, events_, nfds_, timeout);
        if (ret < 0) {
            if (errno != EINTR) {
                std::cout << "EpollDemultiplexer wait error:" << errno
                          << std::endl;
                break;
            } else {
                continue;
            }
        } else if (ret == 0) {
            continue;
        } else {
            for (int i = 0; i < ret; i++) {
                int fd = events_[i].data.fd;
                if (Reactor::get_instance().handlers.count(fd)) {
                    EventHandler *handler =
                        Reactor::get_instance().handlers[fd];
                    if (events_[i].events & EPOLLIN) {
                        handler->handle_read();
                    }

                    if (events_[i].events & EPOLLOUT) {
                        handler->handle_write();
                    }

                    if (events_[i].events &
                        (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                        handler->handle_error();
                    }
                }
            }
        }
    }
}
} // namespace reactor
