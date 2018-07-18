// Copyright (c) 2012-2014 Zeex
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

#ifndef SAMPQUERY_QUERY_HPP
#define SAMPQUERY_QUERY_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/date_time.hpp>

#include "packet.hpp"

namespace sampquery {

typedef packet_opcode query_type;

class query {
 public:
  static const int max_response_text = 1024;

  query(query_type type,
        boost::asio::io_service &io_service,
        boost::asio::ip::udp::endpoint endpoint);
  ~query();

  boost::asio::ip::udp::endpoint endpoint() const {
    return endpoint_;
  }

  void set_endpoint(const boost::asio::ip::udp::endpoint endpoint) {
    endpoint_ = endpoint;
  }

  typedef std::function<void(
    const boost::system::error_code &ec)
  > timeout_handler;

  void set_timeout_handler(timeout_handler handler) {
    timeout_handler_ = handler;
  }

  typedef std::function<void(
    const boost::system::error_code &ec, std::size_t nbytes)
  > receive_handler;

  void set_receive_handler(receive_handler handler) {
    receive_handler_ = handler;
  }

  void send(std::vector<boost::asio::const_buffer> &buffers);

  void receive();
  void receive(const boost::posix_time::milliseconds &timeout);

  void cancel();

  std::string response_text() const;

 private:
  void on_receive(const boost::system::error_code &ec, std::size_t nbytes);
  void on_timeout(const boost::system::error_code &ec);

 private:
  query_type type_;

  boost::asio::io_service &io_service_;
  boost::asio::ip::udp::endpoint endpoint_;
  boost::asio::ip::udp::socket socket_;

  timeout_handler timeout_handler_;
  boost::asio::deadline_timer timeout_timer_;

  struct response_packet_data {
    packet_header_data header;
    std::uint16_t      text_length;
    char               text[max_response_text];
  };

  receive_handler receive_handler_;
  response_packet_data response_;
};

} // namespace sampquery

#endif // SAMPQUERY_QUERY_HPP
