#ifndef REACTOR_PATTERN_REACTOR_H_
#define REACTOR_PATTERN_REACTOR_H_

#include <iostream>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <unordered_map>

namespace reactor {
// Handle
typedef int Handle;

// Event Type
enum EventType {
    READ_EVENT = 01,
    WRITE_EVENT = 02,
    ERROR_EVENT = 04,
};

// Event Handler
class EventHandler {
  public:
    int events;

    virtual void handle_read() = 0;
    virtual void handle_write() = 0;
    virtual void handle_error() = 0;
    virtual Handle get_handle() = 0;
};

// Synchronous Event Demultiplexer
class SynchronousEventDemultiplexer {
  public:
    virtual void add(Handle handle, int events) = 0;
    virtual void set(Handle handle, int events) = 0;
    virtual void del(Handle handle) = 0;
    virtual void wait(int timeout) = 0;
};

// Initiation Dispatcher
class InitiationDispatcher {
  public:
    std::unordered_map<int, EventHandler *> handlers;

    virtual void register_handler(EventHandler *eh, int events) = 0;
    virtual void modify_handler(EventHandler *eh, int events) = 0;
    virtual void remove_handler(EventHandler *eh) = 0;
    virtual void handle_events(int timeout) = 0; // timeout milliseconds

  protected:
    SynchronousEventDemultiplexer *demultiplexer;
};

// Select Demultiplexer implementation
class SelectDemultiplexer : public SynchronousEventDemultiplexer {
  public:
    SelectDemultiplexer() { maxfd_ = 0; }
    ~SelectDemultiplexer() {}
    void add(Handle handle, int events) override;
    void set(Handle handle, int events) override;
    void del(Handle handle) override;
    void wait(int timeout) override;

  private:
    fd_set rfds_;
    fd_set wfds_;
    fd_set efds_;
    int maxfd_;
};

// Poll Demultiplexer implementation
class PollDemultiplexer : public SynchronousEventDemultiplexer {
  public:
    PollDemultiplexer(int size) {
        fds_ = new struct pollfd[size];
        nfds_ = 0;
        size_ = size;
    }
    ~PollDemultiplexer() {}
    void add(Handle handle, int events) override;
    void set(Handle handle, int events) override;
    void del(Handle handle) override;
    void wait(int timeout) override;

  private:
    struct pollfd *fds_;
    int nfds_;
    int size_;
};

// Epoll Demultiplexer implementation
class EpollDemultiplexer : public SynchronousEventDemultiplexer {
  public:
    EpollDemultiplexer(int size) {
        epfd_ = epoll_create(size);
        events_ = new struct epoll_event[size];
        nfds_ = 0;
        size_ = size;
    }
    ~EpollDemultiplexer() {}
    void add(Handle handle, int events) override;
    void set(Handle handle, int events) override;
    void del(Handle handle) override;
    void wait(int timeout) override;

  private:
    int epfd_;
    struct epoll_event *events_;
    int nfds_;
    int size_;
};

// Reactor
class Reactor : public InitiationDispatcher {
  public:
    ~Reactor() {}
    static Reactor &get_instance() {
        static Reactor reactor;
        return reactor;
    }
    void register_handler(EventHandler *eh, int events) override;
    void modify_handler(EventHandler *eh, int events) override;
    void remove_handler(EventHandler *eh) override;
    void handle_events(int timeout) override;

  private:
    static const int size = 4096;

    Reactor() {
#ifdef HAVE_EPOLL
        demultiplexer = new EpollDemultiplexer(size);
#elif defined(HAVE_POLL)
        demultiplexer = new PollDemultiplexer(size);
#else
        demultiplexer = new SelectDemultiplexer();
#endif
    }
};
} // namespace reactor
#endif // REACTOR_PATTERN_REACTOR_H_
