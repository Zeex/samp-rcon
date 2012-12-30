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

class rcon_client {
public:
  rcon_client(boost::asio::io_service &io_service, boost::posix_time::milliseconds timeout)
    : io_service_(io_service),
      timeout_timer_(io_service, timeout),
      socket_(io_service),
      recv_bufs_({
        boost::asio::buffer(&response_.header.signature, sizeof(response_.header.signature)),
        boost::asio::buffer(&response_.header.address,   sizeof(response_.header.address)),
        boost::asio::buffer(&response_.header.port,      sizeof(response_.header.port)),
        boost::asio::buffer(&response_.header.opcode,    sizeof(response_.header.opcode)),
        boost::asio::buffer(&response_.text_length,      sizeof(response_.text_length)),
        boost::asio::buffer(&response_.text,             sizeof(response_.text))
      })
  {
  }

  void connect(const std::string &host, const std::string &port,
               const std::string &password)
  {
    boost::asio::ip::udp::endpoint endpoint;

    boost::asio::ip::udp::resolver resolver(io_service_);
    boost::asio::ip::udp::resolver::query query(host, port);

    for (auto iterator = resolver.resolve(query);
         iterator != boost::asio::ip::udp::resolver::iterator();
         ++iterator)
    {
      endpoint = *iterator;
      socket_.connect(endpoint);
    }

    do_async_receive();
  }

  void disconnect() {
    socket_.close();
  }

  void send(const std::string &password, const std::string &command) {
    packet_header header;

    std::memcpy(&header.signature, &packet_signature, sizeof(header.signature));
    header.address = static_cast<std::uint32_t>(socket_.remote_endpoint().address().to_v4().to_ulong());
    header.port    = static_cast<std::uint16_t>(socket_.remote_endpoint().port());
    header.opcode  = packet_opcode::rcon_command;

    std::vector<boost::asio::const_buffer> send_bufs = {
      boost::asio::buffer(&header.signature, sizeof(header.signature)),
      boost::asio::buffer(&header.address,   sizeof(header.address)),
      boost::asio::buffer(&header.port,      sizeof(header.port)),
      boost::asio::buffer(&header.opcode,    sizeof(header.opcode))
    };

    std::uint16_t password_length = password.length();
    send_bufs.push_back(boost::asio::buffer(&password_length, sizeof(password_length)));
    send_bufs.push_back(boost::asio::buffer(password.c_str(), password.length()));

    std::uint16_t command_length = command.length();
    send_bufs.push_back(boost::asio::buffer(&command_length, sizeof(command_length)));
    send_bufs.push_back(boost::asio::buffer(command.c_str(), command.length()));

    socket_.send(send_bufs);
  }

  std::string output() const {
    return std::string(response_.text, response_.text_length);
  }

  void receive(const boost::system::error_code &error, std::size_t nbytes) {
    std::cout << output() << std::endl;

    if (error == 0) {
      do_async_receive();
    } else {
      disconnect();
    }
  }

  void do_async_receive() {
    auto timeout = std::bind(&rcon_client::timeout, this, std::placeholders::_1);
    timeout_timer_.async_wait(timeout);

    auto handler = std::bind(&rcon_client::receive, this, std::placeholders::_1,
                                                          std::placeholders::_2);
    socket_.async_receive(recv_bufs_, handler);
  }

  void timeout(const boost::system::error_code &error) {
    if (timeout_handler_) {
      timeout_handler_(error);
    }
  }

  template<typename TimeoutHandler>
  void on_timeout(TimeoutHandler handler) {
    timeout_handler_ = handler;
  }

private:
  boost::asio::io_service &io_service_;
  boost::asio::deadline_timer timeout_timer_;;
  std::function<void (
    const boost::system::error_code &)
  > timeout_handler_;
  boost::asio::ip::udp::socket socket_;

private:
  rcon_response_packet response_;
  std::vector<boost::asio::mutable_buffer> recv_bufs_;
};

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
      ("host,a", boost::program_options::value<std::string>(&host)->default_value("localhost"),
       "set server IP address or hostname")
      ("port,p", boost::program_options::value<std::string>(&port)->default_value("7777"),
       "set server port")
      ("password,w", boost::program_options::value<std::string>(&password)->required(),
       "set RCON password")
      ("command,c", boost::program_options::value<std::string>(&command),
       "set RCON command to be sent")
      ("timeout,t", boost::program_options::value<long>(&timeout_ms)->default_value(100),
       "set timeout in milliseconds")
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
    auto timeout = boost::posix_time::milliseconds(timeout_ms);

    rcon_client rcon(io_service, timeout);
    rcon.connect(host, port, password);

    auto exit_timeout = [](const boost::system::error_code &error) {
      if (error) {
        std::cerr << boost::system::system_error(error).what() << std::endl;
        std::exit(EXIT_FAILURE);
      } else {
        std::exit(EXIT_SUCCESS);
      }
    };

    rcon.on_timeout(exit_timeout);
    rcon.send(password, command);

    io_service.run();
  }
  catch (boost::system::system_error &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::exit(EXIT_SUCCESS);
}
