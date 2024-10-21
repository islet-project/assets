#ifndef _SHARED_REALM_MEMORY_H
#define _SHARED_REALM_MEMORY_H

#define SHRM_CHUNK_SIZE 1024 * 8// TODO: need to defined by config 
#define SHRM_IPA_RANGE_SIZE 0x10000000

#define SHRM_ID_MASK 0xFFF

#define SHRM_RW_IPA_REGION_START 0xC0000000
#define SHRM_RW_IPA_REGION_END SHRM_RW_IPA_REGION_START + SHRM_IPA_RANGE_SIZE
#define RESERVED_SHRM_RW_IPA_REGION_START 0xC0000000
#define RESERVED_SHRM_RW_IPA_REGION_END RESERVED_SHRM_RW_IPA_REGION_START +SHRM_CHUNK_SIZE 

#define SHRM_RO_IPA_REGION_START 0xD0000000
#define SHRM_RO_IPA_REGION_END SHRM_RO_IPA_REGION_START + SHRM_IPA_RANGE_SIZE

#define SHRM_TEMP_TOKEN1 0xAA
#define SHRM_TEMP_TOKEN2 0xBB
#define INVALID_SHRM_TOKEN 0xEE

typedef enum {
	SHRM_RW = 0,
	SHRM_RO = 1,
} SHRM_TYPE;

struct shared_realm_memory {
	struct list_head head;
	u64 ipa;
	u32 shrm_id;
	bool in_use;
	SHRM_TYPE type;
};

struct pos {
	struct shared_realm_memory *shrm;
	u64 offset;
};

struct packet_pos {
	struct pos front, rear; //TODO: use 'head' & 'tail'
	u64 size;
};

int mmio_write_to_remove_shrm(u64 ipa);
int mmio_write_to_unmap_shrm(u64 ipa);

#endif /* _SHARED_REALM_MEMORY_H */
