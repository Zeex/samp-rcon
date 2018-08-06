// Copyright (c) 2018 Zeex
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
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
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>
#include "dumbsockets.h"
#include "rcon_version.h"

namespace {

template <typename T, typename F>
class AutoClose {
 public:
  AutoClose(T resource, F closeFunc):
    resource_(resource), closeFunc_(closeFunc)
  {}
  ~AutoClose() {
    closeFunc_(resource_);
  }
  AutoClose(const AutoClose &other) = delete;
  void operator =(const AutoClose &other) = delete;

  operator T() const {
    return resource_;
  }
  void SetResource(T resource) {
    resource_ = resource;
  }

 private:
  T resource_;
  F closeFunc_;
};

enum class CLType {
  Bool,
  String,
  Int,
  Long
};

union CLValue {
  const char *stringValue;
  int intValue;
  long longValue;
  bool boolValue;
};

struct CLOption
{
 public:
  CLOption(std::string shortName,
           std::string longName,
           CLType type,
           bool isRequired = false)
    : shortName_(shortName),
      longName_(longName),
      type_(type),
      isRequired_(isRequired)
  {
    assert(!longName.empty());
  }

  const std::string &ShortName() const
    { return shortName_; }
  const std::string &LongName() const
    { return longName_; }
  CLType Type() const
    { return type_; }
  bool IsRequired() const
    { return isRequired_; }
  bool HasValue() const
    { return value_.has_value(); }
  CLValue Value() const
    { return value_.value(); }
  void SetValue(bool value)
    { CLValue v; v.boolValue = value; value_ = v; }
  void SetValue(const char *value)
    { CLValue v; v.stringValue = value; value_ = v; }
  void SetValue(int value)
    { CLValue v; v.intValue = value; value_ = v; }
  void SetValue(long value)
    { CLValue v; v.longValue = value; value_ = v; }

  std::string stringValue() const {
    switch (type_) {
      case CLType::Bool:
        return std::to_string(value_.value().boolValue);
      case CLType::String:
        return value_.value().stringValue;
      case CLType::Int:
        return std::to_string(value_.value().intValue);
      case CLType::Long:
        return std::to_string(value_.value().longValue);
    }
    return "";
  }

 private:
  std::string shortName_;
  std::string longName_;
  CLType type_;
  bool isRequired_;
  std::optional<CLValue> value_;
};

enum class RCONQueryType: char {
  Clients = 'c',
  Details = 'd',
  Info = 'i',
  Rules = 'r',
  Ping = 'p',
  Execute = 'x'
};

class RCONQuery {
 public:
  RCONQuery(RCONQueryType type): type_(type) {}

  RCONQueryType Type() const
    { return type_; }
  const std::string Password() const
    { return password_; }
  void SetPassword(std::string password)
    { password_ = password; }
  const std::vector<std::string> &ExtraData() const
    { return extraData_; }
  void AppendExtraData(std::string value)
    { extraData_.push_back(value); }

