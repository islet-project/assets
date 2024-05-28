#include "kvm/virtio-pci-dev.h"
#include "kvm/ioport.h"
#include "kvm/util.h"
#include "kvm/threadpool.h"
#include "kvm/irq.h"
#include "kvm/virtio-9p.h"
#include "kvm/guest_compat.h"
#include "kvm/builtin-setup.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/vfs.h>

#include <linux/virtio_ring.h>
#include <linux/virtio_9p.h>
#include <linux/9p.h>

extern int cloak_single_test;
extern bool is_no_shared_region(struct kvm *kvm);

extern int send_msg(const void *msg, size_t size, bool app_to_gw);
extern int receive_msg(void *msg, size_t size, bool app_from_gw);
extern bool queue_exist(bool app_to_gw);

static LIST_HEAD(devs);
static int compat_id = -1;

static int insert_new_fid(struct p9_dev *dev, struct p9_fid *fid);
static struct p9_fid *find_or_create_fid(struct p9_dev *dev, u32 fid)
{
	struct rb_node *node = dev->fids.rb_node;
	struct p9_fid *pfid = NULL;
	size_t len;

	while (node) {
		struct p9_fid *cur = rb_entry(node, struct p9_fid, node);

		if (fid < cur->fid) {
			node = node->rb_left;
		} else if (fid > cur->fid) {
			node = node->rb_right;
		} else {
			return cur;
		}
	}

	pfid = calloc(sizeof(*pfid), 1);
	if (!pfid)
		return NULL;

	len = strlen(dev->root_dir);
	if (len >= sizeof(pfid->abs_path)) {
		free(pfid);
		return NULL;
	}

	pfid->fid = fid;
	strcpy(pfid->abs_path, dev->root_dir);
	pfid->path = pfid->abs_path + strlen(pfid->abs_path);

	insert_new_fid(dev, pfid);

	return pfid;
}

static int insert_new_fid(struct p9_dev *dev, struct p9_fid *fid)
{
	struct rb_node **node = &(dev->fids.rb_node), *parent = NULL;

	while (*node) {
		int result = fid->fid - rb_entry(*node, struct p9_fid, node)->fid;

		parent = *node;
		if (result < 0)
			node    = &((*node)->rb_left);
		else if (result > 0)
			node    = &((*node)->rb_right);
		else
			return -EEXIST;
	}

	rb_link_node(&fid->node, parent, node);
	rb_insert_color(&fid->node, &dev->fids);
	return 0;
}

static struct p9_fid *get_fid(struct p9_dev *p9dev, int fid)
{
	struct p9_fid *new;

	new = find_or_create_fid(p9dev, fid);

	return new;
}

static void stat2qid(struct stat *st, struct p9_qid *qid)
{
	*qid = (struct p9_qid) {
		.path		= st->st_ino,
		.version	= st->st_mtime,
	};

	if (S_ISDIR(st->st_mode))
		qid->type	|= P9_QTDIR;
}

static void close_fid(struct p9_dev *p9dev, u32 fid)
{
	struct p9_fid *pfid = get_fid(p9dev, fid);

	if (pfid->fd > 0)
		close(pfid->fd);

	if (pfid->dir)
		closedir(pfid->dir);

	rb_erase(&pfid->node, &p9dev->fids);
	free(pfid);
}

static void virtio_p9_set_reply_header(struct p9_pdu *pdu, u32 size)
{
	u8 cmd;
	u16 tag;

	pdu->read_offset = sizeof(u32);
	virtio_p9_pdu_readf(pdu, "bw", &cmd, &tag);
	pdu->write_offset = 0;
	/* cmd + 1 is the reply message */
	virtio_p9_pdu_writef(pdu, "dbw", size, cmd + 1, tag);
}

static u16 virtio_p9_update_iov_cnt(struct iovec iov[], u32 count, int iov_cnt)
{
	int i;
	u32 total = 0;
	for (i = 0; (i < iov_cnt) && (total < count); i++) {
		if (total + iov[i].iov_len > count) {
			/* we don't need this iov fully */
			iov[i].iov_len -= ((total + iov[i].iov_len) - count);
			i++;
			break;
		}
		total += iov[i].iov_len;
	}
	return i;
}

static void virtio_p9_error_reply(struct p9_dev *p9dev,
				  struct p9_pdu *pdu, int err, u32 *outlen)
{
	u16 tag;

	/* EMFILE at server implies ENFILE for the VM */
	if (err == EMFILE)
		err = ENFILE;

	pdu->write_offset = VIRTIO_9P_HDR_LEN;
	virtio_p9_pdu_writef(pdu, "d", err);
	*outlen = pdu->write_offset;

	/* read the tag from input */
	pdu->read_offset = sizeof(u32) + sizeof(u8);
	virtio_p9_pdu_readf(pdu, "w", &tag);

	/* Update the header */
	pdu->write_offset = 0;
	virtio_p9_pdu_writef(pdu, "dbw", *outlen, P9_RLERROR, tag);
}

static void virtio_p9_version(struct p9_dev *p9dev,
			      struct p9_pdu *pdu, u32 *outlen)
{
	u32 msize;
	char *version;
	virtio_p9_pdu_readf(pdu, "ds", &msize, &version);
	/*
	 * reply with the same msize the client sent us
	 * Error out if the request is not for 9P2000.L
	 */
	if (!strcmp(version, VIRTIO_9P_VERSION_DOTL))
		virtio_p9_pdu_writef(pdu, "ds", msize, version);
	else
		virtio_p9_pdu_writef(pdu, "ds", msize, "unknown");

	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	free(version);

	//printf("[JB] p9_version outlen: %d\n", *outlen);
	return;
}

static void virtio_p9_clunk(struct p9_dev *p9dev,
			    struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid;

	virtio_p9_pdu_readf(pdu, "d", &fid);
	close_fid(p9dev, fid);

	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
}

/*
 * FIXME!! Need to map to protocol independent value. Upstream
 * 9p also have the same BUG
 */
static int virtio_p9_openflags(int flags)
{
	flags &= ~(O_NOCTTY | O_ASYNC | O_CREAT | O_DIRECT);
	flags |= O_NOFOLLOW;
	return flags;
}

static bool is_dir(struct p9_fid *fid)
{
	struct stat st;

	stat(fid->abs_path, &st);

	return S_ISDIR(st.st_mode);
}

