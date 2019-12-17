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
namespace MTnonblock {

// See Connection.h
    void Connection::Start() {
        //Logger section
        _logger->info("Start st_nonblocking network service");

        _event.events = EPOLLIN | EPOLLRDHUP | EPOLLPRI;

        // Prepare for the first command
        command_to_execute.reset();
        argument_for_command.resize(0);
        parser.Reset();

        _state = 0;
    }

// See Connection.h
    void Connection::OnError() {
        //Logger section
        _logger->error("Connection error");

        std::unique_lock<std::mutex> lock(mutex);
        _state = 1;
        _results.clear();
    }

// See Connection.h
    void Connection::OnClose() {
        //Logger section
        _logger->debug("Closing connection");

        std::unique_lock<std::mutex> lock(mutex);
        _state = 2;
        _results.clear();
    }

// See Connection.h
    void Connection::DoRead() {
        //Logger section
        _logger->debug("DoRead");
        std::unique_lock<std::mutex> lock(mutex);
        try {
            int readed_bytes = -1;
            while ((readed_bytes = read(_socket, client_buffer + already_read,
                                        sizeof(client_buffer) - already_read)) > 0) {
                _logger->debug("Got {} bytes from socket", readed_bytes);
                already_read += readed_bytes;

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

                        if (_results.size() == 1) {
                            _event.events |= EPOLLOUT);
                        }
                    }
                } // while (readed_bytes)
            }

            if (readed_bytes > 0 && errno != EAGAIN) {
                throw std::runtime_error(strerror(errno));
            }
        } catch (std::runtime_error &ex) {
            _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
        }
    }

// See Connection.h
    void Connection::DoWrite() {
        std::unique_lock<std::mutex> lock(mutex);
        //Logger section
        _logger->debug("Do write");
        size_t to_be_written = _results.size();

        struct iovec iovector[to_be_written];

        size_t i = 0;
        for (auto it = _results.begin(); it < _results.end(); it++) {
            if (i != 0) {
                iovector[i].iov_base = (void *) (it->c_str());
                iovector[i].iov_len = it->size();
            } else {
                iovector[i].iov_base = (void *) (it->c_str() + _written_amount);
                iovector[i].iov_len = it->size() - _written_amount;
            }
            i++;
        }

        int written = write(_socket, iovector, to_be_written);
        if (written == -1) {
            _logger->error("Failed to write response to client: {}", strerror(-1));
            OnError();
        } else {
            int current_amount = 0;
            auto to_be_deleted = _results.begin();

            for (to_be_deleted; to_be_deleted < _results.end(); to_be_deleted++) {
                if ((current_amount + to_be_deleted->size()) > written) {
                    _written_amount = current_amount + to_be_deleted->size() - written;
                    break;
                }
                else {
                    current_amount += to_be_deleted->size();
                }
            }

            _results.erase(_results.begin(), to_be_deleted);
        }


        if (_results.empty()) {
            _event.events = ((EPOLLIN | EPOLLRDHUP) | EPOLLPRI);
        }
        else {
            _event.events = (((EPOLLIN | EPOLLRDHUP) | EPOLLERR) | EPOLLOUT);
        }
    }

} // namespace MTnonblock
} // namespace Network
} // namespace Afina
