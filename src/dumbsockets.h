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

#include <sys/types.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef int ds_socklen_t;
  typedef SOCKET ds_socket_t;
#else
  #include <sys/socket.h>
  #include <fcntl.h>
  #include <netdb.h>
  #include <unistd.h>
  typedef socklen_t ds_socklen_t;
  typedef int ds_socket_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

int ds_init();
int ds_getaddrinfo(
  const char *node,
  const char *service,
  const struct addrinfo *hints,
  struct addrinfo **res);
int ds_socket(int domain, int type, int protocol);
int ds_accept(
  ds_socket_t sockfd,
  struct sockaddr *addr,
  ds_socklen_t *addrlen);
int ds_bind(
  ds_socket_t sockfd,
  const struct sockaddr *addr,
  ds_socklen_t addrlen);
int ds_close(ds_socket_t sockfd);
int ds_set_blocking(ds_socket_t sockfd, int mode);

#ifdef __cplusplus
}
#endif
