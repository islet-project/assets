#include "kvm/disk-image.h"
#include "kvm/qcow.h"
#include "kvm/virtio-blk.h"
#include "kvm/kvm.h"
#include "kvm/iovec.h"

#include <linux/err.h>
#include <poll.h>

int debug_iodelay;

static int disk_image__close(struct disk_image *disk);

int disk_img_name_parser(const struct option *opt, const char *arg, int unset)
{
	const char *cur;
	char *sep;
	struct kvm *kvm = opt->ptr;

	if (kvm->nr_disks >= MAX_DISK_IMAGES)
		die("Currently only 4 images are supported");

	kvm->cfg.disk_image[kvm->nr_disks].filename = arg;
	cur = arg;

	if (strncmp(arg, "scsi:", 5) == 0) {
		sep = strstr(arg, ":");
		if (sep)
			kvm->cfg.disk_image[kvm->nr_disks].wwpn = sep + 1;
		sep = strstr(sep + 1, ":");
		if (sep) {
			*sep = 0;
			kvm->cfg.disk_image[kvm->nr_disks].tpgt = sep + 1;
		}
		cur = sep + 1;
	}

	do {
		sep = strstr(cur, ",");
		if (sep) {
			if (strncmp(sep + 1, "ro", 2) == 0)
				kvm->cfg.disk_image[kvm->nr_disks].readonly = true;
			else if (strncmp(sep + 1, "direct", 6) == 0)
				kvm->cfg.disk_image[kvm->nr_disks].direct = true;
			*sep = 0;
			cur = sep + 1;
		}
	} while (sep);

	kvm->nr_disks++;

	return 0;
}

struct disk_image *disk_image__new(int fd, u64 size,
				   struct disk_image_operations *ops,
				   int use_mmap)
{
	struct disk_image *disk;
	int r;

	disk = malloc(sizeof *disk);
	if (!disk)
		return ERR_PTR(-ENOMEM);

	*disk = (struct disk_image) {
		.fd	= fd,
		.size	= size,
		.ops	= ops,
	};

	if (use_mmap == DISK_IMAGE_MMAP) {
		/*
		 * The write to disk image will be discarded
		 */
		disk->priv = mmap(NULL, size, PROT_RW, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
		if (disk->priv == MAP_FAILED) {
			r = -errno;
			goto err_free_disk;
		}
	}

	r = disk_aio_setup(disk);
	if (r)
		goto err_unmap_disk;

	return disk;

err_unmap_disk:
	if (disk->priv)
		munmap(disk->priv, size);
err_free_disk:
	free(disk);
	return ERR_PTR(r);
}

static struct disk_image *disk_image__open(const char *filename, bool readonly, bool direct)
{
	struct disk_image *disk;
	struct stat st;
	int fd, flags;

	if (readonly)
		flags = O_RDONLY;
	else
		flags = O_RDWR;
	if (direct)
		flags |= O_DIRECT;

	if (stat(filename, &st) < 0)
		return ERR_PTR(-errno);

	/* blk device ?*/
	disk = blkdev__probe(filename, flags, &st);
	if (!IS_ERR_OR_NULL(disk)) {
		disk->readonly = readonly;
		return disk;
	}

	fd = open(filename, flags);
	if (fd < 0)
		return ERR_PTR(fd);

	/* qcow image ?*/
	disk = qcow_probe(fd, true);
	if (!IS_ERR_OR_NULL(disk)) {
		pr_warning("Forcing read-only support for QCOW");
		disk->readonly = true;
		return disk;
	}

	/* raw image ?*/
	disk = raw_image__probe(fd, &st, readonly);
	if (!IS_ERR_OR_NULL(disk)) {
		disk->readonly = readonly;
		return disk;
	}

	if (close(fd) < 0)
		pr_warning("close() failed");

	return ERR_PTR(-ENOSYS);
}

static struct disk_image **disk_image__open_all(struct kvm *kvm)
{
	struct disk_image **disks;
	const char *filename;
	const char *wwpn;
	const char *tpgt;
	bool readonly;
	bool direct;
	void *err;
	int i;
	struct disk_image_params *params = (struct disk_image_params *)&kvm->cfg.disk_image;
	int count = kvm->nr_disks;

	if (!count)
		return ERR_PTR(-EINVAL);
	if (count > MAX_DISK_IMAGES)
		return ERR_PTR(-ENOSPC);

	disks = calloc(count, sizeof(*disks));
	if (!disks)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < count; i++) {
		filename = params[i].filename;
		readonly = params[i].readonly;
		direct = params[i].direct;
		wwpn = params[i].wwpn;
		tpgt = params[i].tpgt;

		if (wwpn) {
			disks[i] = malloc(sizeof(struct disk_image));
			if (!disks[i])
				return ERR_PTR(-ENOMEM);
			disks[i]->wwpn = wwpn;
			disks[i]->tpgt = tpgt;
			continue;
		}

		if (!filename)
			continue;

		disks[i] = disk_image__open(filename, readonly, direct);
		if (IS_ERR_OR_NULL(disks[i])) {
			pr_err("Loading disk image '%s' failed", filename);
			err = disks[i];
			goto error;
		}
		disks[i]->debug_iodelay = kvm->cfg.debug_iodelay;
	}

	return disks;
error:
	for (i = 0; i < count; i++)
		if (!IS_ERR_OR_NULL(disks[i]))
			disk_image__close(disks[i]);

	free(disks);
	return err;
}

