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

// =========================================================
// log
#define LOG_ON_ERROR
//#define LOG_ON_DEBUG

#ifdef LOG_ON_DEBUG
#define LOG_DEBUG(...) \
do { \
	printf(__VA_ARGS__); \
} while(0)
#else
#define LOG_DEBUG(...) do {} while (0)
#endif

#ifdef LOG_ON_ERROR
#define LOG_ERROR(...) \
do { \
	printf(__VA_ARGS__); \
} while(0)
#else
#define LOG_ERROR(...) do {} while (0)
#endif
// =========================================================

// =========================================================
// contexts
static struct rb_root vm_fids;   // equivalent to struct p9_dev.fids;
static char vm_root_dir[PATH_MAX] = {0,};  // equivalent to struct p9_dev.root_dir;

//static unsigned char vring_mirror[64 * 1024 * 1024] __attribute__((aligned(4096))) = {0,};
#define VRING_SIZE (8 * 1024 * 1024)
#define VQ_ELEM_SIZE (8 * 1024)
static unsigned char *vring_mirror = NULL;
static unsigned char *vq_elem_mirror = NULL;
static char vring_shared[VRING_SIZE] = {0,};
static unsigned long base_ipa_addr = 0x88200000;
static unsigned long base_ipa_elem_addr = 0x8c260000;

int vm_virtio_p9_pdu_readf(struct p9_pdu *pdu, const char *fmt, ...);
int vm_virtio_p9_pdu_writef(struct p9_pdu *pdu, const char *fmt, ...);
// =========================================================

// =========================================================
// helper functions of p9_pdu
static void vm_virtio_p9_pdu_read(struct p9_pdu *pdu, void *data, size_t size)
{
	size_t len;
	int i, copied = 0;
	u16 iov_cnt = pdu->out_iov_cnt;
	size_t offset = pdu->read_offset;
	struct iovec *iov = pdu->out_iov;

	for (i = 0; i < iov_cnt && size; i++) {
		if (offset >= iov[i].iov_len) {
			offset -= iov[i].iov_len;
			continue;
		} else {
			len = MIN(iov[i].iov_len - offset, size);
			memcpy(data, iov[i].iov_base + offset, len);
			size -= len;
			data += len;
			offset = 0;
			copied += len;
		}
	}
	pdu->read_offset += copied;
}

static void vm_virtio_p9_pdu_write(struct p9_pdu *pdu,
				const void *data, size_t size)
{
	size_t len;
	int i, copied = 0;
	u16 iov_cnt = pdu->in_iov_cnt;
	size_t offset = pdu->write_offset;
	struct iovec *iov = pdu->in_iov;

	for (i = 0; i < iov_cnt && size; i++) {
		if (offset >= iov[i].iov_len) {
			offset -= iov[i].iov_len;
			continue;
		} else {
			len = MIN(iov[i].iov_len - offset, size);
			memcpy(iov[i].iov_base + offset, data, len);
			size -= len;
			data += len;
			offset = 0;
			copied += len;
		}
	}
	pdu->write_offset += copied;
}

static void vm_virtio_p9_wstat_free(struct p9_wstat *stbuf)
{
	free(stbuf->name);
	free(stbuf->uid);
	free(stbuf->gid);
	free(stbuf->muid);
}

static int vm_virtio_p9_decode(struct p9_pdu *pdu, const char *fmt, va_list ap)
{
	int retval = 0;
	const char *ptr;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
		case 'b':
		{
			int8_t *val = va_arg(ap, int8_t *);
			vm_virtio_p9_pdu_read(pdu, val, sizeof(*val));
		}
		break;
		case 'w':
		{
			int16_t le_val;
			int16_t *val = va_arg(ap, int16_t *);
			vm_virtio_p9_pdu_read(pdu, &le_val, sizeof(le_val));
			*val = le16toh(le_val);
		}
		break;
		case 'd':
		{
			int32_t le_val;
			int32_t *val = va_arg(ap, int32_t *);
			vm_virtio_p9_pdu_read(pdu, &le_val, sizeof(le_val));
			*val = le32toh(le_val);
		}
		break;
		case 'q':
		{
			int64_t le_val;
			int64_t *val = va_arg(ap, int64_t *);
			vm_virtio_p9_pdu_read(pdu, &le_val, sizeof(le_val));
			*val = le64toh(le_val);
		}
		break;
		case 's':
		{
			int16_t len;
			char **str = va_arg(ap, char **);

			vm_virtio_p9_pdu_readf(pdu, "w", &len);
			*str = malloc(len + 1);
			if (*str == NULL) {
				retval = ENOMEM;
				break;
			}
			vm_virtio_p9_pdu_read(pdu, *str, len);
			(*str)[len] = 0;
		}
		break;
		case 'Q':
		{
			struct p9_qid *qid = va_arg(ap, struct p9_qid *);
			retval = vm_virtio_p9_pdu_readf(pdu, "bdq",
						     &qid->type, &qid->version,
						     &qid->path);
		}
		break;
		case 'S':
		{
			struct p9_wstat *stbuf = va_arg(ap, struct p9_wstat *);
			memset(stbuf, 0, sizeof(struct p9_wstat));
			stbuf->n_uid = KUIDT_INIT(-1);
			stbuf->n_gid = KGIDT_INIT(-1);
			stbuf->n_muid = KUIDT_INIT(-1);
			retval = vm_virtio_p9_pdu_readf(pdu, "wwdQdddqssss",
						&stbuf->size, &stbuf->type,
						&stbuf->dev, &stbuf->qid,
						&stbuf->mode, &stbuf->atime,
						&stbuf->mtime, &stbuf->length,
						&stbuf->name, &stbuf->uid,
						&stbuf->gid, &stbuf->muid);
			if (retval)
				vm_virtio_p9_wstat_free(stbuf);
		}
		break;
		case 'I':
		{
			struct p9_iattr_dotl *p9attr = va_arg(ap,
						       struct p9_iattr_dotl *);

			retval = vm_virtio_p9_pdu_readf(pdu, "ddddqqqqq",
						     &p9attr->valid,
						     &p9attr->mode,
						     &p9attr->uid,
						     &p9attr->gid,
						     &p9attr->size,
						     &p9attr->atime_sec,
						     &p9attr->atime_nsec,
						     &p9attr->mtime_sec,
						     &p9attr->mtime_nsec);
		}
		break;
		default:
			retval = EINVAL;
			break;
		}
	}
	return retval;
}

