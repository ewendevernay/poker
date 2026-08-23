#ifdef POKER_WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "poker.c"

// I have to do this because C doesn't have namespaces
#define Rectangle ___Rectangle
#define CloseWindow ___CloseWindow
#define ShowCursor ___ShowCursor

#include <winsock2.h>
#include <ws2tcpip.h>

#undef Rectangle
#undef CloseWindow
#undef ShowCursor

#include <stdio.h>

struct PkSock {
 SOCKET sock;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
 poker_main(0, 0);
}

static PkSock server_socket;
static PkSock client_socket;

static bool8_t server_socket_is_active;
static bool8_t client_socket_is_active;

static WSADATA wsaData;

static bool8_t initialize_networking() {
 int iResult;

 // Initialize Winsock
 iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
 if (iResult != 0) {
  // TODO: Error logging
  return false;
 }

 server_socket_is_active = false;
 client_socket_is_active = false;

 return true;
}

static PkSock *create_client_socket() {
 if (client_socket_is_active) {
  // TODO: Add error logging
  return 0;
 }

 SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
 if (sock == INVALID_SOCKET) {
  // TODO: Add error logging
  WSACleanup();
  return 0;
 }

 unsigned long ul = 1;
 int nRet = ioctlsocket(sock, FIONBIO, (unsigned long *) &ul);
 if (nRet == SOCKET_ERROR) {
  // TODO: Add error logging
  closesocket(sock);
  WSACleanup();
  return 0;
 }

 client_socket.sock = sock;
 client_socket_is_active = true;

 return &client_socket;
}

static PkSock *create_server_socket(uint16_t port) {
 int iResult;

 if (server_socket_is_active) {
  // TODO: Add error logging
  return 0;
 }

 SOCKET ListenSocket = socket(AF_INET, SOCK_DGRAM, 0);
 if (ListenSocket == INVALID_SOCKET) {
  // TODO: Add error logging
  WSACleanup();
  return 0;
 }

 unsigned long ul = 1;
 int nRet = ioctlsocket(ListenSocket, FIONBIO, (unsigned long *) &ul);
 if (nRet == SOCKET_ERROR) {
  // TODO: Add error logging
  closesocket(ListenSocket);
  WSACleanup();
  return 0;
 }

 struct sockaddr_in addr;
 memset(&addr, 0, sizeof(addr));
 addr.sin_family = AF_INET;
 addr.sin_addr.s_addr = INADDR_ANY;
 addr.sin_port = htons(port);

 // Setup the TCP listening socket
 iResult = bind(ListenSocket, (struct sockaddr*)&addr, sizeof addr);
 if (iResult == SOCKET_ERROR) {
  // TODO: Add error logging
  closesocket(ListenSocket);
  WSACleanup();
  return 0;
 }

 server_socket.sock = ListenSocket;
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

 closesocket(socket->sock);
 return true;
}

static NBResult receive_packet(PkSock *socket, uint8_t *buffer, uint64_t buffer_size, PkAddr *out_src_addr, uint64_t *out_bytes_read) {
 struct sockaddr_in client_addr;
 memset(&client_addr, 0, sizeof client_addr);

 socklen_t len = sizeof(client_addr);

 *out_bytes_read = 0;

 int bytes_read = recvfrom(socket->sock, (char*)buffer, (int)buffer_size, 0, (struct sockaddr*)&client_addr, &len);
 if (bytes_read == SOCKET_ERROR) {
  if (WSAGetLastError() == WSAEWOULDBLOCK) {
   return R_WAIT;
  } else {
   // TODO: Add error logging
   return R_ERROR;
  }
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

 int bytes_sent = sendto(socket->sock, (char*)buffer, (int)buffer_size, 0,
                         (struct sockaddr*)&client_addr, sizeof client_addr);

 if (bytes_sent == SOCKET_ERROR) {
  if (WSAGetLastError() == WSAEWOULDBLOCK) {
   return R_WAIT;
  } else {
   printf("Error code: %d\n", WSAGetLastError());
   // TODO: add error logging
   return R_ERROR;
  }
 }

 *out_bytes_sent = bytes_sent;
 return R_READY;
}

// Misc

static void pk_sleep(uint32_t secs) {
 Sleep(secs * 1000);
}

static void *allocate_pages(uint64_t size) {
 return VirtualAlloc(
  NULL,
  size,
  MEM_COMMIT | MEM_RESERVE,
  PAGE_READWRITE
  );
}
