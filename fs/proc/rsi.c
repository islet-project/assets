#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <asm/rsi.h>
#include <asm/rsi_cmds.h>
#include <asm/uaccess.h>
#include <linux/cc_platform.h>

#include "rsi.h"

#define CLOAK_READ_P9_PDU (99988)
#define CLOAK_WAIT_P9_PDU (99987)

extern unsigned long cloak_virtio_start;

//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Havner");
//MODULE_DESCRIPTION("Linux RSI playground");


#define RSI_TAG   "rsi: "
#define RSI_INFO  KERN_INFO  RSI_TAG
#define RSI_ALERT KERN_ALERT RSI_TAG

#define DEVICE_NAME       "rsi"       /* Name of device in /proc/devices */

static int device_major;              /* Major number assigned to our device driver */
//static int device_open_count = 0;     /* Used to prevent multiple open */
static struct class *cls;

/* RSI attestation call consists of several arm_smc calls,
 * don't let several users interrupt eachother.
 */
static DEFINE_MUTEX(attestation_call);

/*
#define VIRTQUEUE_NUM 128
struct p9_pdu_cloak {
    unsigned long         out_iov_cnt;
    unsigned long         in_iov_cnt;
    struct iovec in_iov[VIRTQUEUE_NUM];
    struct iovec out_iov[VIRTQUEUE_NUM];
}; */

char __attribute__((aligned(PAGE_SIZE))) cloak_vq_desc_mem[2 * 1024 * 1024] = {0,}; // 2mb
EXPORT_SYMBOL(cloak_vq_desc_mem);

static void rsi_playground(void)
{
	unsigned long ret = 0;
	bool realm = false;
	unsigned long ver = 0;

	// creative use of an API
	realm = cc_platform_has(CC_ATTR_MEM_ENCRYPT);
	printk(RSI_INFO "Is realm: %s\n", realm ? "true" : "false");

	// version
	ver = rsi_get_version();
	printk(RSI_INFO "RSI version: %lu.%lu\n",
	       RSI_ABI_VERSION_GET_MAJOR(ver), RSI_ABI_VERSION_GET_MINOR(ver));

	// get config
	ret = rsi_get_realm_config(&config);
	printk(RSI_INFO "Config ret: %lu, Bits: %lX\n", ret, config.ipa_bits);
}

#if 0
#define BYTE_STRING_LEN 4
static void print_data(uint8_t *data, size_t len)
{
	size_t i;
	char ch[BYTE_STRING_LEN], line[32] = {0};

	for (i = 0; i < len; ++i) {
		if (i > 0 && i % 8 == 0) {
			printk(RSI_INFO "%s\n", line);
			line[0] = '\0';
		}
		snprintf(ch, BYTE_STRING_LEN, "%.2X ", data[i]);
		strncat(line, ch, BYTE_STRING_LEN);
	}

	if (line[0] != '\0')
		printk(RSI_INFO "%s\n", line);
}
#endif

static int rsi_ret_to_errno(unsigned long rsi_ret)
{
	switch (rsi_ret) {
	case RSI_SUCCESS:
		return 0;
	case RSI_ERROR_INPUT:
		return EFAULT;
	case RSI_ERROR_STATE:
		return EBADF;
	case RSI_INCOMPLETE:
		return 0;
	default:
		printk(RSI_ALERT "unknown ret code returned from RSI: %lu\n", rsi_ret);
		return ENXIO;
	}
}

/*
 * Chardev
 */

static int device_open(struct inode *i, struct file *f)
{
	//printk(RSI_INFO "device %s open\n", DEVICE_NAME);

/*
	if (device_open_count > 0)
		return -EBUSY;

	++device_open_count;

	if (!try_module_get(THIS_MODULE))
		return -ENOENT; */
	
	return 0;
}

static int device_release(struct inode *i, struct file *f)
{
	//printk(RSI_INFO "device %s released\n", DEVICE_NAME);

/*
	module_put(THIS_MODULE);
	--device_open_count; */

	return 0;
}

