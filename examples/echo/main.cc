#include "examples/echo/echo_server.h"

int main(int argc, char const *argv[]) {
    EchoServer *server = new EchoServer(8317);

    Reactor::get_instance().register_handler(server->listen_handler,
                                             READ_EVENT);
    Reactor::get_instance().handle_events(5000);

    return 0;
}
