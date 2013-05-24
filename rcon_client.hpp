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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/date_time.hpp>

#include "packet.hpp"

class rcon_client {
 public:
  static const int max_response_text = 1024;

  struct response_packet {
    pod_packet_header header;
    std::uint16_t     text_length;
    char              text[max_response_text];
  };

  rcon_client(boost::asio::io_service &io_service,
              const boost::asio::ip::udp::endpoint &endpoint);
  ~rcon_client();

  void send(const std::string &password, const std::string &command);
  void receive();
  void receive(const boost::posix_time::milliseconds &timeout);

  void cancel();

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
  void on_receive(const boost::system::error_code &error, std::size_t nbytes);
  void on_timeout(const boost::system::error_code &error);

 private:
  boost::asio::io_service &io_service_;
  boost::asio::ip::udp::endpoint endpoint_;
  boost::asio::ip::udp::socket socket_;

  boost::asio::deadline_timer timeout_timer_;
  std::function<void (
    const boost::system::error_code &)
  > timeout_handler_;

  response_packet response_;
  std::function<void (
    const boost::system::error_code &,
    std::size_t bytes_transferred)
  > receive_handler_;
};