static int do_measurement_read(struct rsi_measurement *measur)
{
	struct arm_smccc_1_2_regs input = {0}, output = {0};

	input.a0 = SMC_RSI_MEASUREMENT_READ;
	input.a1 = measur->index;
	arm_smccc_1_2_smc(&input, &output);

	if (output.a0 != RSI_SUCCESS)
		return -rsi_ret_to_errno(output.a0);

	measur->data_len = sizeof(output.a1) * 8;
	memcpy(measur->data, (uint8_t*)&output.a1, measur->data_len);

	return 0;
}

static int do_measurement_extend(struct rsi_measurement *measur)
{
	struct arm_smccc_1_2_regs input = {0}, output = {0};

	if (measur->data_len == 0 || measur->data_len > 64) {
		printk(RSI_ALERT "measurement_extend: must be in 1-64 bytes range\n");
		return -EINVAL;
	}

	input.a0 = SMC_RSI_MEASUREMENT_EXTEND;
	input.a1 = measur->index;
	input.a2 = measur->data_len;
	memcpy((uint8_t*)&input.a3, measur->data, measur->data_len);

	arm_smccc_1_2_smc(&input, &output);

	if (output.a0 != RSI_SUCCESS)
		return -rsi_ret_to_errno(output.a0);

	return 0;
}

static int do_attestation_init(phys_addr_t page, struct rsi_attestation *attest)
{
	struct arm_smccc_1_2_regs input = {0}, output = {0};

	input.a0 = SMC_RSI_ATTESTATION_TOKEN_INIT;
	input.a1 = page;

	// Copy challenge to registers (a2-a9)
	memcpy((uint8_t*)&input.a2, attest->challenge, sizeof(attest->challenge));

	arm_smccc_1_2_smc(&input, &output);

	// TODO: which is correct?
	if (output.a0 == RSI_INCOMPLETE || output.a0 == RSI_SUCCESS)
		return 0;
	else
		return -rsi_ret_to_errno(output.a0);
}

static int do_attestation_continue(phys_addr_t page, struct rsi_attestation *attest)
{
	struct arm_smccc_1_2_regs input = {0}, output = {0};

	input.a0 = SMC_RSI_ATTESTATION_TOKEN_CONTINUE;
	input.a1 = page;

	arm_smccc_1_2_smc(&input, &output);

	if (output.a0 == RSI_SUCCESS) {
		attest->token_len = output.a1;
		return 0;  // we're done
	}

	if (output.a0 == RSI_INCOMPLETE)
		return 1;  // carry on

	return -rsi_ret_to_errno(output.a0);
}

static int do_attestation(struct rsi_attestation *attest)
{
	int ret;
	phys_addr_t page = virt_to_phys(rsi_page_buf);

	mutex_lock(&attestation_call);

	ret = do_attestation_init(page, attest);
	if (ret != 0)
		goto unlock;

	do {
		ret = do_attestation_continue(page, attest);
	} while (ret == 1);

unlock:
	mutex_unlock(&attestation_call);

	if (ret == 0)
		memcpy(attest->token, rsi_page_buf, attest->token_len);

	return ret;
}

static int do_cloak_create(struct rsi_cloak *cloak, unsigned long size)
{
	phys_addr_t page; // virt_to_phys(cloak_virtio_mem);
	struct arm_smccc_1_2_regs input = {0}, output = {0};

    if (size == (16 * 1024 * 1024)) {
        page = virt_to_phys(cloak_virtio_mem);
    }
    else if (size == (2 * 1024 * 1024)) {
        page = virt_to_phys(cloak_vq_desc_mem);
    }
    else {
        page = virt_to_phys(cloak_virtio_mem);
    }

	input.a0 = SMC_RSI_CHANNEL_CREATE;
	input.a1 = cloak->id;
	input.a2 = page;
    input.a3 = size;

	arm_smccc_1_2_smc(&input, &output);
    if (output.a0 == RSI_SUCCESS)
        return 0;
    else
        return -rsi_ret_to_errno(output.a0);
}

