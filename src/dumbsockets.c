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

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <stdio.h>
#else
  #include <unistd.h>
#endif
#include "dumbsockets.h"

#ifdef _MSC_VER
  #pragma comment(lib, "ws2_32.lib")
#endif

#ifdef _WIN32

static BOOL ws2_ready = FALSE;
static WSADATA ws2_data;

static int ds_init() {
  int result;
  if (ws2_ready) {
    /* Already initialized WinSock2, nothing to do. */
    return TRUE;
  }
  result = WSAStartup(MAKEWORD(2, 2), &ws2_data);
  if (result != 0) {
    fprintf(stderr, "Failed to initialize WinSock2: %d\n", result);
    return FALSE;
  }
  ws2_ready = TRUE;
  return TRUE;
}

#endif

int ds_getaddrinfo(
  const char *node,
  const char *service,
  const struct addrinfo *hints,
  struct addrinfo **res)
{
#ifdef _WIN32
  if (!ds_init()) {
    return -1;
  }
#endif
  return getaddrinfo(node, service, hints, res);
}

int ds_socket(int domain, int type, int protocol) {
#ifdef _WIN32
  if (!ds_init()) {
    return -1;
  }
#endif
  return socket(domain, type, protocol);
}

int ds_accept(
  ds_socket_t sockfd,
  struct sockaddr *addr,
  ds_socklen_t *addrlen) {
#ifdef _WIN32
  if (!ds_init()) {
    return -1;
  }
#endif
  return accept(sockfd, addr, addrlen);
}

int ds_bind(
  ds_socket_t sockfd,
  const struct sockaddr *addr,
  ds_socklen_t addrlen) {
#ifdef _WIN32
  if (!ds_init()) {
    return -1;
  }
#endif
  return bind(sockfd, addr, addrlen);
}

int ds_close(ds_socket_t sockfd) {
#ifdef _WIN32
  return closesocket(sockfd);
#else
  return close(sockfd);
#endif
}

int ds_set_blocking(ds_socket_t sockfd, int mode) {
#ifdef _WIN32
  u_long arg = mode == 0;
  return ioctlsocket(sockfd, FIONBIO, &arg);
#else
  int flags = fcntl(sockfd, F_GETFL);
  if (mode == 0) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }
  return fcntl(sockfd, F_SETFL, flags);
#endif
}
