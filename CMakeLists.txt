cmake_minimum_required(VERSION 2.8)
project(rcon)

find_package(Boost 1.49 REQUIRED COMPONENTS
  filesystem
  program_options
  system
)

include_directories(${Boost_INCLUDE_DIRS})

add_executable(rcon
  packet.hpp
  rcon.cpp
  rcon_client.cpp
  rcon_client.hpp
)

if(CMAKE_COMPILER_IS_GNUCXX)
  set_property(TARGET rcon APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++11")
  if(UNIX)
	set_property(TARGET rcon APPEND_STRING PROPERTY LINK_FLAGS "-pthread")
  endif()
endif()

target_link_libraries(rcon ${Boost_LIBRARIES})