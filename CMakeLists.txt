cmake_minimum_required(VERSION 2.8)
project(rcon)

add_executable(rcon
	rcon.c++
)

if(CMAKE_COMPILER_IS_GNUCXX)
	set_property(TARGET rcon APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++11")
endif()

find_package(Boost 1.49 REQUIRED COMPONENTS
	filesystem
	program_options
	system
)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(rcon ${Boost_LIBRARIES} "pthread")
