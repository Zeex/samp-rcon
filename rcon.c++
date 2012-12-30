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
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "rcon_client.h++"

int main(int argc, char **argv) {
  std::string host;
  std::string port;
  std::string password;
  std::string command;
  long timeout_ms;

  try {
    boost::program_options::options_description desc("Available options");
    desc.add_options()
      ("help,h",
       "show this message and exit")
      ("host,a", boost::program_options::value<std::string>(&host)->default_value("localhost"),
       "set server IP address or hostname")
      ("port,p", boost::program_options::value<std::string>(&port)->default_value("7777"),
       "set server port")
      ("password,w", boost::program_options::value<std::string>(&password)->required(),
       "set RCON password")
      ("command,c", boost::program_options::value<std::string>(&command)->required(),
       "set RCON command to be sent")
      ("timeout,t", boost::program_options::value<long>(&timeout_ms)->default_value(250),
       "set connection timeout (in milliseconds)")
    ;

    boost::program_options::variables_map vars;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vars);

    if (vars.count("help")) {
      std::cout << "Usage: " << boost::filesystem::basename(argv[0]) << " [options]\n\n"
                << desc << std::endl;
      std::exit(EXIT_SUCCESS);
    }

    vars.notify();
  }
  catch (boost::program_options::error &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  try {
    boost::asio::io_service io_service;

    rcon_client rcon(io_service);
    rcon.connect(host, port, password);

    rcon.set_timeout_handler([](const boost::system::error_code &error) {
      if (error) {
        std::cerr << boost::system::system_error(error).what() << std::endl;
        std::exit(EXIT_FAILURE);
      } else {
        std::exit(EXIT_SUCCESS);
      }
    });

    auto timeout = boost::posix_time::milliseconds(timeout_ms);

    rcon.set_receive_handler([&rcon, &timeout](const boost::system::error_code &error, std::size_t nbytes) {
      std::cout << rcon.response_text() << std::endl;
      rcon.receive(timeout);
    });

    rcon.send(command);
    rcon.receive(timeout);

    io_service.run();
  }
  catch (boost::system::system_error &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::exit(EXIT_SUCCESS);
}