static int do_cloak_connect(struct rsi_cloak *cloak)
{
	//phys_addr_t page = virt_to_phys(rsi_page_connector);
    // TODO: we need to support MMAP for shared memory region
	phys_addr_t page = virt_to_phys(rsi_page_creator);
	struct arm_smccc_1_2_regs input = {0}, output = {0};

	input.a0 = SMC_RSI_CHANNEL_CONNECT;
	input.a1 = cloak->id;
	input.a2 = page;

	arm_smccc_1_2_smc(&input, &output);
    if (output.a0 == RSI_SUCCESS)
        return 0;
    else
        return -rsi_ret_to_errno(output.a0);
}

static int do_cloak_gen_report(struct rsi_cloak *cloak)
{
	int ret;
	struct arm_smccc_1_2_regs input = {0}, output = {0};
	phys_addr_t page = virt_to_phys(rsi_page_buf);

	mutex_lock(&attestation_call);

	pr_info("before gen_report smc\n");

	input.a0 = SMC_RSI_CHANNEL_GEN_REPORT;
	input.a1 = cloak->id;
	input.a2 = page;

	// do attest_init & attest_continue at once
	arm_smccc_1_2_smc(&input, &output);

	pr_info("after gen_report smc\n");

	if (output.a0 == RSI_SUCCESS) {
		cloak->token_len = output.a1;
		ret = 0;
	} else {
		ret = -rsi_ret_to_errno(output.a0);
	}

	mutex_unlock(&attestation_call);
	if (ret == 0) {
        unsigned i;
        pr_info("[JB] cloak_gen_report: token_len: %d\n", (int)cloak->token_len);
        for (i=0; i<8; i++) {
            pr_info("[JB] att_report[%d]: %02x\n", i, rsi_page_buf[i]);
        }
        memcpy(cloak->token, rsi_page_buf, cloak->token_len);
    }

	pr_info("after gen_report end: %d\n", ret);
	return ret;
}

static int do_cloak_result(struct rsi_cloak *cloak)
{
	struct arm_smccc_1_2_regs input = {0}, output = {0};

	input.a0 = SMC_RSI_CHANNEL_RESULT;
	input.a1 = cloak->id;

	arm_smccc_1_2_smc(&input, &output);
    if (output.a0 == RSI_SUCCESS) {
        cloak->result = output.a1;
        return 0;
    }
    else
        return -rsi_ret_to_errno(output.a0);
}

struct host_call_arg {
	u16 imm;
	unsigned long gprs[7];
};
struct host_call_arg host_call_mem __attribute__((aligned(PAGE_SIZE)));
#define CLOAK_HOST_CALL (799)

static int do_cloak_host_call(unsigned long outlen)
{
	struct arm_smccc_1_2_regs input = {0}, output = {0};

	memset(&host_call_mem, 0, sizeof(host_call_mem));
	host_call_mem.imm = CLOAK_HOST_CALL;
	host_call_mem.gprs[0] = outlen;

	input.a0 = SMC_RSI_HOST_CALL;
	input.a1 = virt_to_phys(&host_call_mem);  // IPA of HostCall

	arm_smccc_1_2_smc(&input, &output);
    return 0;  // do not use return value as of now
/*
    if (output.a0 == RSI_SUCCESS) {
        return 0;
    }
    else
        return -rsi_ret_to_errno(output.a0); */
}

