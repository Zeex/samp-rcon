// Copyright (c) 2012 Zeex
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

#include <cstdlib>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "rcon_client.hpp"

static const char prompt_string[] = ">>> ";

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
       "set connection timeout (in milliseconds)")
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
      std::cout << "Usage: " << program_name << " [options]\n\n" << desc << std::endl;
      std::exit(EXIT_SUCCESS);
    }

    boost::program_options::notify(variables);

    if (!interactive) {
      if (!variables.count("command")) {
        std::cerr << "non-interactive mode requries a command" << std::endl;
        std::exit(EXIT_FAILURE);
      }
    }
  }
  catch (boost::program_options::error &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  try {
    boost::asio::io_service io_service;

    boost::asio::ip::udp::resolver resolver(io_service);
    boost::asio::ip::udp::resolver::query query(host, port);
    boost::asio::ip::udp::endpoint endpoint = *resolver.resolve(query);

    rcon_client rcon(io_service, endpoint);

    if (interactive) {
      std::cout << "Remote console to " << endpoint << "\n"
                << "Type \"cmdlist\" for the list of available commands\n"
                << std::flush;
    }

    auto timeout = boost::posix_time::milliseconds(timeout_ms);
    auto timeout_handler = [&rcon](const boost::system::error_code &error) {
    if (error) {
        std::cerr << boost::system::system_error(error).what() << std::endl;
      } else {
        rcon.cancel();
      }
    };
    rcon.set_timeout_handler(timeout_handler);

    auto receive_handler = [&rcon, &password, &command, &timeout, &interactive](
      const boost::system::error_code &error, std::size_t nbytes)
    {
      if (error == boost::asio::error::operation_aborted) {
        if (interactive) {
          std::cout << prompt_string;
          if (!std::getline(std::cin, command)) {
            std::cout << std::endl;
            std::exit(EXIT_SUCCESS);
          }
          rcon.send(password, command);
          rcon.receive(timeout);
        }
      } else {
        std::cout << rcon.response_text() << std::endl;
        rcon.receive(timeout);
      }
    };
    rcon.set_receive_handler(receive_handler);

    if (interactive) {
      std::cout << prompt_string;
      if (!std::getline(std::cin, command)) {
        std::cout << std::endl;
        std::exit(EXIT_SUCCESS);
      }
    }

    rcon.send(password, command);
    rcon.receive(timeout);

    io_service.run();
  }
  catch (boost::system::system_error &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::exit(EXIT_SUCCESS);
}
