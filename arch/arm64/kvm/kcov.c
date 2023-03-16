// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 - Google LLC
 * Author: David Brazdil <dbrazdil@google.com>
 */

#include <linux/kcov.h>
#include <linux/kvm_host.h>

#include <asm/kvm_mmu.h>

#define KVM_KCOV_DISABLED		0
#define KVM_KCOV_ENABLED		BIT(0)
#define KVM_KCOV_PREEMPT		BIT(1)

static void kcov_stop_kvm_cb(void);

static kvm_pfn_t *kcov_map_area(void *start, size_t size)
{
	kvm_pfn_t *pfns;
	void *cur, *end = start + PAGE_ALIGN(size * sizeof(unsigned long));
	size_t nr_pfns, idx;
	int ret;

	/*
	 * The following code assumes area is page-aligned. Otherwise need
	 * to account for the offset in all pages.
	 */
	WARN_ON(!PAGE_ALIGNED(start));

	nr_pfns = (end - start) / PAGE_SIZE;
	pfns = kmalloc(sizeof(kvm_pfn_t) * nr_pfns, GFP_KERNEL);
	BUG_ON(!pfns);

	for (cur = start; cur < end; cur += PAGE_SIZE) {
		idx = (cur - start) / PAGE_SIZE;
		pfns[idx] = __phys_to_pfn(page_to_phys(vmalloc_to_page(cur)));

		ret = kvm_share_hyp(cur, cur + PAGE_SIZE - 1); /* XXX */
		BUG_ON(ret);
	}

	ret = kvm_share_hyp(pfns, pfns + nr_pfns);
	BUG_ON(ret);
	return pfns;
}

static void kcov_unmap_area(kvm_pfn_t *pfns, void *start, size_t size)
{
	void *cur, *end = start + PAGE_ALIGN(size * sizeof(unsigned long));
	size_t nr_pfns = (end - start) / PAGE_SIZE;

	for (cur = start; cur < end; cur += PAGE_SIZE)
		kvm_unshare_hyp(cur, cur + PAGE_SIZE - 1); /* XXX */

	kvm_unshare_hyp(pfns, pfns + nr_pfns);
	kfree(pfns);
}

int kcov_start_kvm(void)
{
	struct task_struct *t = current;
	kvm_pfn_t *pfns;
	int err, ret = KVM_KCOV_ENABLED;

	/* Step 1: Are we in a task? */
	if (!in_task())
		return KVM_KCOV_DISABLED;

	kvm_info("KCOV START KVM: Reached past Step 1");
	kvm_info("KCOV START KVM: kcov mode");

	unsigned int mode = t->kcov_mode;


	/* Step 2: Is kcov enabled for this task? Are we inside a kcov hyp section already? */
	switch (t->kcov_mode) {
	case KCOV_MODE_TRACE_PC:
		kvm_info("KCOV START KVM: KCOV_MODE_TRACE_PC");
		kcov_prepare_switch(t); /* modifies mode, fails step 4 */
		break;
	// Working out which mode it is
	case KCOV_MODE_DISABLED:
		kvm_info("KCOV START KVM: KCOV_MODE_DISABLED");
		return KVM_KCOV_DISABLED;
	case KCOV_MODE_INIT:
		kvm_info("KCOV START KVM: KCOV_MODE_INIT");
		return KVM_KCOV_DISABLED;
	case KCOV_MODE_TRACE_CMP:
		kvm_info("KCOV START KVM: KCOV_MODE_TRACE_CMP");
		return KVM_KCOV_DISABLED;
	default:
		kvm_info("KCOV START KVM: DEFAULT");
		return KVM_KCOV_DISABLED;
	}

	kvm_info("KCOV START KVM: Reached past Step 2");


	/* Step 3: Should we map in the area? */
	if (!t->kcov_stop_cb) {
		t->kcov_stop_cb = kcov_stop_kvm_cb;
		t->kcov_stop_cb_arg = kcov_map_area(t->kcov_area, t->kcov_size);
	} else if (t->kcov_stop_cb != kcov_stop_kvm_cb) {
		return KVM_KCOV_DISABLED;
	}
	pfns = t->kcov_stop_cb_arg;

	kvm_info("KCOV START KVM: Reached past Step 3");

	/* Step 4: Disable preemption to pin the area to this core. */
	if (preemptible()) {
		preempt_disable();
		ret |= KVM_KCOV_PREEMPT;
	}

	kvm_info("KCOV START KVM: Reached past Step 4");
	kvm_info("KCOV START KVM: About to call kcov hypercall");

	/* Step 5: Tell hyp to use this area. */
	err = kvm_call_hyp_nvhe(__kvm_kcov_set_area, kern_hyp_va(pfns), t->kcov_size);
	BUG_ON(err);

	return ret;
}

void kcov_stop_kvm(int ret)
{
	struct task_struct *t = current;

	if (!(ret & KVM_KCOV_ENABLED))
		return;

	/* Step 5B: Tell hyp to stop using this area. */
	WARN_ON(kvm_call_hyp_nvhe(__kvm_kcov_set_area, NULL, 0));

	/* Step 4B: Reenable preemption. */
	if (ret & KVM_KCOV_PREEMPT)
		preempt_enable();

	/* Step 2B: Get out of the kcov hyp section. */
	kcov_finish_switch(t);
}

static void kcov_stop_kvm_cb(void)
{
	struct task_struct *t = current;

	/* Warn if still in the kcov hyp section. */
	WARN_ON(t->kcov_mode != KCOV_MODE_TRACE_PC);

	kcov_prepare_switch(t);
	kcov_unmap_area(t->kcov_stop_cb_arg, t->kcov_area, t->kcov_size);
	kcov_finish_switch(t);
}