static long device_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	uint32_t version = 0;
	struct rsi_measurement *measur = NULL;
	struct rsi_attestation *attest = NULL;
	struct rsi_cloak cloak;
	//phys_addr_t page;

    //pr_info("[Cloak] device_ioctl: %d\n", cmd);

	switch (cmd) {
	case CLOAK_WAIT_P9_PDU:
		//pr_info("[Cloak] CLOAK_WAIT_P9_PDU\n");
		ret = do_cloak_host_call(arg);
		//pr_info("[Cloak] after host_call!\n");
		return ret;
	case CLOAK_READ_P9_PDU:
		//pr_info("[Cloak] CLOAK_READ_P9_PDU\n");
		ret = copy_to_user((struct p9_pdu_cloak *)arg, (struct p9_pdu_cloak *)cloak_vq_desc_mem, sizeof(struct p9_pdu_cloak));
		if (ret != 0) {
			pr_info("[Cloak] ioctl: copy_to_user error: %d\n", ret);
		}
		return ret;
	case RSIIO_ABI_VERSION:
		printk(RSI_INFO "ioctl: abi_version\n");

		version = (uint32_t)rsi_get_version();
		ret = copy_to_user((uint32_t*)arg, &version, sizeof(uint32_t));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_to_user failed: %d\n", ret);
			return ret;
		}

		break;
	case RSIIO_MEASUREMENT_READ:
		measur = kmalloc(sizeof(struct rsi_measurement), GFP_KERNEL);
		if (measur == NULL) {
			printk("ioctl: failed to allocate");
			return -ENOMEM;
		}

		ret = copy_from_user(measur, (struct rsi_measurement*)arg, sizeof(struct rsi_measurement));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_from_user failed: %d\n", ret);
			goto end;
		}

		printk(RSI_INFO "ioctl: measurement_read: %u\n", measur->index);

		ret = do_measurement_read(measur);
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: measurement_read failed: %d\n", ret);
			goto end;
		}

		ret = copy_to_user((struct rsi_measurement*)arg, measur, sizeof(struct rsi_measurement));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_to_user failed: %d\n", ret);
			goto end;
		}

		break;
	case RSIIO_MEASUREMENT_EXTEND:
		measur = kmalloc(sizeof(struct rsi_measurement), GFP_KERNEL);
		if (measur == NULL) {
			printk("ioctl: failed to allocate");
			return -ENOMEM;
		}

		ret = copy_from_user(measur, (struct rsi_measurement*)arg, sizeof(struct rsi_measurement));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_from_user failed: %d\n", ret);
			goto end;
		}

		printk(RSI_INFO "ioctl: measurement_extend: %u, %u\n", measur->index, measur->data_len);

		ret = do_measurement_extend(measur);
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: measurement_extend failed: %d\n", ret);
			goto end;
		}

		break;
	case RSIIO_ATTESTATION_TOKEN:
		attest = kmalloc(sizeof(struct rsi_attestation), GFP_KERNEL);
		if (attest == NULL) {
			printk("ioctl: failed to allocate");
			return -ENOMEM;
		}

		ret = copy_from_user(attest, (struct rsi_attestation*)arg, sizeof(struct rsi_attestation));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_from_user failed: %d\n", ret);
			goto end;
		}

		printk(RSI_INFO "ioctl: attestation_token");

		ret = do_attestation(attest);
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: attestation failed: %d\n", ret);
			goto end;
		}

		ret = copy_to_user((struct rsi_attestation*)arg, attest, sizeof(struct rsi_attestation));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_to_user failed: %d\n", ret);
			goto end;
		}

		break;
	case RSIIO_CHANNEL_CREATE:
		ret = copy_from_user(&cloak, (struct rsi_cloak*)arg, sizeof(struct rsi_cloak));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_from_user failed: %d\n", ret);
			goto end;
		}

		ret = do_cloak_create(&cloak, 4096);
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: do_cloak_create failed: %d\n", ret);
			goto end;
		}

		ret = copy_to_user((struct rsi_cloak*)arg, &cloak, sizeof(struct rsi_cloak));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_to_user failed: %d\n", ret);
			goto end;
		}
		break;
	case RSIIO_CHANNEL_CONNECT:
		printk(RSI_INFO "ioctl: RSIIO_CHANNEL_CONNECT\n");

		ret = copy_from_user(&cloak, (struct rsi_cloak*)arg, sizeof(struct rsi_cloak));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_from_user failed: %d\n", ret);
			goto end;
		}
		printk(RSI_INFO "ioctl: after copy_from_user\n");

		ret = do_cloak_connect(&cloak);
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: do_cloak_connect failed: %d\n", ret);
			goto end;
		}
		printk(RSI_INFO "ioctl: after do_cloak_connect\n");

		ret = copy_to_user((struct rsi_cloak*)arg, &cloak, sizeof(struct rsi_cloak));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_to_user failed: %d\n", ret);
			goto end;
		}

		printk(RSI_ALERT "RSIIO_CHANNEL_CONNECT success!\n");
		break;
	case RSIIO_CHANNEL_GEN_REPORT:
		ret = copy_from_user(&cloak, (struct rsi_cloak*)arg, sizeof(struct rsi_cloak));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_from_user failed: %d\n", ret);
			goto end;
		}

		ret = do_cloak_gen_report(&cloak);
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: do_cloak_gen_report failed: %d\n", ret);
			goto end;
		}

		ret = copy_to_user((struct rsi_cloak*)arg, &cloak, sizeof(struct rsi_cloak));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_to_user failed: %d\n", ret);
			goto end;
		}
		break;
	case RSIIO_CHANNEL_RESULT:
		ret = copy_from_user(&cloak, (struct rsi_cloak*)arg, sizeof(struct rsi_cloak));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_from_user failed: %d\n", ret);
			goto end;
		}

		ret = do_cloak_result(&cloak);
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: do_cloak_result failed: %d\n", ret);
			goto end;
		}

		ret = copy_to_user((struct rsi_cloak*)arg, &cloak, sizeof(struct rsi_cloak));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: copy_to_user failed: %d\n", ret);
			goto end;
		}
		break;
	case RSIIO_CHANNEL_WRITE:
		ret = copy_from_user(&cloak, (struct rsi_cloak*)arg, sizeof(struct rsi_cloak));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: RSIIO_CHANNEL_WRITE: copy_from_user failed: %d\n", ret);
			goto end;
		}
		memcpy(rsi_page_creator, cloak.token, sizeof(cloak.token));
		break;
	case RSIIO_CHANNEL_READ:
		ret = copy_to_user(((struct rsi_cloak*)arg)->token, rsi_page_creator, sizeof(rsi_page_creator));
		if (ret != 0) {
			printk(RSI_ALERT "ioctl: RSIIO_CHANNEL_READ: copy_to_user failed: %d\n", ret);
			goto end;
		}
		break;
	default:
		printk(RSI_ALERT "ioctl: unknown ioctl cmd\n");
		return -EINVAL;
	}

	ret = 0;

