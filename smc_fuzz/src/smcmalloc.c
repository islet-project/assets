/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <drivers/arm/private_timer.h>
#include <events.h>
#include "fifo3d.h"
#include <libfdt.h>

#include <power_management.h>
#include <sdei.h>
#include <tftf_lib.h>
#include <timer.h>

#include <plat_topology.h>
#include <platform.h>

/*
 * Priority encoder for enabling proper alignment of returned malloc
 * addresses
 */
struct peret priorityencoder(unsigned int num)
{
	unsigned int topbit = 0U;
	struct peret prt;
	unsigned int cntbit = 0U;

	for (unsigned int i = TOPBITSIZE; i != 0U; i--) {
		if (((num >> i) & 1U) == 1U) {
			if (topbit < i) {
				topbit = i;
			}
			cntbit++;
		}
	}
	if ((num  & 1U) == 1U) {
		cntbit++;
	}

	prt.pow2 = 0U;
	if (cntbit == 1U) {
		prt.pow2 = 1U;
	}
	prt.tbit = topbit;
	return prt;
}

/*
 * Generic malloc function requesting memory.  Alignment of
 * returned memory is the next largest size if not a power
 * of two. The memmod structure is required to represent memory image
 */
void *smcmalloc(unsigned int rsize,
		struct memmod *mmod)
{
	unsigned int alignnum;
	unsigned int modval;
	unsigned int aladd;
	unsigned int mallocdeladd_pos = 0U;
	struct memblk *newblk = NULL;
	bool foundmem = false;
	struct peret prt;
	int incrnmemblk = 0;
	int incrcntdeladd = 0;

	/*
	 * minimum size is 16
	 */
	if (rsize < 16U) {
		rsize = 16U;
	}

