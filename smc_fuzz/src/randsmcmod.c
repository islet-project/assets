/*
 * Copyright (c) 2020, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
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

extern char _binary___dtb_start[];

struct memmod tmod __aligned(65536) __section("smcfuzz");

/*
 * switch to use either standard C malloc or custom SMC malloc
 */

#define FIRST_NODE_DEVTREE_OFFSET (8)

#ifdef SMC_FUZZ_TMALLOC
#define GENMALLOC(x)	malloc((x))
#define GENFREE(x)	free((x))
#else
#define GENMALLOC(x)	smcmalloc((x), mmod)
#define GENFREE(x)	smcfree((x), mmod)
#endif

/*
 * Device tree parameter struct
 */

struct fdt_header_sf {
	unsigned int magic;
	unsigned int totalsize;
	unsigned int off_dt_struct;
	unsigned int off_dt_strings;
	unsigned int off_mem_rsvmap;
	unsigned int version;
	unsigned int last_comp_version;
	unsigned int boot_cpuid_phys;
	unsigned int size_dt_strings;
	unsigned int size_dt_struct;
};

/*
 * Structure to read the fields of the device tree
 */
struct propval {
	unsigned int len;
	unsigned int nameoff;
};

/*
 * Converting from big endian to little endian to read values
 * of device tree
 */
unsigned int lendconv(unsigned int val)
{
	unsigned int res;

	res = val << 24;
	res |= ((val << 8) & 0xFF0000U);
	res |= ((val >> 8) & 0xFF00U);
	res |= ((val >> 24) & 0xFFU);
	return res;
}

/*
 * Function to read strings from device tree
 */
void pullstringdt(void **dtb,
		  void *dtb_beg,
		  unsigned int offset,
		  char *cset)
{
	int fistr;
	int cntchr;
	char rval;

	if (offset != 0U) {
		*dtb = dtb_beg + offset;
	}
	fistr = 0;

	cntchr = 0;
	while (fistr == 0) {
		rval = *((char *)*dtb);
		*dtb += sizeof(char);
		cset[cntchr] = rval;
		if (cset[cntchr] == 0) {
			fistr = 1;
		}
		cntchr++;
	}

	if ((cntchr % 4) != 0) {
		for (unsigned int i = 0U; (int)i < (4 - (cntchr % 4)); i++) {
			*dtb += sizeof(char);
		}
	}
}

/*
 * Structure for Node information extracted from device tree
 */
struct rand_smc_node {
	int *biases;				 // Biases of the individual nodes
	int *biasarray;				 // Array of biases across all nodes
	char **snames;				 // String that is unique to the SMC call called in test
	struct rand_smc_node *treenodes;	 // Selection of nodes that are farther down in the tree
						// that reference further rand_smc_node objects
	int *norcall;				// Specifies whether a particular node is a leaf node or tree node
	int entries;				// Number of nodes in object
	int biasent;				// Number that gives the total number of entries in biasarray
						// based on all biases of the nodes
	char **nname;				// Array of node names
};


/*
 * Create bias tree from given device tree description
 */

struct rand_smc_node *createsmctree(int *casz,
				    struct memmod *mmod)
{
	void *dtb;
	void *dtb_pn;
	void *dtb_beg;
	struct fdt_header fhd;
	unsigned int rval;
	struct propval pv;
	char cset[MAX_NAME_CHARS];
	char nodename[MAX_NAME_CHARS];
	int dtdone;
	struct fifo3d f3d;
	int leafnode = 0;
	unsigned int fnode = 0U;
	unsigned int bias_count = 0U;
	unsigned int bintnode = 0U;
	unsigned int treenodetrack = 0U;
	struct fdt_header *fhdptr;
	struct rand_smc_node *ndarray = NULL;
	int cntndarray;
	struct rand_smc_node nrnode;
	struct rand_smc_node *tndarray;

	f3d.col = 0;
	f3d.curr_col = 0;

	/*
	 * Read device tree header and check for valid type
	 */

	fhdptr = (struct fdt_header *)_binary___dtb_start;

	if (fdt_check_header((void *)fhdptr) != 0) {
		printf("ERROR, not device tree compliant\n");
	}
	fhd = *fhdptr;
	cntndarray = 0;
	nrnode.entries = 0;

	/*
	 * Create pointers to device tree data
	 */
	dtb = _binary___dtb_start;
	dtb_pn = _binary___dtb_start;

	dtb_beg = dtb;
	fhd = *((struct fdt_header *)dtb);
	dtb += (fdt32_to_cpu(fhd.off_dt_struct) + FIRST_NODE_DEVTREE_OFFSET);
	dtdone = 0;

