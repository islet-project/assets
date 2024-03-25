#ifndef ARM_AARCH64__SOCKET_H
#define ARM_AARCH64__SOCKET_H

#include <inttypes.h>

#define PEER_LIST_MAX  128

typedef struct Peer {
	int vm_id;
	int eventfd;
} Peer;

typedef struct Client {
	bool initialized;
	int vm_id;
	int sock_fd;
	int eventfd;
	int hc_eventfd; // host channel's eventfd
	pthread_t thread;
	int peer_cnt;
	Peer peers[PEER_LIST_MAX];
} Client;

// create & connect socket fd
Client* get_client(const char *socket_path);
void *poll_events(void *c_ptr);
void client_close(Client *client);

#endif // ARM_AARCH64__SOCKET_H
