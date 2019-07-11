/*
 * Copyright (c) 2019, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef DEBUGFS_H
#define DEBUGFS_H

#define NAMELEN   13 /* Maximum length of a file name */
#define PATHLEN   41 /* Maximum length of a path */
#define STATLEN   41 /* Size of static part of dir format */
#define ROOTLEN   (2 + 4) /* Size needed to encode root string */
#define FILNAMLEN (2 + NAMELEN) /* Size needed to encode filename */
#define DIRLEN    (STATLEN + FILNAMLEN + 3*ROOTLEN) /* Size of dir entry */

#define KSEEK_SET 0
#define KSEEK_CUR 1
#define KSEEK_END 2

#define NELEM(tab) (sizeof(tab) / sizeof((tab)[0]))

typedef unsigned short qid_t;

/*******************************************************************************
 * This structure contains the necessary information to represent a 9p
 * directory.
 ******************************************************************************/
typedef struct {
	char name[NAMELEN];
	long length;
	unsigned char mode;
	unsigned char type;
	unsigned char dev;
	qid_t qid;
} dir_t;

enum devflags {
	O_READ   = 1 << 0,
	O_WRITE  = 1 << 1,
	O_RDWR   = 1 << 2,
	O_BIND   = 1 << 3,
	O_DIR    = 1 << 4,
	O_STAT   = 1 << 5
};

#define MOUNT		0
#define CREATE		1
#define OPEN		2
#define CLOSE		3
#define READ		4
#define WRITE		5
#define SEEK		6
#define BIND		7
#define STAT		8
#define INIT		10
#define VERSION		11

#endif /* DEBUGFS_H */
