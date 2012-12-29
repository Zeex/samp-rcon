#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

namespace opt = boost::program_options;
namespace net = boost::asio;

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

void send_rcon_command(net::ip::udp::socket &socket,
                       net::ip::udp::endpoint &endpoint,
                       const std::string &password,
                       const std::string &command)
{
  packet_header header;

  std::memcpy(&header.signature, &packet_signature, sizeof(header.signature));
  header.address = htonl(static_cast<std::uint32_t>(endpoint.address().to_v4().to_ulong()));
  header.port    = htons(static_cast<std::uint16_t>(endpoint.port()));
  header.opcode  = packet_opcode::rcon_command;

  std::vector<net::const_buffer> buffers = {
    net::buffer(&header.signature, sizeof(header.signature)),
    net::buffer(&header.address,   sizeof(header.address)),
    net::buffer(&header.port,      sizeof(header.port)),
    net::buffer(&header.opcode,    sizeof(header.opcode))
  };

  std::uint16_t password_length = password.length();
  buffers.push_back(net::buffer(&password_length, sizeof(password_length)));
  buffers.push_back(net::buffer(password.c_str(), password.length()));

  std::uint16_t command_length = command.length();
  buffers.push_back(net::buffer(&command_length, sizeof(command_length)));
  buffers.push_back(net::buffer(command.c_str(), command.length()));

  socket.send_to(buffers, endpoint);

  std::size_t size;
  while ((size = socket.available()) > 0) {
    socket.receive_from(net::buffer(std::cout, size), endpoint);
  }
}

int main(int argc, char **argv) {
  std::string host;
  std::string port;
  std::string password;
  std::string command;
  bool interactive;

  try {
    opt::options_description desc("Available options");
    desc.add_options()
      ("help,h",
       "show this message and exit")
      ("host,a", opt::value<std::string>(&host)->default_value("localhost"),
       "set server IP address or hostname")
      ("port,p", opt::value<std::string>(&port)->default_value("7777"),
       "set server port")
      ("password,w", opt::value<std::string>(&password)->required(),
       "set RCON password")
      ("command,c", opt::value<std::string>(&command)->required(),
       "set RCON command to be sent")
      ("interactive,i",
       "start in interactive mode")
    ;

    opt::variables_map vars;
    opt::store(opt::parse_command_line(argc, argv, desc), vars);

    if (vars.count("help")) {
      std::cout << "Usage: " << boost::filesystem::basename(argv[0])
                << " [options]\n\n" << desc << std::endl;
      std::exit(EXIT_SUCCESS);
    }

    interactive = vars.count("interactive");

    vars.notify();
  }
  catch (opt::required_option &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  try {
    net::io_service io_service;

    net::ip::udp::socket socket(io_service);
    net::ip::udp::endpoint endpoint;

    net::ip::udp::resolver resolver(io_service);
    net::ip::udp::resolver::query query(host, port);

    for (auto iterator = resolver.resolve(query);
         iterator != net::ip::udp::resolver::iterator();
         ++iterator)
    {
      endpoint = *iterator;
      socket.connect(endpoint);
    }

    std::cout << "connected to " << endpoint << std::endl;

    if (interactive) {
      while (getline(std::cin, command)) {
        send_rcon_command(socket, endpoint, password, command);
      }
    } else {
      send_rcon_command(socket, endpoint, password, command);
    }

    socket.close();
  }
  catch (boost::system::system_error &e) {
    std::cerr << e.what() << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::exit(EXIT_SUCCESS);
}
