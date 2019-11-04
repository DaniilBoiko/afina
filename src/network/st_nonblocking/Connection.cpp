#include "Connection.h"

#include <iostream>
#include <cstring>
#include <vector>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <afina/logging/Service.h>

#include "protocol/Parser.h"

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    //Logger section
    _logger = pLogging->select("network");
    _logger->info("Start st_nonblocking network service");

    // Prepare for the first command
    command_to_execute.reset();
    argument_for_command.resize(0);
    parser.Reset();

    _written_pos = _results.begin();

    _state = 0;
}

// See Connection.h
void Connection::OnError() {
    //Logger section
    _logger->error("Connection error");

    _state = 1;
    _results.clear();
}

// See Connection.h
void Connection::OnClose() {
    //Logger section
    _logger->debug("Closing connection");

    _state = 2;
    _results.clear();
}

// See Connection.h
void Connection::DoRead() {
    //Logger section
    _logger->debug("DoRead");


        try {
            int readed_bytes = -1;
            char client_buffer[4096];
            while ((readed_bytes = read(_socket, client_buffer, sizeof(client_buffer))) > 0) {
                _logger->debug("Got {} bytes from socket", readed_bytes);

                // Single block of data readed from the socket could trigger inside actions a multiple times,
                // for example:
                // - read#0: [<command1 start>]
                // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
                while (readed_bytes > 0) {
                    _logger->debug("Process {} bytes", readed_bytes);
                    // There is no command yet
                    if (!command_to_execute) {
                        std::size_t parsed = 0;
                        if (parser.Parse(client_buffer, readed_bytes, parsed)) {
                            // There is no command to be launched, continue to parse input stream
                            // Here we are, current chunk finished some command, process it
                            _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                            command_to_execute = parser.Build(arg_remains);
                            if (arg_remains > 0) {
                                arg_remains += 2;
                            }
                        }

                        // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                        // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                        if (parsed == 0) {
                            break;
                        } else {
                            std::memmove(client_buffer, client_buffer + parsed, readed_bytes - parsed);
                            readed_bytes -= parsed;
                        }
                    }

                    // There is command, but we still wait for argument to arrive...
                    if (command_to_execute && arg_remains > 0) {
                        _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                        // There is some parsed command, and now we are reading argument
                        std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                        argument_for_command.append(client_buffer, to_read);

                        std::memmove(client_buffer, client_buffer + to_read, readed_bytes - to_read);
                        arg_remains -= to_read;
                        readed_bytes -= to_read;
                    }

                    // Thre is command & argument - RUN!
                    if (command_to_execute && arg_remains == 0) {
                        _logger->debug("Start command execution");

                        std::string result;

                        try {
                            command_to_execute->Execute(*pStorage, argument_for_command, result);
                        } catch (std::runtime_error &ex) {
                            result = "SERVER_ERROR ";
                            result += strerror(errno);
                        }

                        // Send response
                        result += "\r\n";
                        _results.push_back(result);

                        // Prepare for the next command
                        command_to_execute.reset();
                        argument_for_command.resize(0);
                        parser.Reset();

                        _event.events = (((EPOLLIN | EPOLLRDHUP) | EPOLLERR) | EPOLLOUT);
                    }
                } // while (readed_bytes)
            }

            if (readed_bytes == 0) {
                _logger->debug("Connection closed");
            } else {
                throw std::runtime_error(std::string(strerror(errno)));
            }
        } catch (std::runtime_error &ex) {
            _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
        }
}

// See Connection.h
void Connection::DoWrite() {
    //Logger section
    _logger->debug("Do write");
    bool updated = false;

    for (auto i = _written_pos; i != _results.end(); i++) {
        auto result = *i;
        result = result.substr(_written_inside);

        int written = write(_socket, result.data(), result.size());
        if (written  <= result.size()) {
            if (written == -1) {
                _logger->error("Failed to write response to client: {}", strerror(-1));
            } else {
                _written_pos = i;
                _written_inside = written;
                updated = true;
            }
            break;
        }
    }

    if (!updated) {
        _written_pos = _results.end();
    }

    if (_results.empty()) {
        _event.events = ((EPOLLIN | EPOLLRDHUP) | EPOLLERR);
    }
    else {
        _event.events = (((EPOLLIN | EPOLLRDHUP) | EPOLLERR) | EPOLLOUT);
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