static int vm_virtio_p9_pdu_encode(struct p9_pdu *pdu, const char *fmt, va_list ap)
{
	int retval = 0;
	const char *ptr;

	for (ptr = fmt; *ptr; ptr++) {
		switch (*ptr) {
		case 'b':
		{
			int8_t val = va_arg(ap, int);
			vm_virtio_p9_pdu_write(pdu, &val, sizeof(val));
		}
		break;
		case 'w':
		{
			int16_t val = htole16(va_arg(ap, int));
			vm_virtio_p9_pdu_write(pdu, &val, sizeof(val));
		}
		break;
		case 'd':
		{
			int32_t val = htole32(va_arg(ap, int32_t));
			vm_virtio_p9_pdu_write(pdu, &val, sizeof(val));
		}
		break;
		case 'q':
		{
			int64_t val = htole64(va_arg(ap, int64_t));
			vm_virtio_p9_pdu_write(pdu, &val, sizeof(val));
		}
		break;
		case 's':
		{
			uint16_t len = 0;
			const char *s = va_arg(ap, char *);
			if (s)
				len = MIN(strlen(s), USHRT_MAX);
			vm_virtio_p9_pdu_writef(pdu, "w", len);
			vm_virtio_p9_pdu_write(pdu, s, len);
		}
		break;
		case 'Q':
		{
			struct p9_qid *qid = va_arg(ap, struct p9_qid *);
			retval = vm_virtio_p9_pdu_writef(pdu, "bdq",
						      qid->type, qid->version,
						      qid->path);
		}
		break;
		case 'S':
		{
			struct p9_wstat *stbuf = va_arg(ap, struct p9_wstat *);
			retval = vm_virtio_p9_pdu_writef(pdu, "wwdQdddqssss",
						stbuf->size, stbuf->type,
						stbuf->dev, &stbuf->qid,
						stbuf->mode, stbuf->atime,
						stbuf->mtime, stbuf->length,
						stbuf->name, stbuf->uid,
						stbuf->gid, stbuf->muid);
		}
		break;
		case 'A':
		{
			struct p9_stat_dotl *stbuf = va_arg(ap,
						      struct p9_stat_dotl *);
			retval  = vm_virtio_p9_pdu_writef(pdu,
						       "qQdddqqqqqqqqqqqqqqq",
						       stbuf->st_result_mask,
						       &stbuf->qid,
						       stbuf->st_mode,
						       stbuf->st_uid,
						       stbuf->st_gid,
						       stbuf->st_nlink,
						       stbuf->st_rdev,
						       stbuf->st_size,
						       stbuf->st_blksize,
						       stbuf->st_blocks,
						       stbuf->st_atime_sec,
						       stbuf->st_atime_nsec,
						       stbuf->st_mtime_sec,
						       stbuf->st_mtime_nsec,
						       stbuf->st_ctime_sec,
						       stbuf->st_ctime_nsec,
						       stbuf->st_btime_sec,
						       stbuf->st_btime_nsec,
						       stbuf->st_gen,
						       stbuf->st_data_version);
		}
		break;
		default:
			retval = EINVAL;
			break;
		}
	}
	return retval;
}

int vm_virtio_p9_pdu_readf(struct p9_pdu *pdu, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vm_virtio_p9_decode(pdu, fmt, ap);
	va_end(ap);

	return ret;
}

int vm_virtio_p9_pdu_writef(struct p9_pdu *pdu, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vm_virtio_p9_pdu_encode(pdu, fmt, ap);
	va_end(ap);

	return ret;
}
// =========================================================

// =========================================================
// helper functions of contexts (e.g., fid)
static int vm_insert_new_fid(struct p9_fid *fid);
static struct p9_fid *vm_find_or_create_fid(u32 fid)
{
	struct rb_node *node = vm_fids.rb_node;
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

	len = strlen(vm_root_dir);
	if (len >= sizeof(pfid->abs_path)) {
		free(pfid);
		return NULL;
	}

