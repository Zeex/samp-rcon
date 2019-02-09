cmake_minimum_required(VERSION 3.0)
project(rcon)

set(RCON_VERSION_MAJOR 2)
set(RCON_VERSION_MINOR 0)
set(RCON_VERSION_PATCH 0)
set(RCON_VERSION 2.0)

configure_file(
  src/rcon_version.h.in
  src/rcon_version.h
)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0501)
endif()

add_executable(rcon
  src/rcon.cpp
  src/rcon_version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/src/rcon_version.h
)
if(WIN32)
  target_link_libraries(rcon ws2_32)
endif()
if(MSVC)
  target_compile_options(rcon PRIVATE /std:c++latest)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR ${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
  target_compile_options(rcon PRIVATE -std=c++17 -Wall)
endif()

install(TARGETS rcon RUNTIME DESTINATION .)

set(CPACK_PACKAGE_NAME rcon)
set(CPACK_PACKAGE_VERSION_MAJOR ${RCON_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${RCON_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${RCON_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${RCON_VERSION})
if(WIN32)
  set(CPACK_PACKAGE_FILE_NAME rcon-${RCON_VERSION}-windows)
  set(CPACK_GENERATOR ZIP)
elseif(APPLE)
  set(CPACK_PACKAGE_FILE_NAME rcon-${RCON_VERSION}-macos)
  set(CPACK_GENERATOR ZIP)
else()
  string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME_LOWER)
  set(CPACK_PACKAGE_FILE_NAME rcon-${RCON_VERSION}-${SYSTEM_NAME_LOWER})
  set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