end:
	if (attest)
		kfree(attest);
	if (measur)
		kfree(measur);

	return ret;
}

extern unsigned long no_shared_region_flag;
extern unsigned long cloak_virtio_start;
static int cloak_mmap(struct file *filp, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	u64 start, end;
	u64 pfn;

	start = (u64)vma->vm_pgoff << PAGE_SHIFT;
	end = start + size;

	if (size > (16 * 1024 * 1024)) {
		return -EINVAL;
	}

    if (no_shared_region_flag) {
        pfn = cloak_virtio_start >> PAGE_SHIFT;
        if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
            return -EINVAL;
        }
        
        // [test]
        return 0;
    }

    if (size == (16 * 1024 * 1024)) {
		pfn = virt_to_phys(cloak_virtio_mem) >> PAGE_SHIFT;
		if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
			return -EINVAL;
		}	

		pr_info("[JB] cloak remap_pfn_range success! cloak_virtio_mem\n");
		return 0;
    }
	else if (size == (2 * 1024 * 1024)) {
		pfn = virt_to_phys(cloak_vq_desc_mem) >> PAGE_SHIFT;
		if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
			return -EINVAL;
		}	

		pr_info("[JB] cloak remap_pfn_range success! cloak_vq_desc_mem\n");
		return 0;
	}
	else {
		return -EINVAL;
	}
}

static ssize_t cloak_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	if (count == (16 * 1024 * 1024)) {
		char *mem = NULL;
		if (no_shared_region_flag) {
			mem = (char *)phys_to_virt(cloak_virtio_start);
		} else {
			mem = cloak_virtio_mem;
		}

		if (copy_from_user(mem, buf, count)) {
			pr_info("[JB] cloak_write: copy_from_user() error");
			return 0;
		}
		return count;
	}
	return 0;
}

static ssize_t cloak_read(struct file *filp, char __user *buf, size_t siz, loff_t *ppos)
{
	if (siz == (16 * 1024 * 1024)) {
		char *mem = NULL;
		if (no_shared_region_flag) {
			mem = (char *)phys_to_virt(cloak_virtio_start);
		} else {
			mem = cloak_virtio_mem;
		}

		if (copy_to_user(buf, mem, siz)) {
			pr_info("[JB] cloak_read: copy_to_user() error");
			return 0;
		}
		return siz;
	}
	return 0;
}