/* path is always absolute */
static bool path_is_illegal(const char *path)
{
	size_t len;

	if (strstr(path, "/../") != NULL)
		return true;

	len = strlen(path);
	if (len >= 3 && strcmp(path + len - 3, "/..") == 0)
		return true;

	return false;
}

static int get_full_path_helper(char *full_path, size_t size,
			 const char *dirname, const char *name)
{
	int ret;

	ret = snprintf(full_path, size, "%s/%s", dirname, name);
	if (ret >= (int)size) {
		errno = ENAMETOOLONG;
		return -1;
	}

	if (path_is_illegal(full_path)) {
		errno = EACCES;
		return -1;
	}

	return 0;
}

static int get_full_path(char *full_path, size_t size, struct p9_fid *fid,
			 const char *name)
{
	return get_full_path_helper(full_path, size, fid->abs_path, name);
}

static int stat_rel(struct p9_dev *p9dev, const char *path, struct stat *st)
{
	char full_path[PATH_MAX];

	if (get_full_path_helper(full_path, sizeof(full_path), p9dev->root_dir, path) != 0)
		return -1;

	if (lstat(full_path, st) != 0)
		return -1;

	return 0;
}

static void virtio_p9_open(struct p9_dev *p9dev,
			   struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid, flags;
	struct stat st;
	struct p9_qid qid;
	struct p9_fid *new_fid;


	virtio_p9_pdu_readf(pdu, "dd", &fid, &flags);
	new_fid = get_fid(p9dev, fid);

	if (lstat(new_fid->abs_path, &st) < 0)
		goto err_out;

	stat2qid(&st, &qid);

	if (is_dir(new_fid)) {
		new_fid->dir = opendir(new_fid->abs_path);
		if (!new_fid->dir)
			goto err_out;
	} else {
		new_fid->fd  = open(new_fid->abs_path,
				    virtio_p9_openflags(flags));
		if (new_fid->fd < 0)
			goto err_out;
	}
	/* FIXME!! need ot send proper iounit  */
	virtio_p9_pdu_writef(pdu, "Qd", &qid, 0);

	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_create(struct p9_dev *p9dev,
			     struct p9_pdu *pdu, u32 *outlen)
{
	int fd, ret;
	char *name;
	size_t size;
	struct stat st;
	struct p9_qid qid;
	struct p9_fid *dfid;
	char full_path[PATH_MAX];
	char *tmp_path;
	u32 dfid_val, flags, mode, gid;

	virtio_p9_pdu_readf(pdu, "dsddd", &dfid_val,
			    &name, &flags, &mode, &gid);
	dfid = get_fid(p9dev, dfid_val);

	if (get_full_path(full_path, sizeof(full_path), dfid, name) != 0)
		goto err_out;

	size = sizeof(dfid->abs_path) - (dfid->path - dfid->abs_path);

	tmp_path = strdup(dfid->path);
	if (!tmp_path)
		goto err_out;

	ret = snprintf(dfid->path, size, "%s/%s", tmp_path, name);
	free(tmp_path);
	if (ret >= (int)size) {
		errno = ENAMETOOLONG;
		if (size > 0)
			dfid->path[size] = '\x00';
		goto err_out;
	}

	flags = virtio_p9_openflags(flags);

	fd = open(full_path, flags | O_CREAT, mode);
	if (fd < 0)
		goto err_out;
	dfid->fd = fd;

	if (lstat(full_path, &st) < 0)
		goto err_out;

	ret = chmod(full_path, mode & 0777);
	if (ret < 0)
		goto err_out;

	stat2qid(&st, &qid);
	virtio_p9_pdu_writef(pdu, "Qd", &qid, 0);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	free(name);
	return;
err_out:
	free(name);
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_mkdir(struct p9_dev *p9dev,
			    struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *name;
	struct stat st;
	struct p9_qid qid;
	struct p9_fid *dfid;
	char full_path[PATH_MAX];
	u32 dfid_val, mode, gid;

	virtio_p9_pdu_readf(pdu, "dsdd", &dfid_val,
			    &name, &mode, &gid);
	dfid = get_fid(p9dev, dfid_val);

	if (get_full_path(full_path, sizeof(full_path), dfid, name) != 0)
		goto err_out;

	ret = mkdir(full_path, mode);
	if (ret < 0)
		goto err_out;

	if (lstat(full_path, &st) < 0)
		goto err_out;

	ret = chmod(full_path, mode & 0777);
	if (ret < 0)
		goto err_out;

	stat2qid(&st, &qid);
	virtio_p9_pdu_writef(pdu, "Qd", &qid, 0);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	free(name);
	return;
err_out:
	free(name);
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static int join_path(struct p9_fid *fid, const char *name)
{
	size_t len, size;

	size = sizeof(fid->abs_path) - (fid->path - fid->abs_path);
	len = strlen(name);
	if (len >= size)
		return -1;

	strncpy(fid->path, name, size);
	return 0;
}

static void virtio_p9_walk(struct p9_dev *p9dev,
			   struct p9_pdu *pdu, u32 *outlen)
{
	u8 i;
	u16 nwqid;
	u16 nwname;
	struct p9_qid wqid;
	struct p9_fid *new_fid, *old_fid;
	u32 fid_val, newfid_val;


	virtio_p9_pdu_readf(pdu, "ddw", &fid_val, &newfid_val, &nwname);
	new_fid	= get_fid(p9dev, newfid_val);

	nwqid = 0;
	if (nwname) {
		struct p9_fid *fid = get_fid(p9dev, fid_val);

		if (join_path(new_fid, fid->path) != 0) {
			errno = ENAMETOOLONG;
			goto err_out;
		}

		/* skip the space for count */
		pdu->write_offset += sizeof(u16);
		for (i = 0; i < nwname; i++) {
			struct stat st;
			char tmp[PATH_MAX] = {0};
			char *str;
			int ret;

			virtio_p9_pdu_readf(pdu, "s", &str);

			/* Format the new path we're 'walk'ing into */
			ret = snprintf(tmp, sizeof(tmp), "%s/%s", new_fid->path, str);
			if (ret >= (int)sizeof(tmp)) {
				errno = ENAMETOOLONG;
				goto err_out;
			}

			free(str);

			if (stat_rel(p9dev, tmp, &st) != 0)
				goto err_out;

			stat2qid(&st, &wqid);
			if (join_path(new_fid, tmp) != 0) {
				errno = ENAMETOOLONG;
				goto err_out;
			}
			new_fid->uid = fid->uid;
			nwqid++;
			virtio_p9_pdu_writef(pdu, "Q", &wqid);
		}
	} else {
		/*
		 * update write_offset so our outlen get correct value
		 */
		pdu->write_offset += sizeof(u16);
		old_fid = get_fid(p9dev, fid_val);
		if (join_path(new_fid, old_fid->path) != 0) {
			errno = ENAMETOOLONG;
			goto err_out;
		}
		new_fid->uid    = old_fid->uid;
	}
	*outlen = pdu->write_offset;
	pdu->write_offset = VIRTIO_9P_HDR_LEN;
	virtio_p9_pdu_writef(pdu, "d", nwqid);
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_attach(struct p9_dev *p9dev,
			     struct p9_pdu *pdu, u32 *outlen)
{
	char *uname;
	char *aname;
	struct stat st;
	struct p9_qid qid;
	struct p9_fid *fid;
	u32 fid_val, afid, uid;

	virtio_p9_pdu_readf(pdu, "ddssd", &fid_val, &afid,
			    &uname, &aname, &uid);

	free(uname);
	free(aname);

	if (lstat(p9dev->root_dir, &st) < 0)
		goto err_out;

	stat2qid(&st, &qid);

	fid = get_fid(p9dev, fid_val);
	fid->uid = uid;
	if (join_path(fid, "/") != 0) {
		errno = ENAMETOOLONG;
		goto err_out;
	}

	virtio_p9_pdu_writef(pdu, "Q", &qid);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);

	//printf("[JB] p9_attach: %d\n", *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_fill_stat(struct p9_dev *p9dev,
				struct stat *st, struct p9_stat_dotl *statl)
{
	memset(statl, 0, sizeof(*statl));
	statl->st_mode		= st->st_mode;
	statl->st_nlink		= st->st_nlink;
	statl->st_uid		= KUIDT_INIT(st->st_uid);
	statl->st_gid		= KGIDT_INIT(st->st_gid);
	statl->st_rdev		= st->st_rdev;
	statl->st_size		= st->st_size;
	statl->st_blksize	= st->st_blksize;
	statl->st_blocks	= st->st_blocks;
	statl->st_atime_sec	= st->st_atime;
	statl->st_atime_nsec	= st->st_atim.tv_nsec;
	statl->st_mtime_sec	= st->st_mtime;
	statl->st_mtime_nsec	= st->st_mtim.tv_nsec;
	statl->st_ctime_sec	= st->st_ctime;
	statl->st_ctime_nsec	= st->st_ctim.tv_nsec;
	/* Currently we only support BASIC fields in stat */
	statl->st_result_mask	= P9_STATS_BASIC;
	stat2qid(st, &statl->qid);
}

static void virtio_p9_read(struct p9_dev *p9dev,
			   struct p9_pdu *pdu, u32 *outlen)
{
	u64 offset;
	u32 fid_val;
	u16 iov_cnt;
	void *iov_base;
	size_t iov_len;
	u32 count, rcount;
	struct p9_fid *fid;


	rcount = 0;
	virtio_p9_pdu_readf(pdu, "dqd", &fid_val, &offset, &count);
	fid = get_fid(p9dev, fid_val);

	iov_base = pdu->in_iov[0].iov_base;
	iov_len  = pdu->in_iov[0].iov_len;
	iov_cnt  = pdu->in_iov_cnt;
	pdu->in_iov[0].iov_base += VIRTIO_9P_HDR_LEN + sizeof(u32);
	pdu->in_iov[0].iov_len -= VIRTIO_9P_HDR_LEN + sizeof(u32);
	pdu->in_iov_cnt = virtio_p9_update_iov_cnt(pdu->in_iov,
						   count,
						   pdu->in_iov_cnt);
	rcount = preadv(fid->fd, pdu->in_iov,
			pdu->in_iov_cnt, offset);
	if (rcount > count)
		rcount = count;
	/*
	 * Update the iov_base back, so that rest of
	 * pdu_writef works correctly.
	 */
	pdu->in_iov[0].iov_base = iov_base;
	pdu->in_iov[0].iov_len  = iov_len;
	pdu->in_iov_cnt         = iov_cnt;

	pdu->write_offset = VIRTIO_9P_HDR_LEN;
	virtio_p9_pdu_writef(pdu, "d", rcount);
	*outlen = pdu->write_offset + rcount;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
}

static int virtio_p9_dentry_size(struct dirent *dent)
{
	/*
	 * Size of each dirent:
	 * qid(13) + offset(8) + type(1) + name_len(2) + name
	 */
	return 24 + strlen(dent->d_name);
}

static void virtio_p9_readdir(struct p9_dev *p9dev,
			      struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid_val;
	u32 count, rcount;
	struct stat st;
	struct p9_fid *fid;
	struct dirent *dent;
	u64 offset, old_offset;

	rcount = 0;
	virtio_p9_pdu_readf(pdu, "dqd", &fid_val, &offset, &count);
	fid = get_fid(p9dev, fid_val);

	if (!is_dir(fid)) {
		errno = EINVAL;
		goto err_out;
	}

	/* Move the offset specified */
	seekdir(fid->dir, offset);

	old_offset = offset;
	/* If reading a dir, fill the buffer with p9_stat entries */
	dent = readdir(fid->dir);

	/* Skip the space for writing count */
	pdu->write_offset += sizeof(u32);
	while (dent) {
		u32 read;
		struct p9_qid qid;

		if ((rcount + virtio_p9_dentry_size(dent)) > count) {
			/* seek to the previous offset and return */
			seekdir(fid->dir, old_offset);
			break;
		}
		old_offset = dent->d_off;
		if (stat_rel(p9dev, dent->d_name, &st) != 0)
			memset(&st, -1, sizeof(st));
		stat2qid(&st, &qid);
		read = pdu->write_offset;
		virtio_p9_pdu_writef(pdu, "Qqbs", &qid, dent->d_off,
				     dent->d_type, dent->d_name);
		rcount += pdu->write_offset - read;
		dent = readdir(fid->dir);
	}

	pdu->write_offset = VIRTIO_9P_HDR_LEN;
	virtio_p9_pdu_writef(pdu, "d", rcount);
	*outlen = pdu->write_offset + rcount;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}


static void virtio_p9_getattr(struct p9_dev *p9dev,
			      struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid_val;
	struct stat st;
	u64 request_mask;
	struct p9_fid *fid;
	struct p9_stat_dotl statl;

	virtio_p9_pdu_readf(pdu, "dq", &fid_val, &request_mask);
	fid = get_fid(p9dev, fid_val);
	if (lstat(fid->abs_path, &st) < 0)
		goto err_out;

	virtio_p9_fill_stat(p9dev, &st, &statl);
	virtio_p9_pdu_writef(pdu, "A", &statl);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

/* FIXME!! from linux/fs.h */
/*
 * Attribute flags.  These should be or-ed together to figure out what
 * has been changed!
 */
#define ATTR_MODE	(1 << 0)
#define ATTR_UID	(1 << 1)
#define ATTR_GID	(1 << 2)
#define ATTR_SIZE	(1 << 3)
#define ATTR_ATIME	(1 << 4)
#define ATTR_MTIME	(1 << 5)
#define ATTR_CTIME	(1 << 6)
#define ATTR_ATIME_SET	(1 << 7)
#define ATTR_MTIME_SET	(1 << 8)
#define ATTR_FORCE	(1 << 9) /* Not a change, but a change it */
#define ATTR_ATTR_FLAG	(1 << 10)
#define ATTR_KILL_SUID	(1 << 11)
#define ATTR_KILL_SGID	(1 << 12)
#define ATTR_FILE	(1 << 13)
#define ATTR_KILL_PRIV	(1 << 14)
#define ATTR_OPEN	(1 << 15) /* Truncating from open(O_TRUNC) */
#define ATTR_TIMES_SET	(1 << 16)

#define ATTR_MASK    127

static void virtio_p9_setattr(struct p9_dev *p9dev,
			      struct p9_pdu *pdu, u32 *outlen)
{
	int ret = 0;
	u32 fid_val;
	struct p9_fid *fid;
	struct p9_iattr_dotl p9attr;

	virtio_p9_pdu_readf(pdu, "dI", &fid_val, &p9attr);
	fid = get_fid(p9dev, fid_val);

	if (p9attr.valid & ATTR_MODE) {
		ret = chmod(fid->abs_path, p9attr.mode);
		if (ret < 0)
			goto err_out;
	}
	if (p9attr.valid & (ATTR_ATIME | ATTR_MTIME)) {
		struct timespec times[2];
		if (p9attr.valid & ATTR_ATIME) {
			if (p9attr.valid & ATTR_ATIME_SET) {
				times[0].tv_sec = p9attr.atime_sec;
				times[0].tv_nsec = p9attr.atime_nsec;
			} else {
				times[0].tv_nsec = UTIME_NOW;
			}
		} else {
			times[0].tv_nsec = UTIME_OMIT;
		}
		if (p9attr.valid & ATTR_MTIME) {
			if (p9attr.valid & ATTR_MTIME_SET) {
				times[1].tv_sec = p9attr.mtime_sec;
				times[1].tv_nsec = p9attr.mtime_nsec;
			} else {
				times[1].tv_nsec = UTIME_NOW;
			}
		} else
			times[1].tv_nsec = UTIME_OMIT;

		ret = utimensat(-1, fid->abs_path, times, AT_SYMLINK_NOFOLLOW);
		if (ret < 0)
			goto err_out;
	}
	/*
	 * If the only valid entry in iattr is ctime we can call
	 * chown(-1,-1) to update the ctime of the file
	 */
	if ((p9attr.valid & (ATTR_UID | ATTR_GID)) ||
	    ((p9attr.valid & ATTR_CTIME)
	     && !((p9attr.valid & ATTR_MASK) & ~ATTR_CTIME))) {
		if (!(p9attr.valid & ATTR_UID))
			p9attr.uid = KUIDT_INIT(-1);

		if (!(p9attr.valid & ATTR_GID))
			p9attr.gid = KGIDT_INIT(-1);

		ret = lchown(fid->abs_path, __kuid_val(p9attr.uid),
				__kgid_val(p9attr.gid));
		if (ret < 0)
			goto err_out;
	}
	if (p9attr.valid & (ATTR_SIZE)) {
		ret = truncate(fid->abs_path, p9attr.size);
		if (ret < 0)
			goto err_out;
	}
	*outlen = VIRTIO_9P_HDR_LEN;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_write(struct p9_dev *p9dev,
			    struct p9_pdu *pdu, u32 *outlen)
{

	u64 offset;
	u32 fid_val;
	u32 count;
	ssize_t res;
	u16 iov_cnt;
	void *iov_base;
	size_t iov_len;
	struct p9_fid *fid;
	/* u32 fid + u64 offset + u32 count */
	int twrite_size = sizeof(u32) + sizeof(u64) + sizeof(u32);

	virtio_p9_pdu_readf(pdu, "dqd", &fid_val, &offset, &count);
	fid = get_fid(p9dev, fid_val);

	iov_base = pdu->out_iov[0].iov_base;
	iov_len  = pdu->out_iov[0].iov_len;
	iov_cnt  = pdu->out_iov_cnt;

	/* Adjust the iovec to skip the header and meta data */
	pdu->out_iov[0].iov_base += (sizeof(struct p9_msg) + twrite_size);
	pdu->out_iov[0].iov_len -=  (sizeof(struct p9_msg) + twrite_size);
	pdu->out_iov_cnt = virtio_p9_update_iov_cnt(pdu->out_iov, count,
						    pdu->out_iov_cnt);
	res = pwritev(fid->fd, pdu->out_iov, pdu->out_iov_cnt, offset);
	/*
	 * Update the iov_base back, so that rest of
	 * pdu_readf works correctly.
	 */
	pdu->out_iov[0].iov_base = iov_base;
	pdu->out_iov[0].iov_len  = iov_len;
	pdu->out_iov_cnt         = iov_cnt;

	if (res < 0)
		goto err_out;
	virtio_p9_pdu_writef(pdu, "d", res);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_remove(struct p9_dev *p9dev,
			     struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	u32 fid_val;
	struct p9_fid *fid;

	virtio_p9_pdu_readf(pdu, "d", &fid_val);
	fid = get_fid(p9dev, fid_val);

	ret = remove(fid->abs_path);
	if (ret < 0)
		goto err_out;
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;

err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_rename(struct p9_dev *p9dev,
			     struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	u32 fid_val, new_fid_val;
	struct p9_fid *fid, *new_fid;
	char full_path[PATH_MAX], *new_name;

	virtio_p9_pdu_readf(pdu, "dds", &fid_val, &new_fid_val, &new_name);
	fid = get_fid(p9dev, fid_val);
	new_fid = get_fid(p9dev, new_fid_val);

	if (get_full_path(full_path, sizeof(full_path), new_fid, new_name) != 0)
		goto err_out;

	ret = rename(fid->abs_path, full_path);
	if (ret < 0)
		goto err_out;
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;

err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_readlink(struct p9_dev *p9dev,
			       struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	u32 fid_val;
	struct p9_fid *fid;
	char target_path[PATH_MAX];

	virtio_p9_pdu_readf(pdu, "d", &fid_val);
	fid = get_fid(p9dev, fid_val);

	memset(target_path, 0, PATH_MAX);
	ret = readlink(fid->abs_path, target_path, PATH_MAX - 1);
	if (ret < 0)
		goto err_out;

	virtio_p9_pdu_writef(pdu, "s", target_path);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_statfs(struct p9_dev *p9dev,
			     struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	u64 fsid;
	u32 fid_val;
	struct p9_fid *fid;
	struct statfs stat_buf;

	virtio_p9_pdu_readf(pdu, "d", &fid_val);
	fid = get_fid(p9dev, fid_val);

	ret = statfs(fid->abs_path, &stat_buf);
	if (ret < 0)
		goto err_out;
	/* FIXME!! f_blocks needs update based on client msize */
	fsid = (unsigned int) stat_buf.f_fsid.__val[0] |
		(unsigned long long)stat_buf.f_fsid.__val[1] << 32;
	virtio_p9_pdu_writef(pdu, "ddqqqqqqd", stat_buf.f_type,
			     stat_buf.f_bsize, stat_buf.f_blocks,
			     stat_buf.f_bfree, stat_buf.f_bavail,
			     stat_buf.f_files, stat_buf.f_ffree,
			     fsid, stat_buf.f_namelen);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_mknod(struct p9_dev *p9dev,
			    struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *name;
	struct stat st;
	struct p9_fid *dfid;
	struct p9_qid qid;
	char full_path[PATH_MAX];
	u32 fid_val, mode, major, minor, gid;

	virtio_p9_pdu_readf(pdu, "dsdddd", &fid_val, &name, &mode,
			    &major, &minor, &gid);

	dfid = get_fid(p9dev, fid_val);

	if (get_full_path(full_path, sizeof(full_path), dfid, name) != 0)
		goto err_out;

	ret = mknod(full_path, mode, makedev(major, minor));
	if (ret < 0)
		goto err_out;

	if (lstat(full_path, &st) < 0)
		goto err_out;

	ret = chmod(full_path, mode & 0777);
	if (ret < 0)
		goto err_out;

	stat2qid(&st, &qid);
	virtio_p9_pdu_writef(pdu, "Q", &qid);
	free(name);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(name);
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_fsync(struct p9_dev *p9dev,
			    struct p9_pdu *pdu, u32 *outlen)
{
	int ret, fd;
	struct p9_fid *fid;
	u32 fid_val, datasync;

	virtio_p9_pdu_readf(pdu, "dd", &fid_val, &datasync);
	fid = get_fid(p9dev, fid_val);

	if (fid->dir)
		fd = dirfd(fid->dir);
	else
		fd = fid->fd;

	if (datasync)
		ret = fdatasync(fd);
	else
		ret = fsync(fd);
	if (ret < 0)
		goto err_out;
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_symlink(struct p9_dev *p9dev,
			      struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	struct stat st;
	u32 fid_val, gid;
	struct p9_qid qid;
	struct p9_fid *dfid;
	char new_name[PATH_MAX];
	char *old_path, *name;

	virtio_p9_pdu_readf(pdu, "dssd", &fid_val, &name, &old_path, &gid);

	dfid = get_fid(p9dev, fid_val);

	if (get_full_path(new_name, sizeof(new_name), dfid, name) != 0)
		goto err_out;

	ret = symlink(old_path, new_name);
	if (ret < 0)
		goto err_out;

	if (lstat(new_name, &st) < 0)
		goto err_out;

	stat2qid(&st, &qid);
	virtio_p9_pdu_writef(pdu, "Q", &qid);
	free(name);
	free(old_path);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(name);
	free(old_path);
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_link(struct p9_dev *p9dev,
			   struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *name;
	u32 fid_val, dfid_val;
	struct p9_fid *dfid, *fid;
	char full_path[PATH_MAX];

	virtio_p9_pdu_readf(pdu, "dds", &dfid_val, &fid_val, &name);

	dfid = get_fid(p9dev, dfid_val);
	fid =  get_fid(p9dev, fid_val);

	if (get_full_path(full_path, sizeof(full_path), dfid, name) != 0)
		goto err_out;

	ret = link(fid->abs_path, full_path);
	if (ret < 0)
		goto err_out;
	free(name);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(name);
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;

}

static void virtio_p9_lock(struct p9_dev *p9dev,
			   struct p9_pdu *pdu, u32 *outlen)
{
	u8 ret;
	u32 fid_val;
	struct p9_flock flock;

	virtio_p9_pdu_readf(pdu, "dbdqqds", &fid_val, &flock.type,
			    &flock.flags, &flock.start, &flock.length,
			    &flock.proc_id, &flock.client_id);

	/* Just return success */
	ret = P9_LOCK_SUCCESS;
	virtio_p9_pdu_writef(pdu, "d", ret);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	free(flock.client_id);
	return;
}

static void virtio_p9_getlock(struct p9_dev *p9dev,
			      struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid_val;
	struct p9_getlock glock;
	virtio_p9_pdu_readf(pdu, "dbqqds", &fid_val, &glock.type,
			    &glock.start, &glock.length, &glock.proc_id,
			    &glock.client_id);

	/* Just return success */
	glock.type = F_UNLCK;
	virtio_p9_pdu_writef(pdu, "bqqds", glock.type,
			     glock.start, glock.length, glock.proc_id,
			     glock.client_id);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	free(glock.client_id);
	return;
}

static int virtio_p9_ancestor(char *path, char *ancestor)
{
	int size = strlen(ancestor);
	if (!strncmp(path, ancestor, size)) {
		/*
		 * Now check whether ancestor is a full name or
		 * or directory component and not just part
		 * of a name.
		 */
		if (path[size] == '\0' || path[size] == '/')
			return 1;
	}
	return 0;
}

static int virtio_p9_fix_path(struct p9_fid *fid, char *old_name, char *new_name)
{
	int ret;
	char *p, tmp_name[PATH_MAX];
	size_t rp_sz = strlen(old_name);

	if (rp_sz == strlen(fid->path)) {
		/* replace the full name */
		p = new_name;
	} else {
		/* save the trailing path details */
		ret = snprintf(tmp_name, sizeof(tmp_name), "%s%s", new_name, fid->path + rp_sz);
		if (ret >= (int)sizeof(tmp_name))
			return -1;
		p = tmp_name;
	}

	return join_path(fid, p);
}

static void rename_fids(struct p9_dev *p9dev, char *old_name, char *new_name)
{
	struct rb_node *node = rb_first(&p9dev->fids);

	while (node) {
		struct p9_fid *fid = rb_entry(node, struct p9_fid, node);

		if (fid->fid != P9_NOFID && virtio_p9_ancestor(fid->path, old_name)) {
				virtio_p9_fix_path(fid, old_name, new_name);
		}
		node = rb_next(node);
	}
}

static void virtio_p9_renameat(struct p9_dev *p9dev,
			       struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *old_name, *new_name;
	u32 old_dfid_val, new_dfid_val;
	struct p9_fid *old_dfid, *new_dfid;
	char old_full_path[PATH_MAX], new_full_path[PATH_MAX];


	virtio_p9_pdu_readf(pdu, "dsds", &old_dfid_val, &old_name,
			    &new_dfid_val, &new_name);

	old_dfid = get_fid(p9dev, old_dfid_val);
	new_dfid = get_fid(p9dev, new_dfid_val);

	if (get_full_path(old_full_path, sizeof(old_full_path), old_dfid, old_name) != 0)
		goto err_out;

	if (get_full_path(new_full_path, sizeof(new_full_path), new_dfid, new_name) != 0)
		goto err_out;

	ret = rename(old_full_path, new_full_path);
	if (ret < 0)
		goto err_out;
	/*
	 * Now fix path in other fids, if the renamed path is part of
	 * that.
	 */
	rename_fids(p9dev, old_name, new_name);
	free(old_name);
	free(new_name);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(old_name);
	free(new_name);
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_unlinkat(struct p9_dev *p9dev,
			       struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *name;
	u32 fid_val, flags;
	struct p9_fid *fid;
	char full_path[PATH_MAX];

	virtio_p9_pdu_readf(pdu, "dsd", &fid_val, &name, &flags);
	fid = get_fid(p9dev, fid_val);

	if (get_full_path(full_path, sizeof(full_path), fid, name) != 0)
		goto err_out;

	ret = remove(full_path);
	if (ret < 0)
		goto err_out;
	free(name);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(name);
	virtio_p9_error_reply(p9dev, pdu, errno, outlen);
	return;
}

static void virtio_p9_flush(struct p9_dev *p9dev,
				struct p9_pdu *pdu, u32 *outlen)
{
	u16 tag, oldtag;

	virtio_p9_pdu_readf(pdu, "ww", &tag, &oldtag);
	virtio_p9_pdu_writef(pdu, "w", tag);
	*outlen = pdu->write_offset;
	virtio_p9_set_reply_header(pdu, *outlen);

	return;
}

static void virtio_p9_eopnotsupp(struct p9_dev *p9dev,
				 struct p9_pdu *pdu, u32 *outlen)
{
	return virtio_p9_error_reply(p9dev, pdu, EOPNOTSUPP, outlen);
}

typedef void p9_handler(struct p9_dev *p9dev,
			struct p9_pdu *pdu, u32 *outlen);

/* FIXME should be removed when merging with latest linus tree */
#define P9_TRENAMEAT 74
#define P9_TUNLINKAT 76

static p9_handler *virtio_9p_dotl_handler [] = {
	[P9_TREADDIR]     = virtio_p9_readdir,
	[P9_TSTATFS]      = virtio_p9_statfs,
	[P9_TGETATTR]     = virtio_p9_getattr,
	[P9_TSETATTR]     = virtio_p9_setattr,
	[P9_TXATTRWALK]   = virtio_p9_eopnotsupp,
	[P9_TXATTRCREATE] = virtio_p9_eopnotsupp,
	[P9_TMKNOD]       = virtio_p9_mknod,
	[P9_TLOCK]        = virtio_p9_lock,
	[P9_TGETLOCK]     = virtio_p9_getlock,
	[P9_TRENAMEAT]    = virtio_p9_renameat,
	[P9_TREADLINK]    = virtio_p9_readlink,
	[P9_TUNLINKAT]    = virtio_p9_unlinkat,
	[P9_TMKDIR]       = virtio_p9_mkdir,
	[P9_TVERSION]     = virtio_p9_version,
	[P9_TLOPEN]       = virtio_p9_open,
	[P9_TATTACH]      = virtio_p9_attach,
	[P9_TWALK]        = virtio_p9_walk,
	[P9_TCLUNK]       = virtio_p9_clunk,
	[P9_TFSYNC]       = virtio_p9_fsync,
	[P9_TREAD]        = virtio_p9_read,
	[P9_TFLUSH]       = virtio_p9_flush,
	[P9_TLINK]        = virtio_p9_link,
	[P9_TSYMLINK]     = virtio_p9_symlink,
	[P9_TLCREATE]     = virtio_p9_create,
	[P9_TWRITE]       = virtio_p9_write,
	[P9_TREMOVE]      = virtio_p9_remove,
	[P9_TRENAME]      = virtio_p9_rename,
};

static struct p9_pdu *virtio_p9_pdu_init(struct kvm *kvm, struct virt_queue *vq)
{
	struct p9_pdu *pdu = calloc(1, sizeof(*pdu));
	if (!pdu)
		return NULL;

	/* skip the pdu header p9_msg */
	pdu->read_offset	= VIRTIO_9P_HDR_LEN;
	pdu->write_offset	= VIRTIO_9P_HDR_LEN;
	pdu->queue_head		= virt_queue__get_inout_iov(kvm, vq, pdu->in_iov,
					pdu->out_iov, &pdu->in_iov_cnt, &pdu->out_iov_cnt);
	return pdu;
}

static u8 virtio_p9_get_cmd(struct p9_pdu *pdu)
{
	struct p9_msg *msg;
	/*
	 * we can peek directly into pdu for a u8
	 * value. The host endianess won't be an issue
	 */
	msg = pdu->out_iov[0].iov_base;
	return msg->cmd;
}

static u32 virtio_p9_pdu_write_to_file(struct p9_pdu *p9pdu)
{
    FILE *fp = NULL;
	u32 outlen = 0;

	//printf("[JB] virtio_p9_pdu_write_to_file start!\n");

    // 1. write request
    fp = fopen("/shared/p9req.bin", "wb");
    if (fp) {
        fwrite((unsigned char *)p9pdu, sizeof(unsigned char), sizeof(struct p9_pdu)/sizeof(unsigned char), fp);
        fclose(fp);
        //printf("[JB] p9req write!\n");
    } else {
        //printf("[JB] p9req write fail..\n");
    }

	// 2. check request. wait for it to get deleted.
	while (true) {
		sync();
        if (access("/shared/p9req.bin", F_OK) == 0) {
            sleep(1);
        } else {
            break;
        }
    }
	//printf("[JB] try to read resp!\n");

	// 3. read resp
	fp = fopen("/shared/p9resp.bin", "rb");
	if (fp) {
		fread(&outlen, sizeof(outlen), 1, fp);
		fclose(fp);
		//printf("[JB] read resp! %d\n", outlen);
	}

	// 4. read result
	return outlen;
}

// test
extern void print_host_mem_with_offset(struct kvm *kvm, u64 offset);

static bool virtio_p9_do_io_request(struct kvm *kvm, struct p9_dev_job *job)
{
	u8 cmd;
	u32 len = 0;
	p9_handler *handler;
	struct p9_dev *p9dev;
	struct virt_queue *vq;
	struct p9_pdu *p9pdu;

	vq = job->vq;
	p9dev = job->p9dev;

	p9pdu = virtio_p9_pdu_init(kvm, vq);

    if ( is_no_shared_region(kvm) ) {
		// no_shared_region
		// for CVM_App

		// 1. wait for CVM_GW (virtio backend) to handle this request
		int dummy = 0;
		int res;

		//len = virtio_p9_pdu_write_to_file(p9pdu);
		res = send_msg(&dummy, sizeof(dummy), true);
		if (res < 0) {
			printf("send_msg from app to gw error: %d, %s\n", errno, strerror(errno));
			return false;
		}
		res = receive_msg(&len, sizeof(len), true);
		if (res < 0) {
			printf("receive_msg from gw to app error, %d, %s\n", errno, strerror(errno));
			return false;
		}

		// 2. update results
		virt_queue__set_used_elem(vq, p9pdu->queue_head, len);
		free(p9pdu);
        return true;
    } else {
		// for CVM_GW
		int res;

		cmd = virtio_p9_get_cmd(p9pdu);
		//printf("[host] in_cnt: %d, out_cnt: %d, cmd: %d\n", p9pdu->in_iov_cnt, p9pdu->out_iov_cnt, cmd); // [test]

		if ((cmd >= ARRAY_SIZE(virtio_9p_dotl_handler)) ||
			!virtio_9p_dotl_handler[cmd])
			handler = virtio_p9_eopnotsupp;
		else
			handler = virtio_9p_dotl_handler[cmd];

		handler(p9dev, p9pdu, &len);

		#if 0
		// check if this msg_queue is already created by CVM_App. send_msg only if so
		if (queue_exist(false)) {
			// log to compare outlen between host and cvm_gw
			printf("[host] in_cnt: %d, out_cnt: %d, cmd: %d, outlen: %d\n", p9pdu->in_iov_cnt, p9pdu->out_iov_cnt, cmd, len);

			res = send_msg(&len, sizeof(len), false);
			if (res < 0) {
				printf("send_msg from gw to app error (for outlen)\n");
			} else {
				printf("send_msg for outlen success!\n");
			}
		}
		#endif

		// [test]
		//print_host_mem_with_offset(kvm, 0x88400000 + (60 * 1024 * 1024));
		//print_host_mem_with_offset(kvm, 0x88400000 + (14 * 1024 * 1024));

		virt_queue__set_used_elem(vq, p9pdu->queue_head, len);
		free(p9pdu);
		return true;
    }
}

static void virtio_p9_do_io(struct kvm *kvm, void *param)
{
	struct p9_dev_job *job = (struct p9_dev_job *)param;
	struct p9_dev *p9dev   = job->p9dev;
	struct virt_queue *vq  = job->vq;

	while (virt_queue__available(vq)) {
		virtio_p9_do_io_request(kvm, job);
		p9dev->vdev.ops->signal_vq(kvm, &p9dev->vdev, vq - p9dev->vqs);
	}
}

static u8 *get_config(struct kvm *kvm, void *dev)
{
	struct p9_dev *p9dev = dev;

	return ((u8 *)(p9dev->config));
}

static size_t get_config_size(struct kvm *kvm, void *dev)
{
	struct p9_dev *p9dev = dev;

	return p9dev->config_size;
}

static u64 get_host_features(struct kvm *kvm, void *dev)
{
	return 1 << VIRTIO_9P_MOUNT_TAG;
}

static void notify_status(struct kvm *kvm, void *dev, u32 status)
{
	struct p9_dev *p9dev = dev;
	struct p9_fid *pfid, *next;

	if (status & VIRTIO__STATUS_CONFIG)
		p9dev->config->tag_len = virtio_host_to_guest_u16(&p9dev->vdev,
								  p9dev->tag_len);

	if (!(status & VIRTIO__STATUS_STOP))
		return;

	rbtree_postorder_for_each_entry_safe(pfid, next, &p9dev->fids, node)
		close_fid(p9dev, pfid->fid);
}

static int init_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct p9_dev *p9dev = dev;
	struct p9_dev_job *job;
	struct virt_queue *queue;

	compat__remove_message(compat_id);

	queue		= &p9dev->vqs[vq];
	job		= &p9dev->jobs[vq];

	virtio_init_device_vq(kvm, &p9dev->vdev, queue, VIRTQUEUE_NUM);

	*job		= (struct p9_dev_job) {
		.vq		= queue,
		.p9dev		= p9dev,
	};
	thread_pool__init_job(&job->job_id, kvm, virtio_p9_do_io, job);

	return 0;
}

static void exit_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct p9_dev *p9dev = dev;

	thread_pool__cancel_job(&p9dev->jobs[vq].job_id);
}

static int notify_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct p9_dev *p9dev = dev;

	thread_pool__do_job(&p9dev->jobs[vq].job_id);

	return 0;
}

static struct virt_queue *get_vq(struct kvm *kvm, void *dev, u32 vq)
{
	struct p9_dev *p9dev = dev;

	return &p9dev->vqs[vq];
}

static int get_size_vq(struct kvm *kvm, void *dev, u32 vq)
{
	return VIRTQUEUE_NUM;
}

static int set_size_vq(struct kvm *kvm, void *dev, u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static unsigned int get_vq_count(struct kvm *kvm, void *dev)
{
	return NUM_VIRT_QUEUES;
}

struct virtio_ops p9_dev_virtio_ops = {
	.get_config		= get_config,
	.get_config_size	= get_config_size,
	.get_host_features	= get_host_features,
	.init_vq		= init_vq,
	.exit_vq		= exit_vq,
	.notify_status		= notify_status,
	.notify_vq		= notify_vq,
	.get_vq			= get_vq,
	.get_size_vq		= get_size_vq,
	.set_size_vq		= set_size_vq,
	.get_vq_count		= get_vq_count,
};

int virtio_9p_rootdir_parser(const struct option *opt, const char *arg, int unset)
{
	char *tag_name;
	char tmp[PATH_MAX];
	struct kvm *kvm = opt->ptr;

	/*
	 * 9p dir can be of the form dirname,tag_name or
	 * just dirname. In the later case we use the
	 * default tag name
	 */
	tag_name = strstr(arg, ",");
	if (tag_name) {
		*tag_name = '\0';
		tag_name++;
	}
	if (realpath(arg, tmp)) {
		if (virtio_9p__register(kvm, tmp, tag_name) < 0)
			die("Unable to initialize virtio 9p");
	} else
		die("Failed resolving 9p path");
	return 0;
}

int virtio_9p_img_name_parser(const struct option *opt, const char *arg, int unset)
{
	char path[PATH_MAX];
	struct stat st;
	struct kvm *kvm = opt->ptr;

	if (stat(arg, &st) == 0 &&
	    S_ISDIR(st.st_mode)) {
		char tmp[PATH_MAX];

		if (kvm->cfg.using_rootfs)
			die("Please use only one rootfs directory atmost");

		if (realpath(arg, tmp) == 0 ||
		    virtio_9p__register(kvm, tmp, "/dev/root") < 0)
			die("Unable to initialize virtio 9p");
		kvm->cfg.using_rootfs = 1;
		return 0;
	}

	snprintf(path, PATH_MAX, "%s%s", kvm__get_dir(), arg);

	if (stat(path, &st) == 0 &&
	    S_ISDIR(st.st_mode)) {
		char tmp[PATH_MAX];

		if (kvm->cfg.using_rootfs)
			die("Please use only one rootfs directory atmost");

		if (realpath(path, tmp) == 0 ||
		    virtio_9p__register(kvm, tmp, "/dev/root") < 0)
			die("Unable to initialize virtio 9p");
		if (virtio_9p__register(kvm, "/", "hostfs") < 0)
			die("Unable to initialize virtio 9p");
		kvm_setup_resolv(arg);
		kvm->cfg.using_rootfs = kvm->cfg.custom_rootfs = 1;
		kvm->cfg.custom_rootfs_name = arg;
		return 0;
	}

	return -1;
}

int virtio_9p__init(struct kvm *kvm)
{
	struct p9_dev *p9dev;
	int r;

	list_for_each_entry(p9dev, &devs, list) {
		r = virtio_init(kvm, p9dev, &p9dev->vdev, &p9_dev_virtio_ops,
				VIRTIO_DEFAULT_TRANS(kvm), PCI_DEVICE_ID_VIRTIO_9P,
				VIRTIO_ID_9P, PCI_CLASS_9P);
		if (r < 0)
			return r;
	}

	//print_host_mem_with_offset(kvm, 0x88400000 + (14 * 1024 * 1024));
	//print_host_mem_with_offset(kvm, 0x188400000 + (14 * 1024 * 1024));
	return 0;
}
virtio_dev_init(virtio_9p__init);

int virtio_9p__register(struct kvm *kvm, const char *root, const char *tag_name)
{
	struct p9_dev *p9dev;
	size_t tag_length;
	size_t config_size;
	int err;

	p9dev = calloc(1, sizeof(*p9dev));
	if (!p9dev)
		return -ENOMEM;

	if (!tag_name)
		tag_name = VIRTIO_9P_DEFAULT_TAG;

	tag_length = strlen(tag_name);
	/* The tag_name zero byte is intentionally excluded */
	config_size = sizeof(*p9dev->config) + tag_length;

	p9dev->config = calloc(1, config_size);
	if (p9dev->config == NULL) {
		err = -ENOMEM;
		goto free_p9dev;
	}
	p9dev->config_size = config_size;

	strncpy(p9dev->root_dir, root, sizeof(p9dev->root_dir));
	p9dev->root_dir[sizeof(p9dev->root_dir)-1] = '\x00';

	p9dev->tag_len = tag_length;
	if (p9dev->tag_len > MAX_TAG_LEN) {
		err = -EINVAL;
		goto free_p9dev_config;
	}

	memcpy(&p9dev->config->tag, tag_name, tag_length);

	list_add(&p9dev->list, &devs);

	if (compat_id == -1)
		compat_id = virtio_compat_add_message("virtio-9p", "CONFIG_NET_9P_VIRTIO");

	return 0;

free_p9dev_config:
	free(p9dev->config);
free_p9dev:
	free(p9dev);
	return err;
}
