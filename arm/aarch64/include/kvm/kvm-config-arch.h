#ifndef KVM__KVM_CONFIG_ARCH_H
#define KVM__KVM_CONFIG_ARCH_H

int vcpu_affinity_parser(const struct option *opt, const char *arg, int unset);

#ifdef RIM_MEASURE
#define ARM_OPT_ARCH_RUN_MEASURE(cfg) 				\
	OPT_STRING('\0', "mpidrs", &(cfg)->mpidr,		\
	"mpidr values",									\
	"comma-separated MPIDR values for CPUs"),
#else
#define ARM_OPT_ARCH_RUN_MEASURE(...)
#endif

#define ARM_OPT_ARCH_RUN(cfg)						\
	ARM_OPT_ARCH_RUN_MEASURE(cfg)					\
	OPT_BOOLEAN('\0', "aarch32", &(cfg)->aarch32_guest,		\
			"Run AArch32 guest"),				\
	OPT_BOOLEAN('\0', "pmu", &(cfg)->has_pmuv3,			\
			"Create PMUv3 device. The emulated PMU will be" \
			" set to the PMU associated with the"		\
			" main thread, unless --vcpu-affinity is set"),	\
	OPT_BOOLEAN('\0', "disable-mte", &(cfg)->mte_disabled,		\
			"Disable Memory Tagging Extension"),		\
	OPT_CALLBACK('\0', "vcpu-affinity", kvm, "cpulist",  		\
			"Specify the CPU affinity that will apply to "	\
			"all VCPUs", vcpu_affinity_parser, kvm),	\
	OPT_U64('\0', "kaslr-seed", &(cfg)->kaslr_seed,			\
			"Specify random seed for Kernel Address Space "	\
			"Layout Randomization (KASLR)"),		\
	OPT_BOOLEAN('\0', "no-pvtime", &(cfg)->no_pvtime, "Disable"	\
			" stolen time"),				\
	OPT_BOOLEAN('\0', "disable-sve", &(cfg)->disable_sve,		\
			"Disable SVE"),					\
	OPT_BOOLEAN('\0', "realm", &(cfg)->is_realm,			\
			"Create VM running in a realm using Arm RME"),	\
	OPT_STRING('\0', "measurement-algo", &(cfg)->measurement_algo,	\
			 "sha256, sha512",				\
			 "Realm Measurement algorithm, default: sha256"),\
	OPT_STRING('\0', "realm-pv", &(cfg)->realm_pv,			\
			"personalisation value",			\
			"Personalisation Value (only) for Realm VMs"),	\
	OPT_U64('\0', "sve-vl", &(cfg)->sve_vl,				\
			"SVE Vector Length the VM"			\
			"(only supported for Realms)"),

#include "arm-common/kvm-config-arch.h"

#endif /* KVM__KVM_CONFIG_ARCH_H */
