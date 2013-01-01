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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <boost/asio.hpp>

#include "rcon_client.h++"

rcon_client::rcon_client(boost::asio::io_service &io_service,
                         const boost::asio::ip::udp::endpoint &endpoint)
  : io_service_(io_service),
    endpoint_(endpoint),
    socket_(io_service),
    timeout_timer_(io_service),
    response_(new response_packet)
{
  socket_.open(boost::asio::ip::udp::v4());
}

rcon_client::~rcon_client() {
  socket_.close();
}

void rcon_client::send(const std::string &password,
                       const std::string &command)
{
  packet_header header = {
    PACKET_SIGNATURE_INITIALIZER,
    static_cast<std::uint32_t>(endpoint_.address().to_v4().to_ulong()),
    static_cast<std::uint16_t>(endpoint_.port()),
    packet_opcode::rcon_command,
  };

  std::uint16_t password_length = password.length();
  std::uint16_t command_length  = command.length();

  using boost::asio::buffer;

  std::vector<boost::asio::const_buffer> buffers = {
    buffer(&header.signature, sizeof(header.signature)),
    buffer(&header.address,   sizeof(header.address)),
    buffer(&header.port,      sizeof(header.port)),
    buffer(&header.opcode,    sizeof(header.opcode)),
    buffer(&password_length,  sizeof(password_length)),
    buffer(password),
    buffer(&command_length,   sizeof(command_length)),
    buffer(command),
  };

  socket_.send_to(buffers, endpoint_);
}

void rcon_client::receive() {
  rcon_client::receive(boost::posix_time::milliseconds(0));
}

void rcon_client::receive(const boost::posix_time::milliseconds &timeout) {
  using boost::asio::buffer;

  std::vector<boost::asio::mutable_buffer> buffers = {
    buffer(&response_->header.signature, sizeof(response_->header.signature)),
    buffer(&response_->header.address,   sizeof(response_->header.address)),
    buffer(&response_->header.port,      sizeof(response_->header.port)),
    buffer(&response_->header.opcode,    sizeof(response_->header.opcode)),
    buffer(&response_->text_length,      sizeof(response_->text_length)),
    buffer(&response_->text,             sizeof(response_->text)),
  };

  using std::placeholders::_1;
  using std::placeholders::_2;

  auto receive_handler = std::bind(&rcon_client::on_receive, this, _1, _2);
  socket_.async_receive_from(buffers, endpoint_, receive_handler);

  if (timeout.total_milliseconds() > 0) {
    auto timer_handler = std::bind(&rcon_client::on_timeout, this, _1);
    timeout_timer_.expires_from_now(timeout);
    timeout_timer_.async_wait(timer_handler);
  }
}

void rcon_client::on_receive(const boost::system::error_code &error,
                             std::size_t nbytes) {
  if (receive_handler_) {
    receive_handler_(error, nbytes);
  }
}

void rcon_client::on_timeout(const boost::system::error_code &error) {
  if (error == boost::asio::error::operation_aborted) {
    return;
  }
  if (timeout_handler_) {
    timeout_handler_(error);
  }
}

void rcon_client::cancel() {
  socket_.cancel();
}

std::string rcon_client::response_text() const {
  return std::string(response_->text, response_->text_length);
}