int disk_image__wait(struct disk_image *disk)
{
	if (disk->ops->wait)
		return disk->ops->wait(disk);

	return 0;
}

int disk_image__flush(struct disk_image *disk)
{
	if (disk->ops->flush)
		return disk->ops->flush(disk);

	return fsync(disk->fd);
}

static int disk_image__close(struct disk_image *disk)
{
	/* If there was no disk image then there's nothing to do: */
	if (!disk)
		return 0;

	disk_aio_destroy(disk);

	if (disk->ops->close)
		return disk->ops->close(disk);

	if (close(disk->fd) < 0)
		pr_warning("close() failed");

	free(disk);

	return 0;
}

static int disk_image__close_all(struct disk_image **disks, int count)
{
	while (count)
		disk_image__close(disks[--count]);

	free(disks);

	return 0;
}

/*
 * Fill iov with disk data, starting from sector 'sector'.
 * Return amount of bytes read.
 */
ssize_t disk_image__read(struct disk_image *disk, u64 sector,
			 const struct iovec *iov, int iovcount, void *param)
{
	ssize_t total = 0;

	if (debug_iodelay)
		msleep(debug_iodelay);

	if (disk->ops->read) {
		total = disk->ops->read(disk, sector, iov, iovcount, param);
		if (total < 0) {
			pr_info("disk_image__read error: total=%ld\n", (long)total);
			return total;
		}
	}

	if (!disk->async && disk->disk_req_cb) {
		disk->disk_req_cb(param, total);
    }

	return total;
}

/*
 * Write iov to disk, starting from sector 'sector'.
 * Return amount of bytes written.
 */
ssize_t disk_image__write(struct disk_image *disk, u64 sector,
			  const struct iovec *iov, int iovcount, void *param)
{
	ssize_t total = 0;

	if (debug_iodelay)
		msleep(debug_iodelay);

	if (disk->ops->write) {
		/*
		 * Try writev based operation first
		 */

		total = disk->ops->write(disk, sector, iov, iovcount, param);
		if (total < 0) {
			pr_info("disk_image__write error: total=%ld\n", (long)total);
			return total;
		}
	} else {
		/* Do nothing */
	}

	if (!disk->async && disk->disk_req_cb)
		disk->disk_req_cb(param, total);

	return total;
}

ssize_t disk_image__get_serial(struct disk_image *disk, struct iovec *iov,
			       int iovcount, ssize_t len)
{
	struct stat st;
	void *buf;
	int r;

	r = fstat(disk->fd, &st);
	if (r)
		return r;

	buf = malloc(len);
	if (!buf)
		return -ENOMEM;

	len = snprintf(buf, len, "%llu%llu%llu",
		       (unsigned long long)st.st_dev,
		       (unsigned long long)st.st_rdev,
		       (unsigned long long)st.st_ino);
	if (len < 0 || (size_t)len > iov_size(iov, iovcount)) {
		free(buf);
		return -ENOMEM;
	}

	memcpy_toiovec(iov, buf, len);
	free(buf);
	return len;
}

void disk_image__set_callback(struct disk_image *disk,
			      void (*disk_req_cb)(void *param, long len))
{
	disk->disk_req_cb = disk_req_cb;
}

int disk_image__init(struct kvm *kvm)
{
	if (kvm->nr_disks) {
		kvm->disks = disk_image__open_all(kvm);
		if (IS_ERR(kvm->disks))
			return PTR_ERR(kvm->disks);
	}

	return 0;
}
dev_base_init(disk_image__init);

int disk_image__exit(struct kvm *kvm)
{
	return disk_image__close_all(kvm->disks, kvm->nr_disks);
}
dev_base_exit(disk_image__exit);
