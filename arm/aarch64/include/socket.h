#ifndef ARM_AARCH64__SOCKET_H
#define ARM_AARCH64__SOCKET_H

#include <inttypes.h>

#define PEER_LIST_MAX  128

typedef struct Peer {
	int id; // NOTE: It's not same with VMID
	int eventfd;
} Peer;

typedef struct Client {
	bool initialized;
	int id; // NOTE: It's not same with VMID
	int sock_fd;
	int eventfd;
	int hc_eventfd; // host channel's eventfd
	pthread_t thread;
	int shm_id;
	int peer_cnt;
	Peer peers[PEER_LIST_MAX];
} Client;

// create & connect socket fd
Client* get_client(const char *socket_path);
void *poll_events(void *c_ptr);
void close_client(Client *client);
bool is_valid_shm_id(Client *client, int shm_id);

#endif // ARM_AARCH64__SOCKET_H
