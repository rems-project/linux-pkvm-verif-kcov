// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 - Google LLC
 * Author: David Brazdil <dbrazdil@google.com>
 */

#include <asm/kvm_asm.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>

#include <nvhe/memory.h>

struct kvm_kcov {
	kvm_pfn_t *pfns;
	size_t size;
};

static DEFINE_PER_CPU(struct kvm_kcov, kvm_kcov);

int kvm_kcov_set_area(kvm_pfn_t *pfns, size_t size)
{
	*this_cpu_ptr(&kvm_kcov) = (struct kvm_kcov){
		.pfns = pfns,
		.size = size,
	};

	return 0;
}

static __always_inline notrace unsigned long canonicalize_ip(unsigned long ip)
{
	ip = hyp_kimg_va(ip);
#ifdef CONFIG_RANDOMIZE_BASE
	ip -= __kaslr_offset();
#endif
	return ip;
}

static __always_inline notrace unsigned long *area_ptr(size_t idx)
{
	struct kvm_kcov *kcov = this_cpu_ptr(&kvm_kcov);
	size_t off = idx * sizeof(unsigned long);

	if (!kcov->pfns || idx >= kcov->size)
		return NULL;

	return hyp_pfn_to_virt(kcov->pfns[off / PAGE_SIZE]) + (off % PAGE_SIZE);
}

/* Workaround for ptrauth */
#undef __builtin_return_address

void notrace __sanitizer_cov_trace_pc(void)
{
	unsigned long pos, ip = canonicalize_ip(_RET_IP_);

	if (!area_ptr(0))
		return;

	/* The first 64-bit word is the number of subsequent PCs. */
	pos = READ_ONCE(*area_ptr(0)) + 1;
	if (likely(area_ptr(pos))) {
		*area_ptr(pos) = ip;
		WRITE_ONCE(*area_ptr(0), pos);
	}
}