	pfid->fid = fid;
	strcpy(pfid->abs_path, vm_root_dir);
	pfid->path = pfid->abs_path + strlen(pfid->abs_path);

	vm_insert_new_fid(pfid);

	return pfid;
}

static int vm_insert_new_fid(struct p9_fid *fid)
{
	struct rb_node **node = &(vm_fids.rb_node), *parent = NULL;

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
	rb_insert_color(&fid->node, &vm_fids);
	return 0;
}

static struct p9_fid *vm_get_fid(int fid)
{
	struct p9_fid *new;

	new = vm_find_or_create_fid(fid);

	return new;
}

static void vm_stat2qid(struct stat *st, struct p9_qid *qid)
{
	*qid = (struct p9_qid) {
		.path		= st->st_ino,
		.version	= st->st_mtime,
	};

	if (S_ISDIR(st->st_mode))
		qid->type	|= P9_QTDIR;
}

static void vm_close_fid(u32 fid)
{
	struct p9_fid *pfid = vm_get_fid(fid);

	if (pfid->fd > 0)
		close(pfid->fd);

	if (pfid->dir)
		closedir(pfid->dir);

	rb_erase(&pfid->node, &vm_fids);
	free(pfid);
}

static bool vm_is_dir(struct p9_fid *fid)
{
	struct stat st;

	stat(fid->abs_path, &st);

	return S_ISDIR(st.st_mode);
}

/*
 * FIXME!! Need to map to protocol independent value. Upstream
 * 9p also have the same BUG
 */
static int vm_virtio_p9_openflags(int flags)
{
	flags &= ~(O_NOCTTY | O_ASYNC | O_CREAT | O_DIRECT);
	flags |= O_NOFOLLOW;
	return flags;
}

static int vm_virtio_p9_dentry_size(struct dirent *dent)
{
	/*
	 * Size of each dirent:
	 * qid(13) + offset(8) + type(1) + name_len(2) + name
	 */
	return 24 + strlen(dent->d_name);
}
// =========================================================

// =========================================================
// helper functions of p9
static void vm_virtio_p9_set_reply_header(struct p9_pdu *pdu, u32 size)
{
	u8 cmd;
	u16 tag;

	pdu->read_offset = sizeof(u32);
	vm_virtio_p9_pdu_readf(pdu, "bw", &cmd, &tag);
	pdu->write_offset = 0;
	/* cmd + 1 is the reply message */
	vm_virtio_p9_pdu_writef(pdu, "dbw", size, cmd + 1, tag);

	// [JB] print addr!
	/*
	do {
		struct iovec *iov = pdu->in_iov;
		printf("[JB] vring_mirror addr: %lx, size: %d, cmd: %d, tag: %d\n", (unsigned long)vring_mirror, size, cmd + 1, tag);

		for (unsigned i = 0; i < pdu->in_iov_cnt; i++) {
			printf("[JB] set_reply_header, %d, iov_base: %lx, offset: %lx\n", i, (unsigned long)iov[i].iov_base, (unsigned long)iov[i].iov_base - (unsigned long)vring_mirror);
		}
	} while(0); */
}

static u16 vm_virtio_p9_update_iov_cnt(struct iovec iov[], u32 count, int iov_cnt)
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

static void vm_virtio_p9_error_reply(struct p9_pdu *pdu, int err, u32 *outlen)
{
	u16 tag;

	/* EMFILE at server implies ENFILE for the VM */
	if (err == EMFILE)
		err = ENFILE;

	pdu->write_offset = VIRTIO_9P_HDR_LEN;
	vm_virtio_p9_pdu_writef(pdu, "d", err);
	*outlen = pdu->write_offset;

	/* read the tag from input */
	pdu->read_offset = sizeof(u32) + sizeof(u8);
	vm_virtio_p9_pdu_readf(pdu, "w", &tag);

	/* Update the header */
	pdu->write_offset = 0;
	vm_virtio_p9_pdu_writef(pdu, "dbw", *outlen, P9_RLERROR, tag);
}

static void vm_virtio_p9_fill_stat(struct stat *st, struct p9_stat_dotl *statl)
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
	vm_stat2qid(st, &statl->qid);
}

static int vm_join_path(struct p9_fid *fid, const char *name)
{
	size_t len, size;

	size = sizeof(fid->abs_path) - (fid->path - fid->abs_path);
	len = strlen(name);
	if (len >= size)
		return -1;

	strncpy(fid->path, name, size);
	return 0;
}

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

static int vm_stat_rel(const char *path, struct stat *st)
{
	char full_path[PATH_MAX];

	if (get_full_path_helper(full_path, sizeof(full_path), vm_root_dir, path) != 0)
		return -1;

	if (lstat(full_path, st) != 0)
		return -1;

	return 0;
}

static int vm_virtio_p9_ancestor(char *path, char *ancestor)
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

static int vm_virtio_p9_fix_path(struct p9_fid *fid, char *old_name, char *new_name)
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

	return vm_join_path(fid, p);
}

static void vm_rename_fids(char *old_name, char *new_name)
{
	struct rb_node *node = rb_first(&vm_fids);

	while (node) {
		struct p9_fid *fid = rb_entry(node, struct p9_fid, node);

		if (fid->fid != P9_NOFID && vm_virtio_p9_ancestor(fid->path, old_name)) {
				vm_virtio_p9_fix_path(fid, old_name, new_name);
		}
		node = rb_next(node);
	}
}
// =========================================================

