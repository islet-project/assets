/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>
#include <tftf_lib.h>
#include <xlat_tables_v2.h>

#include "debugfs.h"

#define SMC_OK			(0)

#define DEBUGFS_VERSION		(0x00000001)
#define DEBUGFS_SMC_64		(0xC2000030)
#define MAX_PATH_LEN		(256)

/* DebugFS shared buffer area */
#ifndef PLAT_ARM_DEBUGFS_BASE
#define PLAT_ARM_DEBUGFS_BASE		(0x81000000)
#define PLAT_ARM_DEBUGFS_SIZE		(0x1000)
#endif /* PLAT_ARM_DEBUGFS_BASE */

union debugfs_parms {
	struct {
		char fname[MAX_PATH_LEN];
	} open;

	struct mount {
		char srv[MAX_PATH_LEN];
		char where[MAX_PATH_LEN];
		char spec[MAX_PATH_LEN];
	} mount;

	struct {
		char path[MAX_PATH_LEN];
		dir_t dir;
	} stat;

	struct {
		char oldpath[MAX_PATH_LEN];
		char newpath[MAX_PATH_LEN];
	} bind;
};

typedef struct {
	char *name;
	qid_t qid;
} dir_expected_t;

static const dir_expected_t root_dir_expected[] = {
	{ "dev",   0x8001 },
	{ "blobs", 0x8003 },
	{ "fip",   0x8002 }
};

static unsigned int read_buffer[4096 / sizeof(unsigned int)];

static void *const payload = (void *) PLAT_ARM_DEBUGFS_BASE;

static int init(unsigned long long phys_addr)
{
	smc_ret_values ret;
	smc_args args;

	args.fid  = DEBUGFS_SMC_64;
	args.arg1 = INIT;
	args.arg2 = phys_addr;
	ret = tftf_smc(&args);

	return (ret.ret0 == SMC_OK) ? 0 : -1;
}

static int version(void)
{
	smc_ret_values ret;
	smc_args args;

	args.fid  = DEBUGFS_SMC_64;
	args.arg1 = VERSION;
	ret = tftf_smc(&args);

	return (ret.ret0 == SMC_OK) ? ret.ret1 : -1;
}

static int open(const char *name, int flags)
{
	union debugfs_parms *parms = payload;
	smc_ret_values ret;
	smc_args args;

	strlcpy(parms->open.fname, name, MAX_PATH_LEN);

	args.fid  = DEBUGFS_SMC_64;
	args.arg1 = OPEN;
	args.arg2 = (u_register_t) flags;
	ret = tftf_smc(&args);

	return (ret.ret0 == SMC_OK) ? ret.ret1 : -1;
}

static int read(int fd, void *buf, size_t size)
{
	smc_ret_values ret;
	smc_args args;

	args.fid  = DEBUGFS_SMC_64;
	args.arg1 = READ;
	args.arg2 = (u_register_t) fd;
	args.arg3 = (u_register_t) size;

	ret = tftf_smc(&args);

	if (ret.ret0 == SMC_OK) {
		memcpy(buf, payload, size);
		return ret.ret1;
	}

	return -1;
}

static int close(int fd)
{
	smc_ret_values ret;
	smc_args args;

	args.fid  = DEBUGFS_SMC_64;
	args.arg1 = CLOSE;
	args.arg2 = (u_register_t) fd;

	ret = tftf_smc(&args);

	return (ret.ret0 == SMC_OK) ? 0 : -1;
}

static int mount(char *srv, char *where, char *spec)
{
	union debugfs_parms *parms = payload;
	smc_ret_values ret;
	smc_args args;

	strlcpy(parms->mount.srv, srv, MAX_PATH_LEN);
	strlcpy(parms->mount.where, where, MAX_PATH_LEN);
	strlcpy(parms->mount.spec, spec, MAX_PATH_LEN);

	args.fid  = DEBUGFS_SMC_64;
	args.arg1 = MOUNT;

	ret = tftf_smc(&args);

	return (ret.ret0 == SMC_OK) ? 0 : -1;
}

static int stat(const char *name, dir_t *dir)
{
	union debugfs_parms *parms = payload;
	smc_ret_values ret;
	smc_args args;

	strlcpy(parms->stat.path, name, MAX_PATH_LEN);

	args.fid  = DEBUGFS_SMC_64;
	args.arg1 = STAT;

	ret = tftf_smc(&args);

	if (ret.ret0 == SMC_OK) {
		memcpy(dir, &parms->stat.dir, sizeof(dir_t));
		return 0;
	}

	return -1;
}

static int seek(int fd, long offset, int whence)
{
	smc_ret_values ret;
	smc_args args;

	args.fid  = DEBUGFS_SMC_64;
	args.arg1 = SEEK;
	args.arg2 = (u_register_t) fd;
	args.arg3 = (u_register_t) offset;
	args.arg4 = (u_register_t) whence;

	ret = tftf_smc(&args);

	return (ret.ret0 == SMC_OK) ? 0 : -1;
}