	/*
	 * Reading device tree file
	 */
	while (dtdone == 0) {
		rval = *((unsigned int *)dtb);
		dtb += sizeof(unsigned int);

		/*
		 * Reading node name from device tree and pushing it into the raw data
		 * Table of possible values reading from device tree binary file:
		 * 1	New node found within current tree, possible leaf or tree variant
		 * 2 	Node termination of current hiearchy.
		 *     Could indicate end of tree or preparation for another branch
		 * 3 	Leaf node indication where a bias with a function name should be
		 *     found for the current node
		 * 9  	End of device tree file and we end the read of the bias tree
		 */
		if (fdt32_to_cpu(rval) == 1) {
			pullstringdt(&dtb, dtb_beg, 0U, cset);
			push_3dfifo_col(&f3d, cset, mmod);
			strlcpy(nodename, cset, MAX_NAME_CHARS);

			/*
			 * Error checking to make sure that bias is specified
			 */
			if (fnode == 0U) {
				fnode = 1U;
			} else {
				if (!((fnode == 1U) && (bias_count == 1U))) {
					printf("ERROR: Did not find bias or multiple bias ");
					printf("designations before %s %u %u\n",
					cset, fnode, bias_count);
				}
				bias_count = 0U;
			}
		}

		/*
		 * Reading node parameters of bias and function name
		 */
		if (fdt32_to_cpu(rval) == 3) {
			pv = *((struct propval *)dtb);
			dtb += sizeof(struct propval);
			pullstringdt(&dtb_pn, dtb_beg,
				     (fdt32_to_cpu(fhd.off_dt_strings) +
				      fdt32_to_cpu(pv.nameoff)), cset);
			if (strcmp(cset, "bias") == 0) {
				rval = *((unsigned int *)dtb);
				dtb += sizeof(unsigned int);
				push_3dfifo_bias(&f3d, fdt32_to_cpu(rval));
				bias_count++;
				if (bintnode == 1U) {
					fnode = 0U;
					bintnode = 0U;
					bias_count = 0U;
				}
			}
			if (strcmp(cset, "functionname") == 0) {
				pullstringdt(&dtb, dtb_beg, 0, cset);
				push_3dfifo_fname(&f3d, cset);
				leafnode = 1;
				if (bias_count == 0U) {
					bintnode = 1U;
					fnode = 1U;
				} else {
					bias_count = 0U;
					fnode = 0U;
				}
			}
		}

		/*
		 * Node termination and evaluate whether the bias tree requires addition.
		 * The non tree nodes are added.
		 */
		if (fdt32_to_cpu(rval) == 2) {
			if ((fnode > 0U) || (bias_count > 0U)) {
				printf("ERROR: early node termination... ");
				printf("no bias or functionname field for leaf node, near %s %u\n",
				nodename, fnode);
			}
			f3d.col--;
			if (leafnode == 1) {
				leafnode = 0;
			} else {
				/*
				 * Create bias tree in memory from raw data
				 */
				tndarray =
					GENMALLOC((cntndarray + 1) *
						   sizeof(struct rand_smc_node));
				unsigned int treenodetrackmal = 0;
				for (unsigned int j = 0U; (int)j < cntndarray; j++) {
					tndarray[j].biases = GENMALLOC(ndarray[j].entries * sizeof(int));
					tndarray[j].snames = GENMALLOC(ndarray[j].entries * sizeof(char *));
					tndarray[j].norcall = GENMALLOC(ndarray[j].entries * sizeof(int));
					tndarray[j].nname = GENMALLOC(ndarray[j].entries * sizeof(char *));
					tndarray[j].treenodes = GENMALLOC(ndarray[j].entries * sizeof(struct rand_smc_node));
					tndarray[j].entries = ndarray[j].entries;
					for (unsigned int i = 0U; (int)i < ndarray[j].entries; i++) {
						tndarray[j].snames[i] = GENMALLOC(1 * sizeof(char[MAX_NAME_CHARS]));
						strlcpy(tndarray[j].snames[i], ndarray[j].snames[i], MAX_NAME_CHARS);
						tndarray[j].nname[i] = GENMALLOC(1 * sizeof(char[MAX_NAME_CHARS]));
						strlcpy(tndarray[j].nname[i], ndarray[j].nname[i], MAX_NAME_CHARS);
						tndarray[j].biases[i] = ndarray[j].biases[i];
						tndarray[j].norcall[i] = ndarray[j].norcall[i];
						if (tndarray[j].norcall[i] == 1) {
							tndarray[j].treenodes[i] = tndarray[treenodetrackmal];
							treenodetrackmal++;
						}
					}
					tndarray[j].biasent = ndarray[j].biasent;
					tndarray[j].biasarray = GENMALLOC((tndarray[j].biasent) * sizeof(int));
					for (unsigned int i = 0U; (int)i < ndarray[j].biasent; i++) {
						tndarray[j].biasarray[i] = ndarray[j].biasarray[i];
					}
				}
				tndarray[cntndarray].biases = GENMALLOC(f3d.row[f3d.col + 1] * sizeof(int));
				tndarray[cntndarray].snames = GENMALLOC(f3d.row[f3d.col + 1] * sizeof(char *));
				tndarray[cntndarray].norcall = GENMALLOC(f3d.row[f3d.col + 1] * sizeof(int));
				tndarray[cntndarray].nname = GENMALLOC(f3d.row[f3d.col + 1] * sizeof(char *));
				tndarray[cntndarray].treenodes = GENMALLOC(f3d.row[f3d.col + 1] * sizeof(struct rand_smc_node));
				tndarray[cntndarray].entries = f3d.row[f3d.col + 1];

				/*
				 * Populate bias tree with former values in tree
				 */
				int cntbias = 0;
				int bias_count = 0;
				for (unsigned int j = 0U; (int)j < f3d.row[f3d.col + 1]; j++) {
					tndarray[cntndarray].snames[j] = GENMALLOC(1 * sizeof(char[MAX_NAME_CHARS]));
					strlcpy(tndarray[cntndarray].snames[j], f3d.fnamefifo[f3d.col + 1][j], MAX_NAME_CHARS);
					tndarray[cntndarray].nname[j] = GENMALLOC(1 * sizeof(char[MAX_NAME_CHARS]));
					strlcpy(tndarray[cntndarray].nname[j], f3d.nnfifo[f3d.col + 1][j], MAX_NAME_CHARS);
					tndarray[cntndarray].biases[j] = f3d.biasfifo[f3d.col + 1][j];
					cntbias += tndarray[cntndarray].biases[j];
					if (strcmp(tndarray[cntndarray].snames[j], "none") != 0) {
						strlcpy(tndarray[cntndarray].snames[j], f3d.fnamefifo[f3d.col + 1][j], MAX_NAME_CHARS);
						tndarray[cntndarray].norcall[j] = 0;
						tndarray[cntndarray].treenodes[j] = nrnode;
					} else {
						tndarray[cntndarray].norcall[j] = 1;
						tndarray[cntndarray].treenodes[j] = tndarray[treenodetrack];
						treenodetrack++;
					}
				}

				tndarray[cntndarray].biasent = cntbias;
				tndarray[cntndarray].biasarray = GENMALLOC((tndarray[cntndarray].biasent) * sizeof(int));
				for (unsigned int j = 0U; j < tndarray[cntndarray].entries; j++) {
					for (unsigned int i = 0U; i < tndarray[cntndarray].biases[j]; i++) {
						tndarray[cntndarray].biasarray[bias_count] = j;
						bias_count++;
					}
				}

				/*
				 * Free memory of old bias tree
				 */
				if (cntndarray > 0) {
					for (unsigned int j = 0U; (int)j < cntndarray; j++) {
						for (unsigned int i = 0U;
						     (int)i < ndarray[j].entries;
						     i++) {
							GENFREE(ndarray[j].snames[i]);
							GENFREE(ndarray[j].nname[i]);
						}
						GENFREE(ndarray[j].biases);
						GENFREE(ndarray[j].norcall);
						GENFREE(ndarray[j].biasarray);
						GENFREE(ndarray[j].snames);
						GENFREE(ndarray[j].nname);
						GENFREE(ndarray[j].treenodes);
					}
					GENFREE(ndarray);
				}

				/*
				 * Move pointers to new bias tree to current tree
				 */
				ndarray = tndarray;
				cntndarray++;

				/*
				 * Free raw data
				 */
				for (unsigned int j = 0U; (int)j < f3d.row[f3d.col + 1]; j++) {
					GENFREE(f3d.nnfifo[f3d.col + 1][j]);
					GENFREE(f3d.fnamefifo[f3d.col + 1][j]);
				}
				GENFREE(f3d.nnfifo[f3d.col + 1]);
				GENFREE(f3d.fnamefifo[f3d.col + 1]);
				GENFREE(f3d.biasfifo[f3d.col + 1]);
				f3d.curr_col -= 1;
			}
		}

		/*
		 * Ending device tree file and freeing raw data
		 */
		if (fdt32_to_cpu(rval) == 9) {
			for (unsigned int i = 0U; (int)i < f3d.col; i++) {
				for (unsigned int j = 0U; (int)j < f3d.row[i]; j++) {
					GENFREE(f3d.nnfifo[i][j]);
					GENFREE(f3d.fnamefifo[i][j]);
				}
				GENFREE(f3d.nnfifo[i]);
				GENFREE(f3d.fnamefifo[i]);
				GENFREE(f3d.biasfifo[i]);
			}
			GENFREE(f3d.nnfifo);
			GENFREE(f3d.fnamefifo);
			GENFREE(f3d.biasfifo);
			GENFREE(f3d.row);
			dtdone = 1;
		}
	}


