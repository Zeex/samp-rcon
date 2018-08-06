cmake_minimum_required(VERSION 2.8)
project(rcon)

set(RCON_VERSION 2.0-rc)

configure_file(
  src/rcon_version.h.in
  src/rcon_version.h
)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0501)
endif()

add_executable(rcon
  src/dumbsockets.c
  src/dumbsockets.h
  src/rcon.cpp
)
if(MSVC)
  target_compile_options(rcon PRIVATE /std:c++latest)
endif()
if(CMAKE_COMPILER_IS_GNUCXX)
  target_compile_options(rcon PRIVATE -std=c++1z -Wall)
endif()
