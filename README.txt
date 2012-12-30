I just needed an RCON client for Linux once, so here it is...

Usage: rcon [options]

Available options:
  -h [ --help ]                  show this message and exit
  -a [ --host ] arg (=localhost) set server IP address or hostname
  -p [ --port ] arg (=7777)      set server port
  -w [ --password ] arg          set RCON password
  -c [ --command ] arg           set RCON command to be sent
  -t [ --timeout ] arg (=100)    set connection timeout (in milliseconds)

To build this you will need Boost 1.49+ and CMake 2.8+

  sudo apt-get install libboost-dev-all
  sudo apt-get install cmake
  cd /path/to/samp-rcon/
  cmake .
  make