	*casz = cntndarray;
	return ndarray;
}

/*
 * Running SMC call from what function name is selected
 */
void runtestfunction(char *funcstr)
{
	if (strcmp(funcstr, "sdei_version") == 0) {
		long long ret = sdei_version();
		if (ret != MAKE_SDEI_VERSION(1, 0, 0)) {
			tftf_testcase_printf("Unexpected SDEI version: 0x%llx\n",
					     ret);
		}
		printf("running %s\n", funcstr);
	}
	if (strcmp(funcstr, "sdei_pe_unmask") == 0) {
		long long ret = sdei_pe_unmask();
		if (ret < 0) {
			tftf_testcase_printf("SDEI pe unmask failed: 0x%llx\n",
					     ret);
		}
		printf("running %s\n", funcstr);
	}
	if (strcmp(funcstr, "sdei_pe_mask") == 0) {
		int64_t ret = sdei_pe_mask();
		if (ret < 0) {
			tftf_testcase_printf("SDEI pe mask failed: 0x%llx\n", ret);
		}
		printf("running %s\n", funcstr);
	}
	if (strcmp(funcstr, "sdei_event_status") == 0) {
		int64_t ret = sdei_event_status(0);
		if (ret < 0) {
			tftf_testcase_printf("SDEI event status failed: 0x%llx\n",
					     ret);
		}
		printf("running %s\n", funcstr);
	}
	if (strcmp(funcstr, "sdei_event_signal") == 0) {
		int64_t ret = sdei_event_signal(0);
		if (ret < 0) {
			tftf_testcase_printf("SDEI event signal failed: 0x%llx\n",
					     ret);
		}
		printf("running %s\n", funcstr);
	}
	if (strcmp(funcstr, "sdei_private_reset") == 0) {
		int64_t ret = sdei_private_reset();
		if (ret < 0) {
			tftf_testcase_printf("SDEI private reset failed: 0x%llx\n",
					     ret);
		}
		printf("running %s\n", funcstr);
	}
	if (strcmp(funcstr, "sdei_shared_reset") == 0) {
		int64_t ret = sdei_shared_reset();
		if (ret < 0) {
			tftf_testcase_printf("SDEI shared reset failed: 0x%llx\n",
					     ret);
		}
		printf("running %s\n", funcstr);
	}
}

