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

#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "query.hpp"

namespace {

template<typename T>
static boost::asio::const_buffers_1 ref_buffer(const T &x) {
  return boost::asio::buffer(&x, sizeof(x));
}

template<typename T>
static boost::asio::mutable_buffers_1 ref_buffer(T &x) {
  return boost::asio::buffer(&x, sizeof(x));
}

} // anonymous namespace

namespace sampquery {

query::query(query_type type,
             boost::asio::io_service &io_service,
             boost::asio::ip::udp::endpoint endpoint):
  type_(type), 
  io_service_(io_service),
  endpoint_(endpoint),
  socket_(io_service),
  timeout_timer_(io_service),
  response_()
{
  socket_.open(boost::asio::ip::udp::v4());
}

query::~query() {
  boost::system::error_code ec;
  socket_.shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);
  socket_.close();
}

void query::send(std::vector<boost::asio::const_buffer> &their_buffers) {
  using boost::asio::buffer;
  
  packet_header_data header = packet_header::make(
    static_cast<std::uint32_t>(endpoint_.address().to_v4().to_ulong()),
    static_cast<std::uint16_t>(endpoint_.port()),
    static_cast<packet_opcode>(type_)
  );

  std::vector<boost::asio::const_buffer> buffers;
  buffers.push_back(ref_buffer(header.signature));
  buffers.push_back(ref_buffer(header.address));
  buffers.push_back(ref_buffer(header.port));
  buffers.push_back(ref_buffer(header.opcode));

  std::copy(their_buffers.begin(), their_buffers.end(),
            std::back_inserter(buffers));

  socket_.send_to(buffers, endpoint_);
}

void query::receive() {
  query::receive(boost::posix_time::milliseconds(0));
}

void query::receive(const boost::posix_time::milliseconds &timeout) {
  using boost::asio::buffer;

  std::vector<boost::asio::mutable_buffer> buffers;
  buffers.push_back(ref_buffer(response_.header.signature));
  buffers.push_back(ref_buffer(response_.header.address));
  buffers.push_back(ref_buffer(response_.header.port));
  buffers.push_back(ref_buffer(response_.header.opcode));
  buffers.push_back(ref_buffer(response_.text_length));
  buffers.push_back(ref_buffer(response_.text));

  using std::placeholders::_1;
  using std::placeholders::_2;

  auto receive_handler = std::bind(&query::on_receive, this, _1, _2);
  socket_.async_receive_from(buffers, endpoint_, receive_handler);

  if (timeout.total_milliseconds() > 0) {
    auto timer_handler = std::bind(&query::on_timeout, this, _1);
    timeout_timer_.expires_from_now(timeout);
    timeout_timer_.async_wait(timer_handler);
  }
}

void query::on_receive(const boost::system::error_code &ec,
                       std::size_t nbytes) {
  if (packet_header::is_valid(response_.header) || ec) {
    if (receive_handler_) {
      receive_handler_(ec, nbytes);
    }
  }
}

void query::on_timeout(const boost::system::error_code &ec) {
  if (ec != boost::asio::error::operation_aborted) {
    if (timeout_handler_) {
      timeout_handler_(ec);
    }
    cancel();
  }
}

void query::cancel() {
  boost::system::error_code ec;
  socket_.shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);
  socket_.close();
  socket_.open(boost::asio::ip::udp::v4());
}

std::string query::response_text() const {
  return std::string(response_.text, response_.text_length);
}

} // namespace sampquery