	/*
	 * Is size on a power of 2 boundary?  if not select next largest power of 2
	 * to place the memory request in
	 */
	prt = priorityencoder(rsize);
	if (prt.pow2 == 1U) {
		alignnum = 1U << prt.tbit;
	} else {
		alignnum = 1U << (prt.tbit + 1);
	}
	mmod->memptr = (void *)mmod->memory;
	for (unsigned int i = 0U; i < mmod->nmemblk; i++) {
		modval = mmod->memptr->address % alignnum;
		if (modval == 0U) {
			aladd = 0U;
		} else {
			aladd = alignnum - modval;
		}

		/*
		 * Searching sizes and alignments of memory blocks to find a candidate that will
		 * accept the size
		 */
		if ((rsize <= (mmod->memptr->size - aladd)) &&
		    (mmod->memptr->size > aladd) && (mmod->memptr->valid == 1)) {
			foundmem = true;

			/*
			 * Reuse malloc table entries that have been retired.
			 * If none exists create new entry
			 */
			if (mmod->mallocdeladd_queue_cnt > 0U) {
				mmod->mallocdeladd_queue_cnt--;
				mallocdeladd_pos =
					mmod->mallocdeladd_queue[mmod->
								 mallocdeladd_queue_cnt];
			} else {
				mallocdeladd_pos = mmod->cntdeladd;
				incrcntdeladd = 1;
			}

			/*
			 * Determining if the size adheres to power of 2 boundary and
			 * if a retired malloc block
			 * can be utilized from the malloc table
			 */
			if (modval == 0U) {
				if (mmod->ptrmemblkqueue > 0U) {
					newblk = mmod->memblkqueue[mmod->ptrmemblkqueue - 1U];
					mmod->ptrmemblkqueue--;
				} else {
					newblk = mmod->memptrend;
					newblk++;
					incrnmemblk = 1;
				}

				/*
				 * Setting memory block parameters for newly created memory
				 */
				newblk->size = 0U;
				newblk->address = mmod->memptr->address;
				newblk->valid = 1;
				mmod->precblock[mallocdeladd_pos] = newblk;

				/*
				 * Scrolling through the malloc attribute table to
				 * find entries that have values that
				 * match the newly created block and replace them with it
				 */
				unsigned int fadd = newblk->address + newblk->size;
				for (unsigned int j = 0U; j < mmod->cntdeladd; j++) {
					if ((fadd == mmod->mallocdeladd[j]) && (mmod->mallocdeladd_valid[j] == 1U)) {
						mmod->precblock[j] = newblk;
					}
					if ((fadd ==
					     (mmod->mallocdeladd[j] + mmod->memallocsize[j])) && (mmod->mallocdeladd_valid[j] == 1U)) {
						mmod->trailblock[j] = newblk;
					}
				}

				/*
				 * Setting table parameters
				 */
				mmod->mallocdeladd[mallocdeladd_pos] =
					mmod->memptr->address;
				mmod->memallocsize[mallocdeladd_pos] = rsize;
				mmod->memptr->size -= rsize;
				mmod->memptr->address += (rsize);
				mmod->trailblock[mallocdeladd_pos] = mmod->memptr;
				mmod->mallocdeladd_valid[mallocdeladd_pos] = 1U;
				mmod->memptr = (void *)mmod->memory;

				/*
				 * Removing entries from malloc table that can be
				 * merged with other blocks
				 */
				for (unsigned int j = 0U; j < mmod->nmemblk; j++) {
					if (mmod->memptr->valid == 1) {
						if ((mmod->trailblock[mallocdeladd_pos]->address +
						     mmod->trailblock[mallocdeladd_pos]->size) == mmod->memptr->address) {
							if ((mmod->memptr->size ==
							     0U) && (mmod->trailblock[mallocdeladd_pos]->size != 0U)) {
								mmod->memptr->valid = 0;
								mmod->memblkqueue[mmod->ptrmemblkqueue] = mmod->memptr;
								mmod->ptrmemblkqueue++;
								if (mmod->ptrmemblkqueue >= mmod->maxmemblk) {
									mmod->memerror = 1U;
								}
							}
						}
					}
					mmod->memptr++;
				}
			} else {
				/*
				 * Allocating memory that is aligned with power of 2
				 */
				unsigned int nblksize = mmod->memptr->size - rsize - (alignnum - modval);
				if (mmod->ptrmemblkqueue > 0U) {
					newblk = mmod->memblkqueue[mmod->ptrmemblkqueue - 1U];
					mmod->ptrmemblkqueue--;
				} else {
					newblk = mmod->memptrend;
					newblk++;
					incrnmemblk = 1;
				}
				newblk->size = nblksize;
				newblk->address = mmod->memptr->address +
						  (alignnum - modval) + rsize;
				newblk->valid = 1;
				mmod->trailblock[mallocdeladd_pos] = newblk;

				/*
				 * Scrolling through the malloc attribute table to find entries
				 * that have values that
				 * match the newly created block and replace them with it
				 */
				unsigned int fadd = newblk->address + newblk->size;
				for (unsigned int i = 0U; i < mmod->cntdeladd; i++) {
					if ((fadd == mmod->mallocdeladd[i]) && (mmod->mallocdeladd_valid[i] == 1U)) {
					    mmod->precblock[i] = newblk;
					}
					if ((fadd == (mmod->mallocdeladd[i] +
					      mmod->memallocsize[i])) && (mmod->mallocdeladd_valid[i] == 1U)) {
						mmod->trailblock[i] = newblk;
					}
				}

				/*
				 * Setting table parameters
				 */
				mmod->memallocsize[mallocdeladd_pos] = rsize;
				mmod->memptr->size = (alignnum - modval);
				mmod->mallocdeladd[mallocdeladd_pos] = mmod->memptr->address + mmod->memptr->size;
				mmod->precblock[mallocdeladd_pos] = mmod->memptr;
				mmod->mallocdeladd_valid[mallocdeladd_pos] = 1U;
			}
			if (incrcntdeladd == 1) {
				mmod->cntdeladd++;
				if (mmod->cntdeladd >= mmod->maxmemblk) {
					printf("ERROR: size of GENMALLOC table exceeded\n");
					mmod->memerror = 2U;
				}
			}
			break;
		}
		mmod->memptr++;
	}
	if (incrnmemblk == 1) {
		mmod->nmemblk++;
		mmod->memptrend++;
		if (mmod->nmemblk >=
		    ((TOTALMEMORYSIZE / BLKSPACEDIV)/sizeof(struct memblk))) {
			printf("SMC GENMALLOC exceeded block limit of %ld\n",
					((TOTALMEMORYSIZE / BLKSPACEDIV) / sizeof(struct memblk)));
			mmod->memerror = 3U;
		}
	}
	if (foundmem == false) {
		printf("ERROR: SMC GENMALLOC did not find memory region, size is %u\n", rsize);
		mmod->memerror = 4U;
	}

/*
 * Debug functions
 */

#ifdef DEBUG_SMC_MALLOC
	if (mmod->checkadd == 1) {
		for (unsigned int i = 0U; i < mmod->checknumentries; i++) {
			if (((mmod->mallocdeladd[mallocdeladd_pos] >
			      mmod->checksa[i])
			     && (mmod->mallocdeladd[mallocdeladd_pos] <
				 mmod->checkea[i]))
			    || (((mmod->mallocdeladd[mallocdeladd_pos] + rsize) >
				 mmod->checksa[i])
				&& ((mmod->mallocdeladd[mallocdeladd_pos] + rsize) <
				    mmod->checkea[i]))) {
				printf("ERROR: found overlap with previous addressin smc GENMALLOC\n");
				printf("New address %u size %u\n", mmod->mallocdeladd[mallocdeladd_pos], rsize);
				printf("Conflicting address %u size %u\n", mmod->checksa[i], (mmod->checkea[i] - mmod->checksa[i]));
				mmod->memerror = 5U;
			}
		}
		mmod->checksa[mmod->checknumentries] =
			mmod->mallocdeladd[mallocdeladd_pos];
		mmod->checkea[mmod->checknumentries] =
			mmod->mallocdeladd[mallocdeladd_pos] + rsize;
		mmod->checknumentries++;
		if (mmod->checknumentries >= (4U * mmod->maxmemblk)) {
			printf("ERROR: check queue size exceeded\n"); mmod->memerror = 6U;
		}
		mmod->memptr = (void *)mmod->memory;
		for (unsigned int i = 0U; i < mmod->nmemblk; i++) {
			if (mmod->memptr->valid == 1) {
				if (((mmod->mallocdeladd[mallocdeladd_pos] >
				      mmod->memptr->address)
				     && (mmod->mallocdeladd[mallocdeladd_pos] < (mmod->memptr->address + mmod->memptr->size)))
				    || (((mmod->mallocdeladd[mallocdeladd_pos] + rsize) > mmod->memptr->address)
					&& ((mmod->mallocdeladd[mallocdeladd_pos] +
					     rsize) < (mmod->memptr->address + mmod->memptr->size)))) {
					printf("ERROR: found overlap with GENFREE memory region in smc GENMALLOC\n");
					printf("New address %u size %u\n", mmod->mallocdeladd[mallocdeladd_pos], rsize);
					printf("Conflicting address %u size %u\n", mmod->memptr->address, mmod->memptr->size);
					mmod->memerror = 7U;
				}
			}
			mmod->memptr++;
		}
		for (unsigned int i = 0U; i < mmod->cntdeladd; i++) {
			if (mmod->mallocdeladd_valid[i] == 1) {
				mmod->memptr = (void *)mmod->memory;
				for (unsigned int j = 0U; j < mmod->nmemblk; j++) {
					if (mmod->memptr->valid == 1) {
						if (((mmod->mallocdeladd[i] >
						      mmod->memptr->address)
						     && (mmod->mallocdeladd[i] < (mmod->memptr->address + mmod->memptr->size)))
						    || (((mmod->mallocdeladd[i] + mmod->memallocsize[i]) > mmod->memptr->address)
							&& ((mmod->mallocdeladd[i] + mmod->memallocsize[i]) <
								(mmod->memptr->address + mmod->memptr->size)))) {
							printf("ERROR: found overlap with GENFREE memory region ");
							printf("full search in smc GENMALLOC\n");
							printf("New address %u size %u\n", mmod->mallocdeladd[i],
									mmod->memallocsize[i]);
							printf("Conflicting address %u size %u\n", mmod->memptr->address,
									mmod->memptr->size);
							mmod->memerror = 8U;
						}
					}
					mmod->memptr++;
				}
			}
		}
		mmod->memptr = (void *)mmod->memory;
		newblk = (void *)mmod->memory;
		for (unsigned int i = 0U; i < mmod->nmemblk; i++) {
			if (mmod->memptr->valid == 1) {
				for (unsigned int j = 0U; j < mmod->nmemblk; j++) {
					if (newblk->valid == 1) {
						if (((mmod->memptr->address >
						      newblk->address) && (mmod->memptr->address < (newblk->address + newblk->size)))
						    || (((mmod->memptr->address + mmod->memptr->size) >
							 newblk->address) && ((mmod->memptr->address +
							 mmod->memptr->size) < (newblk->address + newblk->size)))) {
							printf("ERROR: found overlap in GENFREE memory regions in smc GENMALLOC\n");
							printf("Region 1 address %u size %u\n", mmod->memptr->address, mmod->memptr->size);
							printf("Region 2 address %u size %u\n", newblk->address, newblk->size);
							mmod->memerror = 9U;
						}
					}
					newblk++;
				}
			}
			mmod->memptr++;
			newblk = (void *)mmod->memory;
		}
	}
#endif
	return (void *)mmod->memory + ((TOTALMEMORYSIZE / BLKSPACEDIV)) +
	       mmod->mallocdeladd[mallocdeladd_pos];
#ifdef DEBUG_SMC_MALLOC
	return (void *)mmod->memory + 0x100U + mmod->mallocdeladd[mallocdeladd_pos];
#endif
}

