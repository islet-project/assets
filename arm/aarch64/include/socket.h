#ifndef ARM_AARCH64__SOCKET_H
#define ARM_AARCH64__SOCKET_H

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/list.h>
#include <linux/bitmap.h>

#define INVALID_PEER_ID -1
#define PEER_LIST_MAX  128
#define HOST_CHANNEL_PATH "/dev/host_channel"
#define SHM_ALLOC_EFD_ID 0

#define INTER_REALM_SHM_SIZE (1 << 12) // 4KB or 2MB only
#define INTER_REALM_SHM_IPA_BASE 0xC0000000
#define INTER_REALM_SHM_IPA_END 0xC0000000 + MAX_SHRM_IPA_SIZE_PER_REALM

#define MAX_SHRM_IPA_SIZE_PER_REALM 0x10000000 // 256 MB
#define MIN_IPA_REGION_SIZE (1 << 12)

struct shared_realm_memory {
	struct list_head list;
    u64 ipa, va;
	int owner_vmid;
	bool mapped_to_realm;
};

typedef struct Peer {
	int id; // NOTE: It's not same with VMID
	int eventfd;
} Peer;

typedef struct Client {
	bool initialized;
	int vmid; // current realm VMID
	int sock_fd;
	int eventfd;
	int shm_alloc_efd;
	pthread_t thread;
	int shm_id;
	int peer_cnt;
	uint32_t ioeventfd_addr;
	struct kvm *kvm; // need it to setup ioeventfd
	struct list_head dyn_shrms_head; // shrm will be keeped in this list until current realm request a shrm
	u64 shrm_ipa_start;
	Peer peers[PEER_LIST_MAX];
	DECLARE_BITMAP(ipa_bits, MAX_SHRM_IPA_SIZE_PER_REALM / MIN_IPA_REGION_SIZE);
} Client;

// create & connect socket fd
Client* get_client(const char *socket_path, uint32_t mmio_addr, struct kvm *kvm);
void *poll_events(void *c_ptr);
void close_client(Client *client);
bool is_valid_shm_id(Client *client, int shm_id);
int get_vmid(void);
int client_init(struct kvm* kvm);
int set_ioeventfd(Client *client, int eventfd, int peer_id);
int create_polling_thread(Client *client);
bool is_mapped(u64 ipa);
void set_ipa_bit(u64 ipa);
void clear_ipa_bit(u64 ipa);
u64 get_unmapped_ipa(void);

#endif // ARM_AARCH64__SOCKET_H