// =========================================================
// handlers
static void vm_virtio_p9_version(struct p9_pdu *pdu, u32 *outlen)
{
	u32 msize;
	char *version;

	vm_virtio_p9_pdu_readf(pdu, "ds", &msize, &version);

	/*
	 * reply with the same msize the client sent us
	 * Error out if the request is not for 9P2000.L
	 */
	if (!strcmp(version, VIRTIO_9P_VERSION_DOTL)) {
		vm_virtio_p9_pdu_writef(pdu, "ds", msize, version);
	}
	else {
		vm_virtio_p9_pdu_writef(pdu, "ds", msize, "unknown");
	}

	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	free(version);
	return;
}

static void vm_virtio_p9_attach(struct p9_pdu *pdu, u32 *outlen)
{
	char *uname;
	char *aname;
	struct stat st;
	struct p9_qid qid;
	struct p9_fid *fid;
	u32 fid_val, afid, uid;

	vm_virtio_p9_pdu_readf(pdu, "ddssd", &fid_val, &afid, &uname, &aname, &uid);

	free(uname);
	free(aname);

	if (lstat(vm_root_dir, &st) < 0) {
		goto err_out;
	}

	vm_stat2qid(&st, &qid);

	fid = vm_get_fid(fid_val);
	fid->uid = uid;
	if (vm_join_path(fid, "/") != 0) {
		errno = ENAMETOOLONG;
		goto err_out;
	}

	vm_virtio_p9_pdu_writef(pdu, "Q", &qid);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_getattr(struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid_val;
	struct stat st;
	u64 request_mask;
	struct p9_fid *fid;
	struct p9_stat_dotl statl;

	vm_virtio_p9_pdu_readf(pdu, "dq", &fid_val, &request_mask);
	fid = vm_get_fid(fid_val);
	if (lstat(fid->abs_path, &st) < 0)
		goto err_out;

	vm_virtio_p9_fill_stat(&st, &statl);
	
	vm_virtio_p9_pdu_writef(pdu, "A", &statl);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_walk(struct p9_pdu *pdu, u32 *outlen)
{
	u8 i;
	u16 nwqid;
	u16 nwname;
	struct p9_qid wqid;
	struct p9_fid *new_fid, *old_fid;
	u32 fid_val, newfid_val;

	vm_virtio_p9_pdu_readf(pdu, "ddw", &fid_val, &newfid_val, &nwname);
	new_fid	= vm_get_fid(newfid_val);

	nwqid = 0;
	if (nwname) {
		struct p9_fid *fid = vm_get_fid(fid_val);

		if (vm_join_path(new_fid, fid->path) != 0) {
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

			vm_virtio_p9_pdu_readf(pdu, "s", &str);

			/* Format the new path we're 'walk'ing into */
			ret = snprintf(tmp, sizeof(tmp), "%s/%s", new_fid->path, str);
			if (ret >= (int)sizeof(tmp)) {
				errno = ENAMETOOLONG;
				goto err_out;
			}

			free(str);

			if (vm_stat_rel(tmp, &st) != 0)
				goto err_out;

			vm_stat2qid(&st, &wqid);
			if (vm_join_path(new_fid, tmp) != 0) {
				errno = ENAMETOOLONG;
				goto err_out;
			}
			new_fid->uid = fid->uid;
			nwqid++;
			vm_virtio_p9_pdu_writef(pdu, "Q", &wqid);
		}
	} else {
		/*
		 * update write_offset so our outlen get correct value
		 */
		pdu->write_offset += sizeof(u16);
		old_fid = vm_get_fid(fid_val);
		if (vm_join_path(new_fid, old_fid->path) != 0) {
			errno = ENAMETOOLONG;
			goto err_out;
		}
		new_fid->uid    = old_fid->uid;
	}
	*outlen = pdu->write_offset;
	pdu->write_offset = VIRTIO_9P_HDR_LEN;
	vm_virtio_p9_pdu_writef(pdu, "d", nwqid);
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_clunk(struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid;

	vm_virtio_p9_pdu_readf(pdu, "d", &fid);
	vm_close_fid(fid);

	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
}

static void vm_virtio_p9_open(struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid, flags;
	struct stat st;
	struct p9_qid qid;
	struct p9_fid *new_fid;

	vm_virtio_p9_pdu_readf(pdu, "dd", &fid, &flags);
	new_fid = vm_get_fid(fid);

	if (lstat(new_fid->abs_path, &st) < 0)
		goto err_out;

	vm_stat2qid(&st, &qid);

	if (vm_is_dir(new_fid)) {
		new_fid->dir = opendir(new_fid->abs_path);
		if (!new_fid->dir)
			goto err_out;
	} else {
		new_fid->fd  = open(new_fid->abs_path,
				    vm_virtio_p9_openflags(flags));
		if (new_fid->fd < 0)
			goto err_out;
	}
	/* FIXME!! need ot send proper iounit  */
	vm_virtio_p9_pdu_writef(pdu, "Qd", &qid, 0);

	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_read(struct p9_pdu *pdu, u32 *outlen)
{
	u64 offset;
	u32 fid_val;
	u16 iov_cnt;
	void *iov_base;
	size_t iov_len;
	u32 count, rcount;
	struct p9_fid *fid;

	rcount = 0;
	vm_virtio_p9_pdu_readf(pdu, "dqd", &fid_val, &offset, &count);
	fid = vm_get_fid(fid_val);

	iov_base = pdu->in_iov[0].iov_base;
	iov_len  = pdu->in_iov[0].iov_len;
	iov_cnt  = pdu->in_iov_cnt;
	pdu->in_iov[0].iov_base += VIRTIO_9P_HDR_LEN + sizeof(u32);
	pdu->in_iov[0].iov_len -= VIRTIO_9P_HDR_LEN + sizeof(u32);
	pdu->in_iov_cnt = vm_virtio_p9_update_iov_cnt(pdu->in_iov,
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
	vm_virtio_p9_pdu_writef(pdu, "d", rcount);
	*outlen = pdu->write_offset + rcount;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
}

static void vm_virtio_p9_readdir(struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid_val;
	u32 count, rcount;
	struct stat st;
	struct p9_fid *fid;
	struct dirent *dent;
	u64 offset, old_offset;

	rcount = 0;
	vm_virtio_p9_pdu_readf(pdu, "dqd", &fid_val, &offset, &count);
	fid = vm_get_fid(fid_val);

	if (!vm_is_dir(fid)) {
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

		if ((rcount + vm_virtio_p9_dentry_size(dent)) > count) {
			/* seek to the previous offset and return */
			seekdir(fid->dir, old_offset);
			break;
		}
		old_offset = dent->d_off;
		if (vm_stat_rel(dent->d_name, &st) != 0)
			memset(&st, -1, sizeof(st));
		vm_stat2qid(&st, &qid);
		read = pdu->write_offset;
		vm_virtio_p9_pdu_writef(pdu, "Qqbs", &qid, dent->d_off,
				     dent->d_type, dent->d_name);
		rcount += pdu->write_offset - read;
		dent = readdir(fid->dir);
	}

	pdu->write_offset = VIRTIO_9P_HDR_LEN;
	vm_virtio_p9_pdu_writef(pdu, "d", rcount);
	*outlen = pdu->write_offset + rcount;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_statfs(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	u64 fsid;
	u32 fid_val;
	struct p9_fid *fid;
	struct statfs stat_buf;

	vm_virtio_p9_pdu_readf(pdu, "d", &fid_val);
	fid = vm_get_fid(fid_val);

	ret = statfs(fid->abs_path, &stat_buf);
	if (ret < 0)
		goto err_out;
	/* FIXME!! f_blocks needs update based on client msize */
	fsid = (unsigned int) stat_buf.f_fsid.__val[0] |
		(unsigned long long)stat_buf.f_fsid.__val[1] << 32;
	vm_virtio_p9_pdu_writef(pdu, "ddqqqqqqd", stat_buf.f_type,
			     stat_buf.f_bsize, stat_buf.f_blocks,
			     stat_buf.f_bfree, stat_buf.f_bavail,
			     stat_buf.f_files, stat_buf.f_ffree,
			     fsid, stat_buf.f_namelen);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
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

static void vm_virtio_p9_setattr(struct p9_pdu *pdu, u32 *outlen)
{
	int ret = 0;
	u32 fid_val;
	struct p9_fid *fid;
	struct p9_iattr_dotl p9attr;

	vm_virtio_p9_pdu_readf(pdu, "dI", &fid_val, &p9attr);
	fid = vm_get_fid(fid_val);

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
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_eopnotsupp(struct p9_pdu *pdu, u32 *outlen)
{
	return vm_virtio_p9_error_reply(pdu, EOPNOTSUPP, outlen);
}

static void vm_virtio_p9_mknod(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *name;
	struct stat st;
	struct p9_fid *dfid;
	struct p9_qid qid;
	char full_path[PATH_MAX];
	u32 fid_val, mode, major, minor, gid;

	vm_virtio_p9_pdu_readf(pdu, "dsdddd", &fid_val, &name, &mode,
			    &major, &minor, &gid);

	dfid = vm_get_fid(fid_val);

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

	vm_stat2qid(&st, &qid);
	vm_virtio_p9_pdu_writef(pdu, "Q", &qid);
	free(name);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(name);
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_lock(struct p9_pdu *pdu, u32 *outlen)
{
	u8 ret;
	u32 fid_val;
	struct p9_flock flock;

	vm_virtio_p9_pdu_readf(pdu, "dbdqqds", &fid_val, &flock.type,
			    &flock.flags, &flock.start, &flock.length,
			    &flock.proc_id, &flock.client_id);

	/* Just return success */
	ret = P9_LOCK_SUCCESS;
	vm_virtio_p9_pdu_writef(pdu, "d", ret);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	free(flock.client_id);
	return;
}

static void vm_virtio_p9_getlock(struct p9_pdu *pdu, u32 *outlen)
{
	u32 fid_val;
	struct p9_getlock glock;
	vm_virtio_p9_pdu_readf(pdu, "dbqqds", &fid_val, &glock.type,
			    &glock.start, &glock.length, &glock.proc_id,
			    &glock.client_id);

	/* Just return success */
	glock.type = F_UNLCK;
	vm_virtio_p9_pdu_writef(pdu, "bqqds", glock.type,
			     glock.start, glock.length, glock.proc_id,
			     glock.client_id);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	free(glock.client_id);
	return;
}

static void vm_virtio_p9_renameat(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *old_name, *new_name;
	u32 old_dfid_val, new_dfid_val;
	struct p9_fid *old_dfid, *new_dfid;
	char old_full_path[PATH_MAX], new_full_path[PATH_MAX];

	vm_virtio_p9_pdu_readf(pdu, "dsds", &old_dfid_val, &old_name,
			    &new_dfid_val, &new_name);

	old_dfid = vm_get_fid(old_dfid_val);
	new_dfid = vm_get_fid(new_dfid_val);

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
	vm_rename_fids(old_name, new_name);
	free(old_name);
	free(new_name);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(old_name);
	free(new_name);
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_readlink(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	u32 fid_val;
	struct p9_fid *fid;
	char target_path[PATH_MAX];

	vm_virtio_p9_pdu_readf(pdu, "d", &fid_val);
	fid = vm_get_fid(fid_val);

	memset(target_path, 0, PATH_MAX);
	ret = readlink(fid->abs_path, target_path, PATH_MAX - 1);
	if (ret < 0)
		goto err_out;

	vm_virtio_p9_pdu_writef(pdu, "s", target_path);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_unlinkat(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *name;
	u32 fid_val, flags;
	struct p9_fid *fid;
	char full_path[PATH_MAX];

	vm_virtio_p9_pdu_readf(pdu, "dsd", &fid_val, &name, &flags);
	fid = vm_get_fid(fid_val);

	if (get_full_path(full_path, sizeof(full_path), fid, name) != 0)
		goto err_out;

	ret = remove(full_path);
	if (ret < 0)
		goto err_out;
	free(name);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(name);
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_mkdir(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *name;
	struct stat st;
	struct p9_qid qid;
	struct p9_fid *dfid;
	char full_path[PATH_MAX];
	u32 dfid_val, mode, gid;

	vm_virtio_p9_pdu_readf(pdu, "dsdd", &dfid_val,
			    &name, &mode, &gid);
	dfid = vm_get_fid(dfid_val);

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

	vm_stat2qid(&st, &qid);
	vm_virtio_p9_pdu_writef(pdu, "Qd", &qid, 0);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	free(name);
	return;
err_out:
	free(name);
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_fsync(struct p9_pdu *pdu, u32 *outlen)
{
	int ret, fd;
	struct p9_fid *fid;
	u32 fid_val, datasync;

	vm_virtio_p9_pdu_readf(pdu, "dd", &fid_val, &datasync);
	fid = vm_get_fid(fid_val);

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
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_flush(struct p9_pdu *pdu, u32 *outlen)
{
	u16 tag, oldtag;

	vm_virtio_p9_pdu_readf(pdu, "ww", &tag, &oldtag);
	vm_virtio_p9_pdu_writef(pdu, "w", tag);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);

	return;
}

static void vm_virtio_p9_link(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	char *name;
	u32 fid_val, dfid_val;
	struct p9_fid *dfid, *fid;
	char full_path[PATH_MAX];

	vm_virtio_p9_pdu_readf(pdu, "dds", &dfid_val, &fid_val, &name);

	dfid = vm_get_fid(dfid_val);
	fid =  vm_get_fid(fid_val);

	if (get_full_path(full_path, sizeof(full_path), dfid, name) != 0)
		goto err_out;

	ret = link(fid->abs_path, full_path);
	if (ret < 0)
		goto err_out;
	free(name);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(name);
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_symlink(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	struct stat st;
	u32 fid_val, gid;
	struct p9_qid qid;
	struct p9_fid *dfid;
	char new_name[PATH_MAX];
	char *old_path, *name;

	vm_virtio_p9_pdu_readf(pdu, "dssd", &fid_val, &name, &old_path, &gid);

	dfid = vm_get_fid(fid_val);

	if (get_full_path(new_name, sizeof(new_name), dfid, name) != 0)
		goto err_out;

	ret = symlink(old_path, new_name);
	if (ret < 0)
		goto err_out;

	if (lstat(new_name, &st) < 0)
		goto err_out;

	vm_stat2qid(&st, &qid);
	vm_virtio_p9_pdu_writef(pdu, "Q", &qid);
	free(name);
	free(old_path);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	free(name);
	free(old_path);
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_create(struct p9_pdu *pdu, u32 *outlen)
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

	vm_virtio_p9_pdu_readf(pdu, "dsddd", &dfid_val,
			    &name, &flags, &mode, &gid);
	dfid = vm_get_fid(dfid_val);

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

	flags = vm_virtio_p9_openflags(flags);

	fd = open(full_path, flags | O_CREAT, mode);
	if (fd < 0)
		goto err_out;
	dfid->fd = fd;

	if (lstat(full_path, &st) < 0)
		goto err_out;

	ret = chmod(full_path, mode & 0777);
	if (ret < 0)
		goto err_out;

	vm_stat2qid(&st, &qid);
	vm_virtio_p9_pdu_writef(pdu, "Qd", &qid, 0);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	free(name);
	return;
err_out:
	free(name);
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_write(struct p9_pdu *pdu, u32 *outlen)
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

	vm_virtio_p9_pdu_readf(pdu, "dqd", &fid_val, &offset, &count);
	fid = vm_get_fid(fid_val);

	iov_base = pdu->out_iov[0].iov_base;
	iov_len  = pdu->out_iov[0].iov_len;
	iov_cnt  = pdu->out_iov_cnt;

	/* Adjust the iovec to skip the header and meta data */
	pdu->out_iov[0].iov_base += (sizeof(struct p9_msg) + twrite_size);
	pdu->out_iov[0].iov_len -=  (sizeof(struct p9_msg) + twrite_size);
	pdu->out_iov_cnt = vm_virtio_p9_update_iov_cnt(pdu->out_iov, count,
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
	vm_virtio_p9_pdu_writef(pdu, "d", res);
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;
err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_remove(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	u32 fid_val;
	struct p9_fid *fid;

	vm_virtio_p9_pdu_readf(pdu, "d", &fid_val);
	fid = vm_get_fid(fid_val);

	ret = remove(fid->abs_path);
	if (ret < 0)
		goto err_out;
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;

err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

static void vm_virtio_p9_rename(struct p9_pdu *pdu, u32 *outlen)
{
	int ret;
	u32 fid_val, new_fid_val;
	struct p9_fid *fid, *new_fid;
	char full_path[PATH_MAX], *new_name;

	vm_virtio_p9_pdu_readf(pdu, "dds", &fid_val, &new_fid_val, &new_name);
	fid = vm_get_fid(fid_val);
	new_fid = vm_get_fid(new_fid_val);

	if (get_full_path(full_path, sizeof(full_path), new_fid, new_name) != 0)
		goto err_out;

	ret = rename(fid->abs_path, full_path);
	if (ret < 0)
		goto err_out;
	*outlen = pdu->write_offset;
	vm_virtio_p9_set_reply_header(pdu, *outlen);
	return;

err_out:
	vm_virtio_p9_error_reply(pdu, errno, outlen);
	return;
}

/* FIXME should be removed when merging with latest linus tree */
#define P9_TRENAMEAT 74
#define P9_TUNLINKAT 76

typedef void p9_handler(struct p9_pdu *pdu, u32 *outlen);

static p9_handler *vm_virtio_9p_dotl_handler [] = {
	[P9_TREADDIR]     = vm_virtio_p9_readdir,
	[P9_TSTATFS]      = vm_virtio_p9_statfs,
	[P9_TGETATTR]     = vm_virtio_p9_getattr,
	[P9_TSETATTR]     = vm_virtio_p9_setattr,
	[P9_TXATTRWALK]   = vm_virtio_p9_eopnotsupp,
	[P9_TXATTRCREATE] = vm_virtio_p9_eopnotsupp,
	[P9_TMKNOD]       = vm_virtio_p9_mknod,
	[P9_TLOCK]        = vm_virtio_p9_lock,
	[P9_TGETLOCK]     = vm_virtio_p9_getlock,
	[P9_TRENAMEAT]    = vm_virtio_p9_renameat,
	[P9_TREADLINK]    = vm_virtio_p9_readlink,
	[P9_TUNLINKAT]    = vm_virtio_p9_unlinkat,
	[P9_TMKDIR]       = vm_virtio_p9_mkdir,
	[P9_TVERSION]     = vm_virtio_p9_version,
	[P9_TLOPEN]       = vm_virtio_p9_open,
	[P9_TATTACH]      = vm_virtio_p9_attach,
	[P9_TWALK]        = vm_virtio_p9_walk,
	[P9_TCLUNK]       = vm_virtio_p9_clunk,
	[P9_TFSYNC]       = vm_virtio_p9_fsync,
	[P9_TREAD]        = vm_virtio_p9_read,
	[P9_TFLUSH]       = vm_virtio_p9_flush,
	[P9_TLINK]        = vm_virtio_p9_link,
	[P9_TSYMLINK]     = vm_virtio_p9_symlink,
	[P9_TLCREATE]     = vm_virtio_p9_create,
	[P9_TWRITE]       = vm_virtio_p9_write,
	[P9_TREMOVE]      = vm_virtio_p9_remove,
	[P9_TRENAME]      = vm_virtio_p9_rename,
};
// =========================================================

static u8 vm_virtio_p9_get_cmd(struct p9_pdu *pdu)
{
	struct p9_msg *msg;
	/*
	 * we can peek directly into pdu for a u8
	 * value. The host endianess won't be an issue
	 */
	msg = pdu->out_iov[0].iov_base;
	return msg->cmd;
}

u32 run_p9_operation_in_vm(struct p9_pdu *p9pdu, char *root_dir)
{
	size_t res;
    u8 cmd;
	p9_handler *handler;
    u32 outlen = 0;

    // 1. copy contexts
    strncpy(vm_root_dir, root_dir, sizeof(vm_root_dir));
    LOG_DEBUG("root_dir: %s\n", vm_root_dir);

    // 2. read cmd
    cmd = vm_virtio_p9_get_cmd(p9pdu);

    // 3. handler
	if ((cmd >= ARRAY_SIZE(vm_virtio_9p_dotl_handler)) || !vm_virtio_9p_dotl_handler[cmd])
		handler = vm_virtio_p9_eopnotsupp;
	else
		handler = vm_virtio_9p_dotl_handler[cmd];

	handler(p9pdu, &outlen);
	LOG_DEBUG("cmd: %d - outlen: %d\n", cmd, outlen);
    return outlen;
}

bool translate_addr_space(struct p9_pdu *p9pdu)
{
	// 1. read first
	ssize_t res;
	int fd = open("/dev/rsi", O_RDWR);
	if (fd < 0) {
		LOG_ERROR("rsi open error\n");
		return false;
	}

	res = read(fd, vring_shared, sizeof(vring_shared));
	if (res != sizeof(vring_shared)) {
		LOG_ERROR("vring_shared read error\n");
		close(fd);
		return false;
	}
	close(fd);

	for (unsigned i=0; i<p9pdu->in_iov_cnt; i++) {
		unsigned long orig = (unsigned long)(p9pdu->in_iov[i].iov_base);
		unsigned long offset = orig - base_ipa_addr;
		unsigned long new_addr = (unsigned long)vring_shared + offset;

		if (offset >= VRING_SIZE) {
			LOG_ERROR("larger than VRING_SIZE!!!\n");
			exit(-1);
		}
		p9pdu->in_iov[i].iov_base = (void *)new_addr;
	}
	for (unsigned i=0; i<p9pdu->out_iov_cnt; i++) {
		unsigned long orig = (unsigned long)(p9pdu->out_iov[i].iov_base);
		unsigned long offset = orig - base_ipa_addr;
		unsigned long new_addr = (unsigned long)vring_shared + offset;

		if (offset >= VRING_SIZE) {
			LOG_ERROR("larger than VRING_SIZE!!!\n");
			exit(-1);
		}
		p9pdu->out_iov[i].iov_base = (void *)new_addr;
	}
	return true;
}

bool flush_result_to_shared(void)
{
	ssize_t res;
	int fd = open("/dev/rsi", O_RDWR);
	if (fd < 0) {
		LOG_ERROR("rsi open error\n");
		return false;
	}

	res = write(fd, vring_shared, sizeof(vring_shared));
	if (res != sizeof(vring_shared)) {
		LOG_ERROR("vring_shared write error\n");
		close(fd);
		return false;
	}
	close(fd);
	return true;
}

#if 0
bool map_vring_mirror(void)
{
	int fd = open("/dev/rsi", O_RDWR);
	if (fd < 0) {
		printf("rsi open error\n");
		return false;
	}

	vring_mirror = mmap(NULL, VRING_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (vring_mirror == MAP_FAILED) {
		printf("mmap error\n");
		close(fd);
		return false;
	}

/*
    vq_elem_mirror = mmap(NULL, VQ_ELEM_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (vq_elem_mirror == MAP_FAILED) {
        pritnf("mmap error, elem\n");
        close(fd);
        return false;
    } */

	close(fd);
	printf("mmap success: %02x, %02x\n", vring_mirror[0], vring_mirror[4 * 1024 * 1024]);
    //printf("mmap_elem success\n");
	return true;
}
#endif

int main(int argc, char **argv)
{
	FILE *fp;
	struct p9_pdu p9pdu;
	size_t res;
	u32 outlen = 0;

    LOG_ERROR("[JB] main loop start...\n");
	while (true) {
		memset(&p9pdu, 0, sizeof(p9pdu));

		// 1. read request
		fp = fopen("/shared/p9req.bin", "rb");
		if (fp == NULL) {
			sleep(1);
			//printf("fopen error!\n");
			continue;
		}
		res = fread((unsigned char *)&p9pdu, sizeof(unsigned char), sizeof(struct p9_pdu), fp);
		(void)res;
		fclose(fp);
		LOG_DEBUG("[JB] p9req.bin read success\n");

		// translate addr space
		if (translate_addr_space(&p9pdu) == false) {
			LOG_ERROR("translate_addr_space error\n");
			exit(1);
		}
		LOG_DEBUG("[JB] translate_addr_space success\n");

		// 2. handle this request
		outlen = run_p9_operation_in_vm(&p9pdu, "/shared");
		LOG_DEBUG("[JB] run_p9_operation_in_vm success\n");
		
		if (flush_result_to_shared() == false) {
			LOG_ERROR("flush_result_to_shared error\n");
		} else {
			LOG_DEBUG("flush_result_to_shared success\n");
		}

		// 3. write resp
		fp = fopen("/shared/p9resp.bin", "wb");
		if (fp == NULL) {
			LOG_ERROR("[JB] p9resp.bin open error\n");
			continue;	
		} else {
			LOG_DEBUG("[JB] run_p9_operation_in_vm success\n");
            fwrite(&outlen, sizeof(outlen), 1, fp);
			fclose(fp);
		}

		// 4. remove req
		if (remove("/shared/p9req.bin") == 0) {
			LOG_DEBUG("[JB] remove p9req.bin success\n");
		} else {
			LOG_ERROR("[JB] remove p9req.bin error\n");
		}
	}

	return 0;
}