/*
 * Memory free function for memory allocated from malloc function.
 * The memmod structure is
 * required to represent memory image
 */

int smcfree(void *faddptr,
	    struct memmod *mmod)
{
	unsigned int fadd = faddptr - ((TOTALMEMORYSIZE/BLKSPACEDIV)) -
			    (void *)mmod->memory;
	int fentry = 0;
	struct memblk *newblk = NULL;
	int incrnmemblk = 0;

	/*
	 * Scrolling through the malloc attribute table to find entries that match
	 * the user supplied address
	 */


	for (unsigned int i = 0U; i < mmod->cntdeladd; i++) {
		if ((fadd == mmod->mallocdeladd[i]) &&
		    (mmod->mallocdeladd_valid[i] == 1U)) {
			fentry = 1;
			if (mmod->trailblock[i] != NULL) {
				if ((mmod->precblock[i]->address + mmod->precblock[i]->size) == fadd) {

					/*
					 * Found matching attribute block and then proceed to merge with
					 * surrounding blocks
					 */

					mmod->precblock[i]->size += mmod->memallocsize[i] + mmod->trailblock[i]->size;
					mmod->memblkqueue[mmod->ptrmemblkqueue] = mmod->trailblock[i];
					mmod->ptrmemblkqueue++;
					if (mmod->ptrmemblkqueue >= mmod->maxmemblk) {
						printf("ERROR: GENMALLOC size exceeded in memory block queue\n");
						exit(1);
					}
					mmod->trailblock[i]->valid = 0;
					newblk = mmod->precblock[i];
					mmod->memptr = (void *)mmod->memory;

					/*
					 * Scrolling through the malloc attribute table to find entries that have values that
					 * match the newly merged block and replace them with it
					 */

					for (unsigned int j = 0U; j < mmod->nmemblk; j++) {
						if (mmod->memptr->valid == 1) {
							if ((mmod->trailblock[i]->address + mmod->trailblock[i]->size) == mmod->memptr->address) {
								if ((mmod->memptr->size == 0U) &&
								    (mmod->trailblock[i]->size != 0U)) {
									mmod->memptr->valid = 0;
									mmod->memblkqueue[mmod->ptrmemblkqueue] = mmod->memptr;
									mmod->ptrmemblkqueue++;
									if (mmod->ptrmemblkqueue >= mmod->maxmemblk) {
										printf("ERROR: GENMALLOC size exceeded in memory block queue\n");
										exit(1);
									}
								}
							}
						}
						mmod->memptr++;
					}
				}
			}

			/*
			 * Setting table parameters
			 */

			mmod->mallocdeladd_valid[i] = 0U;
			mmod->mallocdeladd_queue[mmod->mallocdeladd_queue_cnt] = i;
			mmod->mallocdeladd_queue_cnt++;
			if (mmod->mallocdeladd_queue_cnt >= mmod->maxmemblk) {
				printf("ERROR: GENMALLOC reuse queue size exceeded\n");
				exit(1);
			}

			/*
			 * Scrolling through the malloc attribute table to find entries
			 * that have values that
			 * match the newly merged block and replace them with it
			 */

			unsigned int faddGENFREE = newblk->address + newblk->size;
			for (unsigned int j = 0U; j < mmod->cntdeladd; j++) {
				if ((faddGENFREE == mmod->mallocdeladd[j]) &&
				    (mmod->mallocdeladd_valid[j] == 1U))
					mmod->precblock[j] = newblk;
				if ((faddGENFREE ==
				     (mmod->mallocdeladd[j] +
				      mmod->memallocsize[i])) &&
				    (mmod->mallocdeladd_valid[j] == 1U))
					mmod->trailblock[j] = newblk;
			}
		}
	}
	if (incrnmemblk == 1) {
		mmod->nmemblk++;
		mmod->memptrend++;
		if (mmod->nmemblk >=
		    ((TOTALMEMORYSIZE / BLKSPACEDIV) / sizeof(struct memblk))) {
			printf("SMC GENFREE exceeded block limit of %ld\n",
					((TOTALMEMORYSIZE / BLKSPACEDIV) / sizeof(struct memblk)));
			exit(1);
		}
	}
	if (fentry == 0) {
		printf("ERROR: smcGENFREE cannot find address to GENFREE %u\n", fadd);
		exit(1);
	}
#ifdef DEBUG_SMC_MALLOC

/*
 * Debug functions
 */