 private:
  RCONQueryType type_;
  std::string password_;
  std::vector<std::string> extraData_;
};

#pragma pack(push, 1)

struct RCONQueryPacket {
  char magic[4];
  std::uint32_t address;
  std::uint16_t port;
  std::uint8_t type;
};

static_assert(sizeof(RCONQueryPacket) == 11,
              "Does you compiler support #pragma pack?");

#pragma pack(pop)

typedef std::function<bool(const std::uint8_t *responseData)>
  RCONResponseHandler;

void PrintUsage() {
    std::cerr
      << "Usage: rcon [options]\n\n"
      << "-h, --help                 "
        << "show this message and exit\n"
      << "-s, --host <hostname>      "
        << "name or IP address of SA-MP server (default is 127.0.0.1)\n"
      << "-p, --port <port>          "
        << "server port (default is 7777)\n"
      << "-w, --password <string>    "
        << "RCON password\n"
      << "-c, --command <command>    "
        << "execute command and exit\n"
      << "-t, --timeout <number>     "
        << "command timeout in milliseconds (default is 150ms)\n"
      << "-i, --interactive          "
        << "run in interactive mode\n"
      << std::endl;
}

bool StringStartsWith(const std::string &s, const char *prefix) {
  if (prefix == nullptr) {
    return false;
  }
  std::size_t prefixLength = std::strlen(prefix);
  if (s.length() < prefixLength) {
    return false;
  }
  for (std::size_t i = 0; i < prefixLength; i++) {
    if (s[i] != prefix[i]) {
      return false;
    }
  }
  return true;
}

bool OptionNameMatches(const std::string &token, CLOption *option) {
  if (StringStartsWith(token, "-")
      && token.find(option->ShortName()) == 1) {
    return true;
  }
  if (StringStartsWith(token, "--")
      && token.find(option->LongName()) == 2) {
    return true;
  }
  return false;
}

bool ParseOptions(int argc,
                  char **argv,
                  std::vector<CLOption *> &options,
                  std::string &error) {
  std::unordered_set<std::string> foundOptions;
  for (int i = 1; i < argc; ) {
    std::string possibleName{argv[i++]};
    bool foundOption = false;
    for (auto &option : options) {
      if (!OptionNameMatches(possibleName, option)) {
        continue;
      }
      foundOption = true;
      foundOptions.insert(option->LongName());
      if (option->Type() == CLType::Bool) {
        option->SetValue(true);
        continue;
      }
      if (i >= argc) {
        break;
      }
      const char *value = argv[i++];
      if (StringStartsWith(value, "-")) {
        i--;
        continue;
      }
      switch (option->Type()) {
        case CLType::Bool:
          break;
        case CLType::String:
          option->SetValue(value);
          break;
        case CLType::Int:
          option->SetValue(std::stoi(value));
          break;
        case CLType::Long:
          option->SetValue(std::stol(value));
          break;
      }
    }
    if (!foundOption) {
      error = "Unrecognized option: " + possibleName;
      return false;
    }
  }
  for (auto &option : options) {
    if (option->IsRequired() && !option->HasValue()) {
      if (option->Type() != CLType::Bool
          && foundOptions.find(option->LongName()) != foundOptions.cend()) {
        error = "Option requires a value: --" + option->LongName();
      } else {
        error = "Option is required: --" + option->LongName();
      }
      return false;
    }
  }
  return true;
}

bool SendRCONQuery(
  const std::string &host,
  const std::string &port,
  const RCONQuery &query,
  long timeout,
  const RCONResponseHandler &responseHandler)
{
  addrinfo gaiHints = {0};
  gaiHints.ai_family = AF_INET;
  gaiHints.ai_protocol = IPPROTO_UDP;
  gaiHints.ai_socktype = SOCK_DGRAM;
  addrinfo *gaiResult;
  int gaiError =
    ds_getaddrinfo(host.c_str(), port.c_str(), &gaiHints, &gaiResult);
  if (gaiError != 0) {
    return false;
  }
  AutoClose gaiResultAC(gaiResult, freeaddrinfo);

  ds_socket_t sock = -1;
  addrinfo *addressPtr = gaiResult;
  while (addressPtr != nullptr) {
    sock = ds_socket(
      addressPtr->ai_family,
      addressPtr->ai_socktype,
      addressPtr->ai_protocol);
    if (sock >= 0) {
      break;
    }
    addressPtr = addressPtr->ai_next;
  }
  if (sock < 0) {
    gaiResult = nullptr;
    return false;
  }
  AutoClose sockAC(sock, ds_close);
  
  sockaddr address;
  std::size_t addressLength = addressPtr->ai_addrlen;
  std::memcpy(&address, addressPtr->ai_addr, addressPtr->ai_addrlen);
  auto addressV4 = reinterpret_cast<sockaddr_in *>(&address);
  gaiResult = nullptr;
  addressPtr = nullptr;

  auto outIpAddress = addressV4->sin_addr.s_addr;
  auto outPort = addressV4->sin_port;
  
  RCONQueryPacket outPacket;
  std::memcpy(&outPacket.magic, "SAMP", 4);
  outPacket.address = outIpAddress;
  outPacket.port = outPort;
  outPacket.type = static_cast<std::uint8_t>(query.Type());

  auto outData = std::vector<std::uint8_t>();
  outData.resize(sizeof(RCONQueryPacket));
  std::memcpy(outData.data(), &outPacket, sizeof(outPacket));

  auto requiresPassword = false;
  switch (query.Type()) {
    case RCONQueryType::Clients:
    case RCONQueryType::Details:
    case RCONQueryType::Info:
    case RCONQueryType::Rules:
    case RCONQueryType::Ping:
      requiresPassword = false;
      break;
    case RCONQueryType::Execute:
      requiresPassword = true;
      break;
  }

  auto AppendString = [&](const std::string &s) {
    auto length = static_cast<std::uint16_t>(s.length());
    auto offset = outData.size();
    outData.resize(outData.size() + sizeof(length) + s.length());
    std::memcpy(outData.data() + offset, &length, sizeof(length));
    std::memcpy(
      outData.data() + offset + sizeof(length), s.c_str(), s.length());
  };

  if (requiresPassword) {
    if (query.Password().empty()) {
      return false;
    }
    AppendString(query.Password());
  }
  for (auto &s : query.ExtraData()) {
    AppendString(s);
  }

  auto numBytesSent = sendto(sock,
    reinterpret_cast<const char *>(outData.data()),
    outData.size(),
    0,
    &address,
    addressLength);
  if (numBytesSent <= 0) {
    return false;
  }

  fd_set selectFds;
  FD_ZERO(&selectFds);
  FD_SET(sock, &selectFds);
  timeval selectTimeout;
  selectTimeout.tv_sec = 0;
  selectTimeout.tv_usec = timeout * 1000;

  for (;;) {
    auto selectResult = select(1,
                               &selectFds,
                               nullptr,
                               nullptr,
                               &selectTimeout);
    if (selectResult < 0) {
      return false;
    }
    if (selectResult == 0) {
      return true;
    }

    std::vector<std::uint8_t> inData;
    inData.resize(4096);
    auto numBytesReceived = recvfrom(
      sock,
      reinterpret_cast<char *>(inData.data()),
      inData.size(),
      0,
      nullptr,
      nullptr);
    if (numBytesReceived <= 0) {
      return false;
    }

    auto inPacket = reinterpret_cast<RCONQueryPacket *>(inData.data());
    if (std::memcmp(inPacket, &outPacket, sizeof(RCONQueryPacket)) != 0) {
      return false;
    }

    auto responseData = inData.data() + sizeof(RCONQueryPacket);
    if (!responseHandler(responseData)) {
      return true;
    }
  }

  return true;
}

bool SendRCONCommand(const std::string &host,
                     const std::string &port,
                     const std::string &password,
                     const std::string &command,
                     long timeout,
                     std::string &output)
{
  RCONQuery query{RCONQueryType::Execute};
  query.SetPassword(password);
  query.AppendExtraData(command);

  auto responseHandler = [&](const std::uint8_t *data) {
    std::uint16_t length;
    std::memcpy(&length, data, sizeof(length));
    if (length > 0) {
      std::size_t currentLength = output.size();
      output.resize(currentLength + length + 1);
      std::memcpy(output.data() + currentLength,
                  data + sizeof(length),
                  length);
      output.push_back('\n');
    }
    return true;
  };
  return SendRCONQuery(host, port, query, timeout, responseHandler);
}

} // namespace