/*
 * Function executes a single SMC fuzz test instance with a supplied seed.
 */
test_result_t smc_fuzzing_instance(uint32_t seed)
{
	/*
	 * Setting up malloc block parameters
	 */
	tmod.memptr = (void *)tmod.memory;
	tmod.memptrend = (void *)tmod.memory;
	tmod.maxmemblk = ((TOTALMEMORYSIZE / BLKSPACEDIV) / sizeof(struct memblk));
	tmod.nmemblk = 1;
	tmod.memptr->address = 0U;
	tmod.memptr->size = TOTALMEMORYSIZE - (TOTALMEMORYSIZE / BLKSPACEDIV);
	tmod.memptr->valid = 1;
	tmod.mallocdeladd[0] = 0U;
	tmod.precblock[0] = (void *)tmod.memory;
	tmod.trailblock[0] = NULL;
	tmod.cntdeladd = 0U;
	tmod.ptrmemblkqueue = 0U;
	tmod.mallocdeladd_queue_cnt = 0U;
	tmod.checkadd = 1U;
	tmod.checknumentries = 0U;
	tmod.memerror = 0U;
	struct memmod *mmod;
	mmod = &tmod;
	int cntndarray;
	struct rand_smc_node *tlnode;

	/*
	 * Creating SMC bias tree
	 */
	struct rand_smc_node *ndarray = createsmctree(&cntndarray, &tmod);

	if (tmod.memerror != 0) {
		return TEST_RESULT_FAIL;
	}

	/*
	 * Initialize pseudo random number generator with supplied seed.
	 */
	srand(seed);

	/*
	 * Code to traverse the bias tree and select function based on the biaes within
	 *
	 * The algorithm starts with the first node to pull up the biasarray.  The
	 * array is specified as a series of values that reflect the bias of the nodes
	 * in question. So for instance if there are three nodes with a bias of 2,5,7
	 * the biasarray would have the following constituency:
	 *
	 * 0,0,1,1,1,1,1,2,2,2,2,2,2,2.
	 *
	 * Mapping 0 as node 1, 1 as node 2, and 2 as node 3.
	 * The biasent variable contains the count of the size of the biasarray which
	 * provides the input for random selection.  This is subsequently applied as an
	 * index to the biasarray.  The selection pulls up the node and then is checked
	 * for whether it is a leaf or tree node using the norcall variable.
	 * If it is a leaf then the bias tree traversal ends with an SMC call.
	 * If it is a tree node then the process begins again with
	 * another loop to continue the process of selection until an eventual leaf
	 * node is found.
	 */
	for (unsigned int i = 0U; i < SMC_FUZZ_CALLS_PER_INSTANCE; i++) {
		tlnode = &ndarray[cntndarray - 1];
		int nd = 0;
		while (nd == 0) {
			int nch = rand()%tlnode->biasent;
			int selent = tlnode->biasarray[nch];
			if (tlnode->norcall[selent] == 0) {
				runtestfunction(tlnode->snames[selent]);
				nd = 1;
			} else {
				tlnode = &tlnode->treenodes[selent];
			}
		}
	}

	/*
	 * End of test SMC selection and freeing of nodes
	 */
	if (cntndarray > 0) {
		for (unsigned int j = 0U; j < cntndarray; j++) {
			for (unsigned int i = 0U; i < ndarray[j].entries; i++) {
				GENFREE(ndarray[j].snames[i]);
				GENFREE(ndarray[j].nname[i]);
			}
			GENFREE(ndarray[j].biases);
			GENFREE(ndarray[j].norcall);
			GENFREE(ndarray[j].biasarray);
			GENFREE(ndarray[j].snames);
			GENFREE(ndarray[j].nname);
			GENFREE(ndarray[j].treenodes);
		}
		GENFREE(ndarray);
	}

	return TEST_RESULT_SUCCESS;
}

