#include "examples/echo/echo_server.h"

ListenHandler::ListenHandler(int port) {
    int fd;
    struct sockaddr_in serv_addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket error");
        exit(-1);
    }

    this->fd_ = fd;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
        perror("bind error");
        exit(-1);
    }

    if (listen(fd, 512) < 0) {
        perror("listen error");
        exit(-1);
    }
}

void ListenHandler::handle_read() {
    int fd = accept(fd_, NULL, NULL);
    if (fd < 0) {
        perror("accept error");
    }

    ConnectHandler *connect_handler = new ConnectHandler(fd);

    Reactor::get_instance().register_handler(connect_handler, READ_EVENT);
}

void ListenHandler::handle_write() {
    std::cout << "ListenHandler handle write" << std::endl;
}

void ListenHandler::handle_error() {
    std::cout << "ListenHandler handle error" << std::endl;
}

Handle ListenHandler::get_handle() { return fd_; }

void ConnectHandler::handle_read() {
    int ret = read(fd_, buf_, bufsize_);
    if (ret > 0) {
        write(fd_, buf_, strlen(buf_));
        memset(buf_, 0, bufsize_);
    } else if (ret == 0) {
        std::cout << "client close: fd = " << fd_ << std::endl;

        Reactor::get_instance().remove_handler(this);
    }
}

void ConnectHandler::handle_write() {
    std::cout << "ConnectHandler handle write" << std::endl;
}

void ConnectHandler::handle_error() {
    std::cout << "ConnectHandler handle error" << std::endl;
}

Handle ConnectHandler::get_handle() { return fd_; }