int main(int argc, char **argv) {
  CLOption helpOption("h", "help", CLType::Bool, false);
  CLOption hostOption("H", "host", CLType::String, false);
  CLOption portOption("p", "port", CLType::String, false);
  CLOption passwordOption("w", "password", CLType::String, true);
  CLOption commandOption("c", "command", CLType::String, false);
  CLOption timeoutOption("t", "timeout", CLType::Long, false);
  CLOption interactiveOption("i", "interactive", CLType::Bool, false);

  std::string error;
  std::vector<CLOption *> allOptions = {
    &helpOption,
    &hostOption,
    &portOption,
    &passwordOption,
    &commandOption,
    &timeoutOption,
    &interactiveOption};
  bool optionsOk = ParseOptions(argc, argv, allOptions, error);
  if (helpOption.HasValue() && helpOption.Value().boolValue) {
    PrintUsage();
    std::exit(EXIT_FAILURE);
  }
  if (!optionsOk) {
    std::cerr << error << "\n\n";
    PrintUsage();
    std::exit(EXIT_FAILURE);
  }

  std::string host =
    hostOption.HasValue() ? hostOption.Value().stringValue : "127.0.0.1";
  std::string port =
    portOption.HasValue() ? portOption.Value().stringValue : "7777";
  std::string password = passwordOption.Value().stringValue;
  long timeout =
    timeoutOption.HasValue() ? timeoutOption.Value().longValue : 150;

  if (commandOption.HasValue()) {
    std::string command = commandOption.Value().stringValue;
    std::string output;
    if (SendRCONCommand(host, port, password, command, timeout, output)) {
      std::cout << output << std::endl;
    }
  } else if (interactiveOption.HasValue()
             && interactiveOption.Value().boolValue) {
    std::cout << "RCON " RCON_VERSION_STRING "\n" << std::endl;
    std::string command;
    std::cout << ">>> ";
    while (std::getline(std::cin, command)) {
      std::string output;
      if (SendRCONCommand(host, port, password, command, timeout, output)) {
        std::cout << output << std::flush;
      }
      std::cout << ">>> ";
    }
  }

  std::exit(EXIT_SUCCESS);
}