	if (mmod->checkadd == 1) {
		for (unsigned int i = 0U; i < mmod->checknumentries; i++) {
			if (fadd == mmod->checksa[i]) {
				mmod->checksa[i] = 0U;
				mmod->checkea[i] = 0U;
			}
		}
		mmod->memptr = (void *)mmod->memory;
		newblk = (void *)mmod->memory;
		for (unsigned int i = 0U; i < mmod->nmemblk; i++) {
			if (mmod->memptr->valid == 1) {
				for (unsigned int j = 0U; j < mmod->nmemblk; j++) {
					if (newblk->valid == 1) {
						if (((mmod->memptr->address > newblk->address)
						     && (mmod->memptr->address < (newblk->address + newblk->size)))
						    || (((mmod->memptr->address + mmod->memptr->size) > newblk->address)
							&& ((mmod->memptr->address + mmod->memptr->size) < ((newblk->address + newblk->size))))) {
							printf("ERROR: found overlap in GENFREE memory regions in smc GENMALLOC\n");
							printf("Region 1 address %u size %u\n", mmod->memptr->address, mmod->memptr->size);
							printf("Region 2 address %u size %u\n", newblk->address, newblk->size);
						}
					}
					newblk++;
				}
			}
			mmod->memptr++;
			newblk = (void *)mmod->memory;
		}
	}
#endif
	return 0;
}