/*
 * Top of SMC fuzzing module
 */
test_result_t smc_fuzzing_top(void)
{
	/* These SMC_FUZZ_x macros are supplied by the build system. */
	test_result_t results[SMC_FUZZ_INSTANCE_COUNT];
	uint32_t seeds[SMC_FUZZ_INSTANCE_COUNT] = {SMC_FUZZ_SEEDS};
	test_result_t result = TEST_RESULT_SUCCESS;
	unsigned int i;

	/* Run each instance. */
	for (i = 0U; i < SMC_FUZZ_INSTANCE_COUNT; i++) {
		printf("Starting SMC fuzz test with seed 0x%x\n", seeds[i]);
		results[i] = smc_fuzzing_instance(seeds[i]);
	}

	/* Report successes and failures. */
	printf("SMC Fuzz Test Results Summary\n");
	for (i = 0U; i < SMC_FUZZ_INSTANCE_COUNT; i++) {
		/* Display instance number. */
		printf("  Instance #%d\n", i);

		/* Print test results. */
		printf("    Result: ");
		if (results[i] == TEST_RESULT_SUCCESS) {
			printf("SUCCESS\n");
		} else if (results[i] == TEST_RESULT_FAIL) {
			printf("FAIL\n");
			/* If we got a failure, update the result value. */
			result = TEST_RESULT_FAIL;
		} else if (results[i] == TEST_RESULT_SKIPPED) {
			printf("SKIPPED\n");
		}

		/* Print seed used */
		printf("    Seed: 0x%x\n", seeds[i]);
	}

	/*
	 * Print out the smc fuzzer parameters so this test can be replicated.
	 */
	printf("SMC fuzz build parameters to recreate this test:\n");
	printf("  SMC_FUZZ_INSTANCE_COUNT=%u\n",
		SMC_FUZZ_INSTANCE_COUNT);
	printf("  SMC_FUZZ_CALLS_PER_INSTANCE=%u\n",
		SMC_FUZZ_CALLS_PER_INSTANCE);
	printf("  SMC_FUZZ_SEEDS=0x%x", seeds[0]);
	for (i = 1U; i < SMC_FUZZ_INSTANCE_COUNT; i++) {
		printf(",0x%x", seeds[i]);
	}
	printf("\n");

	return result;
}
