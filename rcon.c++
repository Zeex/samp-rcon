#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <vector>

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

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

int main(int argc, char **argv) {

  if (argc < 5) {
    std::cout << "usage: rcon <host> <port> <password> <command>" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::string host(argv[1]);
  std::string port(argv[2]);
  std::string password(argv[3]);
  std::string command(argv[4]);

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

    packet_header header;

    std::memcpy(&header.signature, &packet_signature, sizeof(header.signature));
    header.address = htonl(static_cast<std::uint32_t>(endpoint.address().to_v4().to_ulong()));
    header.port    = htons(static_cast<std::uint16_t>(endpoint.port()));
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

    std::size_t size;
    while ((size = socket.available()) > 0) {
      socket.receive_from(boost::asio::buffer(std::cout, size), endpoint);
    }

    socket.close();
  }
  catch (boost::system::system_error &e) {
    std::cout << "error: " << e.what() << std::endl;
  }

  std::exit(EXIT_SUCCESS);
}
