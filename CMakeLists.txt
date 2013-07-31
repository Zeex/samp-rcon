cmake_minimum_required(VERSION 2.8)
project(rcon)

find_package(Boost 1.49 REQUIRED COMPONENTS
  date_time
  filesystem
  program_options
  regex
  system
)

include_directories(${Boost_INCLUDE_DIRS})

add_executable(rcon
  sampquery/packet.cpp
  sampquery/packet.hpp
  sampquery/query.cpp
  sampquery/query.hpp
  rcon.cpp
)

if(CMAKE_COMPILER_IS_GNUCXX)
  set_property(TARGET rcon APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++11")
  if(UNIX)
	set_property(TARGET rcon APPEND_STRING PROPERTY LINK_FLAGS "-pthread")
  endif()
endif()

if(WIN32)
  target_link_libraries(rcon ws2_32)
endif()
target_link_libraries(rcon ${Boost_LIBRARIES})