static struct file_operations chardev_fops = {
	.open = device_open,
	.release = device_release,
	.unlocked_ioctl = device_ioctl,
	.mmap = cloak_mmap,
	.write = cloak_write,
	.read = cloak_read,
};

/*
 * Module
 */

unsigned long no_shared_region_flag = 0;
unsigned long cloak_single_test_flag = 0;

static unsigned long jb_align_to_2mb(unsigned long value) {
    unsigned long align_min = 2 * 1024 * 1024;
    unsigned long remainder = value % align_min;
    unsigned long aligned_value = value + (align_min - remainder);
    return aligned_value;
}

static int __init rsi_init(void)
{
    struct rsi_cloak cl;

	printk(RSI_INFO "Initializing\n");

	device_major = register_chrdev(0, DEVICE_NAME, &chardev_fops);
	if (device_major < 0) {
		printk(RSI_ALERT "register_chrdev failed with %d\n", device_major);
		return device_major;
	}

	printk(RSI_INFO "Chardev registered with major %d\n", device_major);

	cls = class_create(THIS_MODULE, DEVICE_NAME);
	device_create(cls, NULL, MKDEV(device_major, 0), NULL, DEVICE_NAME);

	printk(RSI_INFO "Device created on /dev/%s\n", DEVICE_NAME);

	rsi_playground();

    // shared_mem creation
    if (no_shared_region_flag == 0) {
		if (cloak_single_test_flag == 1) {
			cloak_virtio_mem = (char *)phys_to_virt(cloak_virtio_start);
		} else {
			// addr adjustment
			cloak_virtio_mem = (char *)rsi_page_creator;
			cloak_virtio_mem = (char *)jb_align_to_2mb((unsigned long)cloak_virtio_mem);

            // for vq data
			cl.id = 0;
			if (do_cloak_create(&cl, 16 * 1024 * 1024) != 0) {
				pr_info("[JB] do_cloak_create fail for vq data\n");
			} else {
				cloak_virtio_mem[0] = 0x8;
				cloak_virtio_mem[4 * 1024 * 1024] = 0x12;
				pr_info("[JB] do_cloak_create success for vq data: %lx - %lx\n", cloak_virtio_mem, virt_to_phys(cloak_virtio_mem));
			}

            // for vq control
            cl.id = 1;
            if (do_cloak_create(&cl, 2 * 1024 * 1024) != 0) {
                pr_info("[JB] do_cloak_create fail for vq control\n");
            } else {
                pr_info("[JB] do_cloak_create success for vq control\n");
            }
		}
        /*
        cl.id = 1;
        if (do_cloak_create(&cl, sizeof(cloak_vq_elem)) != 0) {
            pr_info("[JB] do_cloak_create fail, cloak_vq_elem\n");
        } else {
            pr_info("[JB] do_cloak_create success, cloak_vq_elem\n");
        } */
    } else {
        unsigned long res;
        unsigned long pa = virt_to_phys(cloak_vq_desc_mem);

        res = rsi_cloak_channel_connect_with_size(1, pa, 2 * 1024 * 1024);
        pr_info("[JB] rsi_cloak_channel_connect_with_size for vq control: %d\n", res);
    }

	return 0;
}

static int __init no_shared_region_param(char *arg)
{
    no_shared_region_flag = 1;
    pr_info("[JB] no_shared_region 1 !!!!\n");
	return 0;
}
early_param("no_shared_region", no_shared_region_param);

static int __init cloak_single_test_param(char *arg)
{
    cloak_single_test_flag = 1;
    pr_info("[JB] cloak_single_test_flag 1 !!!!\n");
	return 0;
}
early_param("cloak_single_test", cloak_single_test_param);

/*
static void __exit rsi_cleanup(void)
{
	printk(RSI_INFO "Cleaning up module\n");

	device_destroy(cls, MKDEV(device_major, 0));
	class_destroy(cls);

	unregister_chrdev(device_major, DEVICE_NAME);
} */

fs_initcall(rsi_init);

//module_init(rsi_init);
//module_exit(rsi_cleanup);