/*
 * Diplay malloc tables for debug purposes
 */

#ifdef DEBUG_SMC_MALLOC
void displayblocks(struct memmod *mmod)
{
	mmod->memptr = (void *)mmod->memory;
	printf("Displaying blocks:\n");
	for (unsigned int i = 0U; i < mmod->nmemblk; i++) {
		if (mmod->memptr->valid == 1) {
			printf("*********************************************************************************************\n");
			printf("%u * Address: %u * Size: %u * Valid: %u *\n", i, mmod->memptr->address, mmod->memptr->size, mmod->memptr->valid);
		}
		mmod->memptr++;
	}
}

void displaymalloctable(struct memmod *mmod)
{
	printf("\n\nDisplaying GENMALLOC table\n");
	for (unsigned int i = 0U; i < mmod->cntdeladd; i++) {
		if (mmod->mallocdeladd_valid[i] == 1U) {
			printf("**********************************************************************************************\n");
			printf("GENMALLOC Address: %u\n", mmod->mallocdeladd[i]);
			printf("**********************************************************************************************\n");
			printf("GENMALLOC Size: %u\n", mmod->memallocsize[i]);
			printf("**********************************************************************************************\n");
			if (mmod->trailblock[i] != NULL) {
				printf("Trail Block:\n");
				printf("* Address: %u * Size: %u *\n",
				       mmod->trailblock[i]->address,
				       mmod->trailblock[i]->size);
			}
			printf("**********************************************************************************************\n");
			if (mmod->precblock[i] != NULL) {
				printf("Previous Block:\n");
				printf("* Address: %u * Size: %u *\n",
				       mmod->precblock[i]->address,
				       mmod->precblock[i]->size);
			}
			printf("**********************************************************************************************\n\n\n");
		}
	}
}
#endif
