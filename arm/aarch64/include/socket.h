#ifndef ARM_AARCH64__SOCKET_H
#define ARM_AARCH64__SOCKET_H

#include <inttypes.h>

#define PEER_LIST_MAX  128
#define HOST_CHANNEL_PATH "/dev/host_channel"
#define SHM_ALLOC_EFD_ID 0

typedef struct Peer {
	int id; // NOTE: It's not same with VMID
	int eventfd;
} Peer;

typedef struct Client {
	bool initialized;
	int id; // NOTE: It's an id allocated by Eventfd Allocator Server. It's not VMID.
	int sock_fd;
	int eventfd;
	int shm_alloc_efd;
	pthread_t thread;
	int shm_id;
	int peer_cnt;
	uint32_t mmio_addr;
	struct kvm *kvm; // need it to setup ioeventfd
	Peer peers[PEER_LIST_MAX];
} Client;

// create & connect socket fd
Client* get_client(const char *socket_path, uint32_t mmio_addr, struct kvm *kvm);
void *poll_events(void *c_ptr);
void close_client(Client *client);
bool is_valid_shm_id(Client *client, int shm_id);

#endif // ARM_AARCH64__SOCKET_H
