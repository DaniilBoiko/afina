#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <vector>

#include <sys/epoll.h>
#include <spdlog/logger.h>
#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <afina/logging/Service.h>
#include "protocol/Parser.h"

namespace Afina {
namespace Network {
namespace MTnonblock {

    class Connection {
    public:
        Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> l) : _socket(s),
                                                                                                   pStorage(ps), _logger(l) {
            std::memset(&_event, 0, sizeof(struct epoll_event));
            _event.data.ptr = this;
        }

        inline bool isAlive() const { return (_state == 0); }

        void Start();

    protected:
        void OnError();
        void OnClose();
        void DoRead();
        void DoWrite();

    private:
        friend class Worker;
        friend class ServerImpl;

        int _socket;
        struct epoll_event _event;

        // 0 — alive
        // 1 — error
        // 2 — dead
        int _state;
        std::mutex mutex;

        std::shared_ptr<spdlog::logger> _logger;
        std::shared_ptr<Afina::Storage> pStorage;
        std::shared_ptr<Afina::Logging::Service> pLogging;

        // Reading related
        std::size_t arg_remains;
        Protocol::Parser parser;
        std::string argument_for_command;
        std::unique_ptr<Execute::Command> command_to_execute;

        // Writing related
        std::vector<std::string> _results;
        int _written_amount;

        int already_read;
        char client_buffer[4096];
    };

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
