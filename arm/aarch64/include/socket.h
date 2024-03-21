#ifndef ARM_AARCH64__SOCKET_H
#define ARM_AARCH64__SOCKET_H

#include <inttypes.h>

#define PEER_LIST_MAX  128

typedef struct peer {
	int vm_id;
	int eventfd;
} peer;

typedef struct client {
	bool initialized;
	int vm_id;
	int sock_fd;
	int eventfd;
	int hc_eventfd; // host channel's eventfd
	pthread_t thread;
	int peer_cnt;
	peer peers[PEER_LIST_MAX];
} client;

// create & connect socket fd
client* get_client(const char *socket_path);
void *poll_events(void *c_ptr);
void client_close(client *client);

#endif // ARM_AARCH64__SOCKET_H
