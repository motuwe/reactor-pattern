#include "reactor.h"
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace reactor;
class ListenHandler : public EventHandler {
  public:
    ListenHandler(int port);
    ~ListenHandler() { close(fd_); }
    void handle_read() override;
    void handle_write() override;
    void handle_error() override;
    Handle get_handle() override;

  private:
    int fd_;
};

class ConnectHandler : public EventHandler {
  public:
    ConnectHandler(int fd) : fd_(fd), buf_(new char[bufsize_]) {
        memset(buf_, 0, bufsize_);
    }
    ~ConnectHandler() {
        close(fd_);
        delete[] buf_;
    }
    void handle_read() override;
    void handle_write() override;
    void handle_error() override;
    Handle get_handle() override;

  private:
    int fd_;
    char *buf_;
    static const int bufsize_ = 1024;
};

class EchoServer {
  public:
    EventHandler *listen_handler;

    EchoServer(int port) { listen_handler = new ListenHandler(port); }
    ~EchoServer() { delete listen_handler; }
};
