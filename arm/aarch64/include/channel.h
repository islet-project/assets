#ifndef ARM_AARCH64__CHANNEL_H
#define ARM_AARCH64__CHANNEL_H

#include <kvm/pci.h>
#include <kvm/devices.h>
#include <syslog.h>
#include <stdarg.h>

/*
 * The device id should be included in the following range to avoid conflict with other device ids:
 * 
 * 1af4:10f0 to 1a4f:10ff
 * Available for experimental usage without registration.
 * Must get official ID when the code leaves the test lab
 * (i.e. when seeking upstream merge or shipping a distro/product) to avoid conflicts.
 * 
 * Referenced by https://github.com/qemu/qemu/blob/master/docs/specs/pci-ids.rst
 */
#define VCHANNEL_PCI_DEVICE_ID  0x10f0
#define VCHANNEL_PCI_CLASS_MEM 0x050000

#define SYSLOG_PREFIX "KVMTOOL"

struct vchannel_device {
	struct pci_device_header	pci_hdr;
	struct device_header		dev_hdr;

	int gsi;
};

static void ch_syslog(const char *format, ...) {
	va_list args;

	// Write logs to the default syslog path (e.g, /var/log/syslog) & stderr as well
	openlog(SYSLOG_PREFIX, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);

	va_start(args, format);
	vsyslog(LOG_INFO, format, args);
	va_end(args);

	closelog();
}

#endif // ARM_AARCH64__CHANNEL_H
