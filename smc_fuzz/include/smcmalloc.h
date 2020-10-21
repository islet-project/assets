/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SMCMALLOC_H
#define SMCMALLOC_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fifo3d.h"

#define TOTALMEMORYSIZE (0x10000)
#define BLKSPACEDIV (4)
#define TOPBITSIZE (20)
#define MAX_NAME_CHARS 50

struct memblk {
	unsigned int address;
	unsigned int size;
	int valid;
};

struct memmod {
	char memory[TOTALMEMORYSIZE];
	unsigned int nmemblk;
	unsigned int maxmemblk;
	unsigned int checkadd;
	struct memblk *memptr;
	struct memblk *memptrend;
	unsigned int mallocdeladd[((TOTALMEMORYSIZE/BLKSPACEDIV)/sizeof(struct memblk))];
	struct memblk *precblock[((TOTALMEMORYSIZE/BLKSPACEDIV)/sizeof(struct memblk))];
	struct memblk *trailblock[((TOTALMEMORYSIZE/BLKSPACEDIV)/sizeof(struct memblk))];
	struct memblk *memblkqueue[((TOTALMEMORYSIZE/BLKSPACEDIV)/sizeof(struct memblk))];
	unsigned int memallocsize[((TOTALMEMORYSIZE/BLKSPACEDIV)/sizeof(struct memblk))];
	unsigned int mallocdeladd_valid[((TOTALMEMORYSIZE/BLKSPACEDIV)/sizeof(struct memblk))];
	unsigned int mallocdeladd_queue[((TOTALMEMORYSIZE/BLKSPACEDIV)/sizeof(struct memblk))];
	unsigned int checksa[4*((TOTALMEMORYSIZE/BLKSPACEDIV)/sizeof(struct memblk))];
	unsigned int checkea[4*((TOTALMEMORYSIZE/BLKSPACEDIV)/sizeof(struct memblk))];
	unsigned int cntdeladd;
	unsigned int ptrmemblkqueue;
	unsigned int mallocdeladd_queue_cnt;
	unsigned int checknumentries;
	unsigned int memerror;
};

struct peret {
	unsigned int tbit;
	unsigned int pow2;
};

void initmem(void);
struct peret priorityencoder(unsigned int);
void *smcmalloc(unsigned int, struct memmod*);
int smcfree(void*, struct memmod *);
#ifdef DEBUG_SMC_MALLOC
void displayblocks(struct memmod *);
void displaymalloctable(struct memmod *);
#endif

#endif /* SMCMALLOC_H */
