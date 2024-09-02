#ifndef _SHARED_REALM_MEMORY_H
#define _SHARED_REALM_MEMORY_H

#define SHRM_CHUNK_SIZE 1024 * 4// TODO: need to defined by config 
#define SHRM_IPA_RANGE_SIZE 0x10000000

#define SHRM_ID_MASK 0xFF

#define SHRM_RW_IPA_REGION_START 0xC0000000
#define SHRM_RW_IPA_REGION_END SHRM_RW_IPA_REGION_START + SHRM_IPA_RANGE_SIZE
#define SHRM_RO_IPA_REGION_START 0xD0000000
#define SHRM_RO_IPA_REGION_END SHRM_RO_IPA_REGION_START + SHRM_IPA_RANGE_SIZE

struct shared_realm_memory {
	struct list_head head;
	u64 ipa;
	u32 shrm_id;
	bool in_use;
};

struct pos {
	struct shared_realm_memory *shrm;
	u64 offset;
};

struct packet_pos {
	struct pos front, rear;
	u64 size;
};

typedef enum {
	SHRM_RO = 0,
	SHRM_RW = 1,
} SHRM_TYPE;

s64 mmio_read_to_get_shrm(SHRM_TYPE shrm_type);
int mmio_write_to_remove_shrm(u64 ipa);
int mmio_write_to_unmap_shrm(u64 ipa);

#endif /* _SHARED_REALM_MEMORY_H */
