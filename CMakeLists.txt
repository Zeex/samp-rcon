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

if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0501)
endif()

add_executable(rcon
  src/sampquery/packet.cpp
  src/sampquery/packet.hpp
  src/sampquery/query.cpp
  src/sampquery/query.hpp
  src/rcon.cpp
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_property(TARGET rcon APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++11")
  if(UNIX)
	set_property(TARGET rcon APPEND_STRING PROPERTY LINK_FLAGS "-pthread")
  endif()
endif()

if(MSVC)
  target_compile_options(rcon PRIVATE "$<$<CONFIG:DEBUG>:/MTd>")
  target_compile_options(rcon PRIVATE "$<$<CONFIG:RELEASE>:/MT>")
endif()
if(WIN32)
  target_link_libraries(rcon ws2_32)
endif()
target_link_libraries(rcon ${Boost_LIBRARIES})