static bool compare_dir(const dir_expected_t *dir_expected,
			unsigned int iteration, dir_t *dir)
{
	return ((memcmp(dir->name, dir_expected[iteration].name,
		       strlen(dir_expected[iteration].name)) == 0) &&
		(dir->qid == dir_expected[iteration].qid));
}

static void dir_print(dir_t *dir)
{
	tftf_testcase_printf("name: %s, length: %ld, mode: %d, type: %d, "
			     "dev: %d, qid: 0x%x\n",
			     dir->name,
			     dir->length,
			     dir->mode,
			     dir->type,
			     dir->dev,
			     dir->qid);
}

/*
 * @Test_Aim@ Issue SMCs to TF-A calling debugfs functions in order to test
 * the exposure of the filesystem.
 * The result is displayed on the console, something that should look like:
 * > ls /
 * dev
 * fip
 * blobs
 */
test_result_t test_debugfs(void)
{
	int fd, ret, iteration;
	dir_t dir;

	/* Get debugfs interface version (if implemented)*/
	ret = version();
	if (ret != DEBUGFS_VERSION) {
		/* Likely debugfs feature is not implemented */
		return TEST_RESULT_SKIPPED;
	}

	/* Initialize debugfs feature, this maps the NS shared buffer in SWd */
	ret = init(PLAT_ARM_DEBUGFS_BASE);
	if (ret != 0) {
		return TEST_RESULT_FAIL;
	}

	/* Calling init a second time should fail */
	ret = init(PLAT_ARM_DEBUGFS_BASE);
	if (ret == 0) {
		tftf_testcase_printf("init succeeded ret=%d\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* open non-existing directory */
	fd = open("/dummy", O_READ);
	if (fd >= 0) {
		tftf_testcase_printf("open succeeded fd=%d\n", fd);
		return TEST_RESULT_FAIL;
	}

	/* stat non-existent file from root */
	ret = stat("/unknown", &dir);
	if (ret == 0) {
		tftf_testcase_printf("stat succeeded ret=%d\n", ret);
		return TEST_RESULT_FAIL;
	}

	/***************** Root directory listing **************/
	/* open root directory */
	fd = open("/", O_READ);
	if (fd < 0) {
		tftf_testcase_printf("open failed fd=%d\n", fd);
		return TEST_RESULT_FAIL;
	}

	/* read directory entries */
	iteration = 0;
	ret = read(fd, &dir, sizeof(dir));
	while (ret > 0) {
		if (compare_dir(root_dir_expected, iteration++,
				&dir) == false) {
			dir_print(&dir);
			return TEST_RESULT_FAIL;
		}

		ret = read(fd, &dir, sizeof(dir));
	}

	/* close root directory handle */
	ret = close(fd);
	if (ret < 0) {
		tftf_testcase_printf("close failed ret=%d\n", ret);
		return TEST_RESULT_FAIL;
	}

	/***************** FIP operations **************/
	/* mount fip */
	ret = mount("#F", "/fip", "/blobs/fip.bin");
	if (ret < 0) {
		tftf_testcase_printf("mount failed ret=%d\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* stat a non-existent file from fip */
	ret = stat("/fip/unknown", &dir);
	if (ret == 0) {
		tftf_testcase_printf("stat succeeded ret=%d\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* detect bl2 image presence */
	ret = stat("/fip/bl2.bin", &dir);
	if (ret != 0) {
		tftf_testcase_printf("stat failed ret=%d\n", ret);
		return TEST_RESULT_FAIL;
	}

	/* open bl2 */
	fd = open("/fip/bl2.bin", O_READ);
	if (fd < 0) {
		tftf_testcase_printf("open failed fd=%d\n", fd);
		return TEST_RESULT_FAIL;
	}

	/* read first 128 bytes */
	ret = read(fd, read_buffer, 128);
	if (ret != 128) {
		tftf_testcase_printf("read failed(%d) ret=%d\n", __LINE__, ret);
		return TEST_RESULT_FAIL;
	}

	/* Compare first word of bl2 binary */
	if (read_buffer[0] != 0xaa0003f4) {
		tftf_testcase_printf("read ret %d, buf[0]: 0x%x\n",
				     ret, read_buffer[0]);
		return TEST_RESULT_FAIL;
	}

	/* rewind to file start */
	ret = seek(fd, 0, KSEEK_SET);
	if (ret != 0) {
		tftf_testcase_printf("seek failed ret=%d\n", ret);
		return TEST_RESULT_FAIL;
	}

	size_t read_size = 0;
	do {
		ret = read(fd, read_buffer, sizeof(read_buffer));
		if (ret < 0) {
			tftf_testcase_printf("read failed(%d) ret=%d\n",
					     __LINE__, ret);
			return TEST_RESULT_FAIL;
		}
		read_size += ret;
	} while (ret);

	if (read_size != dir.length) {
		tftf_testcase_printf("read size mismatch read_size=%zu "
				"dir.length=%ld\n", read_size, dir.length);
		return TEST_RESULT_FAIL;
	}

	return TEST_RESULT_SUCCESS;
}
