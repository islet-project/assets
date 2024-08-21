#ifndef _SHARED_REALM_MEMORY_H
#define _SHARED_REALM_MEMORY_H
#define SHRM_CHUNK_SIZE PAGE_SIZE // TODO: need to defined by config 

#define MAX_SHRM_IPA_SIZE 0x10000000

#define SHRM_RW_IPA_START 0xC0000000
#define SHRM_RW_IPA_END SHRM_RW_IPA_START + MAX_SHRM_IPA_SIZE
#define SHRM_RO_IPA_START 0xD0000000
#define SHRM_RO_IPA_END SHRM_RO_IPA_START + MAX_SHRM_IPA_SIZE

struct shared_realm_memory {
	struct list_head head;
	u64 ipa;
};

struct pos {
	struct shared_realm_memory *shrm;
	u64 offset;
};

struct packet_pos {
	struct pos front, rear;
	u64 size;
};

#endif /* _SHARED_REALM_MEMORY_H */
