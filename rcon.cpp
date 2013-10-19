// Copyright (c) 2012-2013 Zeex
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "sampquery/query.hpp"

static const char prompt_string[] = ">>> ";

class rcon {
 public:
  typedef std::function<void(
    const std::string &output, const boost::system::error_code &ec)
  > receive_handler;

  rcon(boost::asio::io_service &io_service,
       boost::asio::ip::udp::endpoint endpoint):
    timeout_(0),
    query_(sampquery::query_type::rcon_command, io_service, endpoint)
  {
    using namespace std::placeholders;
    query_.set_timeout_handler(std::bind(&rcon::handle_timeout, this, _1));
    query_.set_receive_handler(std::bind(&rcon::handle_receive, this, _1, _2));
  }

  void set_timeout(boost::posix_time::milliseconds timeout) {
    timeout_ = timeout;
  }

  boost::posix_time::milliseconds timeout() const {
    return timeout_;
  }

  void set_receive_handler(receive_handler handler) {
    receive_handler_ = handler;
  }

  void send_command(const std::string password, std::string &command) {
    using boost::asio::buffer;

    std::vector<boost::asio::const_buffer> send_buffers;

    std::uint16_t password_length = password.length();
    send_buffers.push_back(buffer(&password_length, sizeof(password_length)));
    send_buffers.push_back(buffer(password));

    std::uint16_t command_length  = command.length();
    send_buffers.push_back(buffer(&command_length,  sizeof(command_length)));
    send_buffers.push_back(buffer(command));

    query_.send(send_buffers);
    query_.receive(timeout_);
  }

 private:
  void handle_receive(const boost::system::error_code &ec,
                      std::size_t nbytes) {
    if (receive_handler_) {
      receive_handler_(ec ? std::string() : query_.response_text(), ec);
    }
    if (ec != boost::asio::error::operation_aborted) {
      query_.receive(timeout_);
    }
  }

  void handle_timeout(const boost::system::error_code &ec) {
    if (ec) {
      throw boost::system::system_error(ec);
    }
  }

 private:
  boost::posix_time::milliseconds timeout_;
  sampquery::query query_;
  receive_handler receive_handler_;
};

static bool read_command(std::string &command) {
  return std::getline(std::cin, command).good();
}

int main(int argc, char **argv) {
  std::string host;
  std::string port;
  std::string password;
  std::string command;
  long timeout_ms;
  bool interactive;

  try {
    boost::program_options::options_description desc("Available options");
    desc.add_options()
      ("help,h",
       "show this message and exit")
      ("host,s",
       boost::program_options::value(&host)->default_value("127.0.0.1"),
       "set server IP address or hostname")
      ("port,p",
       boost::program_options::value(&port)->default_value("7777"),
       "set server port")
      ("password,w",
        boost::program_options::value(&password)->required(),
       "set RCON password")
      ("command,c",
        boost::program_options::value(&command),
       "set command to be executed")
      ("timeout,t",
        boost::program_options::value(&timeout_ms)->default_value(150),
       "set response timeout (in milliseconds)")
      ("interactive,i",
        boost::program_options::bool_switch(&interactive),
       "run in interactive mode")
    ;

    auto parser = boost::program_options::command_line_parser(argc, argv);
    auto parsed_options = parser.options(desc).run();

    boost::program_options::variables_map variables;
    boost::program_options::store(parsed_options, variables);

    if (variables.count("help")) {
      auto program_name = boost::filesystem::basename(argv[0]);
      std::cout << "Usage: " << program_name << " [options]\n\n"
                << desc << std::endl;
      return 0;
    }

    boost::program_options::notify(variables);

    if (!interactive) {
      if (!variables.count("command")) {
        std::cerr << "non-interactive mode requries a command" << std::endl;
        return 1;
      }
    }
  }
  catch (boost::program_options::error &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  try {
    if (interactive) {
      std::cout << "Remote console to " << host << ":" << port << "\n"
                << "Type \"cmdlist\" for the list of available commands\n"
                << std::flush;
    }

    boost::asio::io_service io_service;
    boost::asio::ip::udp::endpoint endpoint;

    boost::asio::ip::udp::resolver resolver(io_service);
    boost::asio::ip::udp::resolver::query resolver_query(host, port);

    for (auto it = resolver.resolve(resolver_query);
          it != boost::asio::ip::udp::resolver::iterator(); it++) {
      if (it->endpoint().address().is_v4()) {
        endpoint = *it;
        break;
      }
    }

    rcon rcon(io_service, endpoint);

    rcon.set_timeout(boost::posix_time::milliseconds(timeout_ms));
    rcon.set_receive_handler([&](const std::string &output,
                                 const boost::system::error_code &ec) {
      if (!ec) {
        std::cout << output << std::endl;
      } else {
        if (ec == boost::asio::error::operation_aborted) {
          std::string command;
          if (interactive) {
            std::cout << prompt_string;
            if (read_command(command)) {
              rcon.send_command(password, command);
            } else {
              std::cout << std::endl;
              io_service.stop();
            }
          }
        } else if (ec == boost::asio::error::connection_reset) {
          // If the server happens to be down we just output nothing
          // and keep running.
        } else {
          throw boost::system::system_error(ec);
        }
      }
    });

    if (interactive) {
      std::cout << prompt_string;
      if (!read_command(command)) {
        std::cout << std::endl;
        return 0;
      }
    }

    rcon.send_command(password, command);
    io_service.run();
  }
  catch (std::exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
