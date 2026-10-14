#pragma once

/* Useful stuff */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define false 0
#define true 1

/* Networking abstract API layer */

typedef struct PkSock PkSock;

typedef struct {
 uint8_t ip[4];
 uint16_t port;
} PkAddr;

typedef enum {
 R_ERROR = 0,
 R_READY,
 R_WAIT
} NBResult;

static bool8_t initialize_networking();
static PkSock *create_server_socket(uint16_t port);
static PkSock *create_client_socket();
static bool8_t pk_socket_close(PkSock *socket);
static NBResult receive_packet(PkSock *socket, uint8_t *buffer, uint64_t buffer_size, PkAddr *out_src_addr, uint64_t *out_bytes_read);
static NBResult send_packet(PkSock *socket, PkAddr *dest_addr, uint8_t *buffer, uint64_t buffer_size, uint64_t *out_bytes_sent);

/* Miscalleanous abstract API layer */

static void *allocate_pages(uint64_t size);
static void pk_sleep(uint32_t secs);
