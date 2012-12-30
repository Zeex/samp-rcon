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
#include <functional>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/date_time.hpp>

#include "packet.h++"

const int max_rcon_response_text = 80;

struct rcon_response_packet {
  packet_header header;
  std::uint16_t text_length;
  char          text[max_rcon_response_text];
};

class rcon_client {
public:
  rcon_client(boost::asio::io_service &io_service,
              boost::posix_time::milliseconds timeout);
  ~rcon_client();

  void connect(const std::string &host, const std::string &port,
               const std::string &password);
  void disconnect();

  void send(const std::string &command);

  template<typename TimeoutHandler>
  void set_timeout_handler(TimeoutHandler handler) {
    timeout_handler_ = handler;
  }

  template<typename ReceiveHandler>
  void set_receive_handler(ReceiveHandler handler) {
    receive_handler_ = handler;
  }

  std::string response_text() const;

private:
  void do_async_receive();

  void on_receive(const boost::system::error_code &error, std::size_t nbytes);
  void on_timeout(const boost::system::error_code &error);

private:
  boost::asio::io_service &io_service_;

  boost::asio::ip::udp::socket socket_;
  std::string password_;

  boost::asio::deadline_timer timeout_timer_;;
  std::function<void (
    const boost::system::error_code &)
  > timeout_handler_;

  rcon_response_packet response_;
  std::vector<boost::asio::mutable_buffer> recv_bufs_;
  std::function<void (
    const boost::system::error_code &,
    std::size_t bytes_transferred)
  > receive_handler_;
};
