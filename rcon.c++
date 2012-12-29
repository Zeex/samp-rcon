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

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

static const char packet_signature[] = {'S', 'A', 'M', 'P'};

enum class packet_opcode : char {
  info          = 'i',
  rules         = 'r',
  client_list   = 'c',
  detailed_info = 'd',
  rcon_command  = 'x',
  ping          = 'p'
};

#pragma pack(push, 1)

struct packet_header {
  char          signature[4];
  std::uint32_t address;
  std::uint16_t port;
  packet_opcode opcode;
};

#pragma pack(pop)

void send_rcon_command(boost::asio::ip::udp::socket &socket,
                       boost::asio::ip::udp::endpoint &endpoint,
                       const std::string &password,
                       const std::string &command)
{
  packet_header header;

  std::memcpy(&header.signature, &packet_signature, sizeof(header.signature));
  header.address = static_cast<std::uint32_t>(endpoint.address().to_v4().to_ulong());
  header.port    = static_cast<std::uint16_t>(endpoint.port());
  header.opcode  = packet_opcode::rcon_command;

  std::vector<boost::asio::const_buffer> buffers = {
    boost::asio::buffer(&header.signature, sizeof(header.signature)),
    boost::asio::buffer(&header.address,   sizeof(header.address)),
    boost::asio::buffer(&header.port,      sizeof(header.port)),
    boost::asio::buffer(&header.opcode,    sizeof(header.opcode))
  };

  std::uint16_t password_length = password.length();
  buffers.push_back(boost::asio::buffer(&password_length, sizeof(password_length)));
  buffers.push_back(boost::asio::buffer(password.c_str(), password.length()));

  std::uint16_t command_length = command.length();
  buffers.push_back(boost::asio::buffer(&command_length, sizeof(command_length)));
  buffers.push_back(boost::asio::buffer(command.c_str(), command.length()));

  socket.send_to(buffers, endpoint);

  std::size_t size = socket.available();
  if (size >= sizeof(packet_header)) {
    size -= socket.receive_from(boost::asio::buffer(&header, sizeof(header)), endpoint);
    while (size > 0) {
      socket.receive_from(boost::asio::buffer(std::cout, size), endpoint);
      size = socket.available();
    }
  }
}

int main(int argc, char **argv) {
  std::string host;
  std::string port;
  std::string password;
  std::string command;
  bool interactive;

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
      ("interactive,i",
       "start in interactive mode")
    ;

    boost::program_options::variables_map vars;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vars);

    if (vars.count("help")) {
      std::cout << "Usage: " << boost::filesystem::basename(argv[0])
                << " [options]\n\n" << desc << std::endl;
      std::exit(EXIT_SUCCESS);
    }

    interactive = vars.count("interactive");

    vars.notify();
  }
  catch (boost::program_options::error &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  try {
    boost::asio::io_service io_service;

    boost::asio::ip::udp::socket socket(io_service);
    boost::asio::ip::udp::endpoint endpoint;

    boost::asio::ip::udp::resolver resolver(io_service);
    boost::asio::ip::udp::resolver::query query(host, port);

    for (auto iterator = resolver.resolve(query);
         iterator != boost::asio::ip::udp::resolver::iterator();
         ++iterator)
    {
      endpoint = *iterator;
      socket.connect(endpoint);
    }

    if (interactive) {
      std::cout << "connected to " << endpoint << std::endl;
      while (getline(std::cin, command)) {
        send_rcon_command(socket, endpoint, password, command);
      }
    } else {
      send_rcon_command(socket, endpoint, password, command);
    }

    socket.close();
  }
  catch (boost::system::system_error &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::exit(EXIT_SUCCESS);
}
