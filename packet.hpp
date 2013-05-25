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

#ifndef PACKET_HPP
#define PACKET_HPP

// See this page for more details:
// http://wiki.sa-mp.com/wiki/Query_Mechanism

#include <cstdint>

static const char packet_signature[] = {'S', 'A', 'M', 'P'};

enum class packet_opcode : char {
  info          = 'i',
  rules         = 'r',
  client_list   = 'c',
  detailed_info = 'd',
  rcon_command  = 'x',
  ping          = 'p'
};

struct pod_packet_header {
  char          signature[4];
  std::uint32_t address;
  std::uint16_t port;
  packet_opcode opcode;
};

class packet_header {
 public:
  packet_header(pod_packet_header header);
  packet_header(std::uint32_t address, std::uint16_t port,
                packet_opcode opcode);

  pod_packet_header &pod() {
    return pod_;
  }

  std::uint32_t address() const {
    return pod_.address;
  }

  std::uint16_t port() const {
    return pod_.port;
  }

  packet_opcode opcode() const {
    return pod_.opcode;
  }

  static pod_packet_header make(std::uint32_t address, std::uint16_t port,
                                packet_opcode opcode);

  static bool is_valid(const pod_packet_header &header);

 private:
  pod_packet_header pod_;
};

#endif // !PACKET_HPP
