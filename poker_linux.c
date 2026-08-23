#include "poker.c"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>

// Sockets

struct PkSock {
 int sock;
};

int main(int argc, char **argv) {
 poker_main(argc, argv);
 return 0;
}

static PkSock server_socket;
static PkSock client_socket;

static bool8_t server_socket_is_active;
static bool8_t client_socket_is_active;

static bool8_t initialize_networking() {
 server_socket_is_active = false;
 client_socket_is_active = false;

 return true;
}

static PkSock *create_client_socket() {
 if (client_socket_is_active) {
  // TODO: Add error logging
  return 0;
 }

 client_socket.sock = socket(AF_INET, SOCK_DGRAM, 0);
 if (client_socket.sock < 0) {
  // TODO: Add error logging
  return 0;
 }

 int flags = fcntl(client_socket.sock, F_GETFL, 0);
 if (flags < 0 || fcntl(client_socket.sock, F_SETFL, flags | O_NONBLOCK) < 0) {
  // TODO: add error logging
  close(client_socket.sock);
  return 0;
 }

 client_socket_is_active = true;

 return &client_socket;
}

static PkSock *create_server_socket(uint16_t port) {
 if (server_socket_is_active) {
  // TODO: Add error logging
  return 0;
 }

 struct sockaddr_in addr;

 server_socket.sock = socket(AF_INET, SOCK_DGRAM, 0);
 if (server_socket.sock < 0) {
  // TODO: add error logging
  return 0;
 }

 int flags = fcntl(server_socket.sock, F_GETFL, 0);
 if (flags < 0 || fcntl(server_socket.sock, F_SETFL, flags | O_NONBLOCK) < 0) {
  // TODO: add error logging
  close(server_socket.sock);
  return 0;
 }

 memset(&addr, 0, sizeof(addr));
 addr.sin_family = AF_INET;
 addr.sin_addr.s_addr = INADDR_ANY;
 addr.sin_port = htons(port);

 if (bind(server_socket.sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
  // TODO: add error logging
  close(server_socket.sock);
  return 0;
 }

 printf("Listening for UDP messages on port %d...\n", port); // TODO: Remove this
 server_socket_is_active = true;

 return &server_socket;
}

static bool8_t pk_socket_close(PkSock *socket) {
 if (socket == &server_socket) {
  if (!server_socket_is_active) {
   // TODO: add error logging
   return false;
  }

  server_socket_is_active = false;
 } else if (socket == &client_socket) {
  if (!client_socket_is_active) {
   // TODO: add error logging
   return false;
  }

  client_socket_is_active = false;
 } else {
  // TODO: add error logging
  return false;
 }

 close(socket->sock);
 return true;
}

static NBResult receive_packet(PkSock *socket, uint8_t *buffer, uint64_t buffer_size, PkAddr *out_src_addr, uint64_t *out_bytes_read) {
 struct sockaddr_in client_addr;
 memset(&client_addr, 0, sizeof client_addr);

 socklen_t len = sizeof(client_addr);

 *out_bytes_read = 0;

 int bytes_read = recvfrom(socket->sock, buffer, buffer_size, 0, (struct sockaddr*)&client_addr, &len);
 if (bytes_read < 0) {
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
   // TODO: add error logging
   return R_ERROR;
  }

  return R_WAIT;
 }

 *out_bytes_read = bytes_read;
 out_src_addr->port = client_addr.sin_port;
 memcpy(out_src_addr->ip, &client_addr.sin_addr.s_addr, sizeof (uint32_t));

 return R_READY;
}

static NBResult send_packet(PkSock *socket, PkAddr *dest_addr, uint8_t *buffer, uint64_t buffer_size, uint64_t *out_bytes_sent) {
 struct sockaddr_in client_addr;
 memset(&client_addr, 0, sizeof client_addr);

 client_addr.sin_family = AF_INET;
 client_addr.sin_port = dest_addr->port;
 memcpy(&client_addr.sin_addr.s_addr, dest_addr->ip, 4);

 *out_bytes_sent = 0;

 int bytes_sent = sendto(socket->sock, buffer, buffer_size, 0, (struct sockaddr*)&client_addr, sizeof client_addr);
 if (bytes_sent < 0) {
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
   // TODO: add error logging
   return R_ERROR;
  }

  return R_WAIT;
 }

 *out_bytes_sent = bytes_sent;
 return R_READY;
}

// Misc

static void pk_sleep(uint32_t secs) {
 sleep(secs);
}
