#include <kvm/util.h>
#include <kvm/kvm-cmd.h>
#include <kvm/builtin-version.h>
#include <kvm/kvm.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>

int kvm_cmd_version(int argc, const char **argv, const char *prefix)
{
#ifndef RIM_MEASURE
	printf("kvm tool %s\n", KVMTOOLS_VERSION);
#else
	printf("Arm CCA Realm Initial Measure (RIM) calculator based on the kvm tool %s\n", KVMTOOLS_VERSION);
#endif

	return 0;
}
