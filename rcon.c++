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

static const int max_rcon_response_text = 80;

struct rcon_response_packet {
  packet_header header;
  std::uint16_t text_length;
  char          text[max_rcon_response_text];
};

#pragma pack(pop)

void send_rcon_command(boost::asio::ip::udp::socket &socket,
                       boost::asio::ip::udp::endpoint &endpoint,
                       const std::string &password,
                       const std::string &command)
{
  packet_header send_header;

  std::memcpy(&send_header.signature, &packet_signature, sizeof(send_header.signature));
  send_header.address = static_cast<std::uint32_t>(endpoint.address().to_v4().to_ulong());
  send_header.port    = static_cast<std::uint16_t>(endpoint.port());
  send_header.opcode  = packet_opcode::rcon_command;

  std::vector<boost::asio::const_buffer> send_bufs = {
    boost::asio::buffer(&send_header.signature, sizeof(send_header.signature)),
    boost::asio::buffer(&send_header.address,   sizeof(send_header.address)),
    boost::asio::buffer(&send_header.port,      sizeof(send_header.port)),
    boost::asio::buffer(&send_header.opcode,    sizeof(send_header.opcode))
  };

  std::uint16_t password_length = password.length();
  send_bufs.push_back(boost::asio::buffer(&password_length, sizeof(password_length)));
  send_bufs.push_back(boost::asio::buffer(password.c_str(), password.length()));

  std::uint16_t command_length = command.length();
  send_bufs.push_back(boost::asio::buffer(&command_length, sizeof(command_length)));
  send_bufs.push_back(boost::asio::buffer(command.c_str(), command.length()));

  socket.send(send_bufs);

  rcon_response_packet response;

  std::vector<boost::asio::mutable_buffer> recv_bufs = {
    boost::asio::buffer(&response.header.signature, sizeof(response.header.signature)),
    boost::asio::buffer(&response.header.address,   sizeof(response.header.address)),
    boost::asio::buffer(&response.header.port,      sizeof(response.header.port)),
    boost::asio::buffer(&response.header.opcode,    sizeof(response.header.opcode)),
    boost::asio::buffer(&response.text_length,      sizeof(response.text_length)),
    boost::asio::buffer(&response.text,             sizeof(response.text))
  };

  do {
    socket.receive(recv_bufs);
    response.text[response.text_length] = '\0';
    std::cout << response.text << std::endl;
  }
  while (response.text_length > 0);
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
      ("command,c", boost::program_options::value<std::string>(&command),
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
        try {
          send_rcon_command(socket, endpoint, password, command);
        }
        catch (boost::system::system_error &e) {
          std::cerr << e.what() << std::endl;
          continue;
        }
      }
    } else {
      if (!command.empty()) {
        send_rcon_command(socket, endpoint, password, command);
      }
    }

    socket.close();
  }
  catch (boost::system::system_error &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::exit(EXIT_SUCCESS);
}
