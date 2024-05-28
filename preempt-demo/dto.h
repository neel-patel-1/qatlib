#ifndef DTO_H
#define DTO_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <cpuid.h>
#include <linux/idxd.h>
#include <x86intrin.h>
#include <sched.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <dlfcn.h>
#include <accel-config/libaccel_config.h>
#include <numaif.h>
#include <numa.h>

#define likely(x)       __builtin_expect((x), 1)
#define unlikely(x)     __builtin_expect((x), 0)

#define MAX_WQS 32
#define MAX_NUMA_NODES 32
#define DTO_DEFAULT_MIN_SIZE 8192
#define DTO_INITIALIZED 0
#define DTO_INITIALIZING 1

struct dto_wq {
	struct accfg_wq *acc_wq;
	char wq_path[PATH_MAX];
	uint64_t dsa_gencap;
	int wq_size;
	uint32_t max_transfer_size;
	int wq_fd;
	void *wq_portal;
};

struct dto_device {
	struct dto_wq* wqs[MAX_WQS];
	uint8_t num_wqs;
	atomic_uchar next_wq;
};

enum return_code {
	SUCCESS = 0x0,
	RETRIES,
	PAGE_FAULT,
	FAIL_OTHERS,
	MAX_FAILURES,
};

// DSA capabilities
#define GENCAP_CC_MEMORY  0x4

#define ENQCMD_MAX_RETRIES 3

#define UMWAIT_DELAY 100000
/* C0.1 state */
#define UMWAIT_STATE 1

static __always_inline unsigned char enqcmd(struct dsa_hw_desc *desc, volatile void *reg)
{
	unsigned char retry;

	asm volatile(".byte 0xf2, 0x0f, 0x38, 0xf8, 0x02\t\n"
			"setz %0\t\n"
			: "=r"(retry) : "a" (reg), "d" (desc));
	return retry;
}

static __always_inline void movdir64b(struct dsa_hw_desc *desc, volatile void *reg)
{
	asm volatile(".byte 0x66, 0x0f, 0x38, 0xf8, 0x02\t\n"
		: : "a" (reg), "d" (desc));
}

static __always_inline void umonitor(const volatile void *addr)
{
	asm volatile(".byte 0xf3, 0x48, 0x0f, 0xae, 0xf0" : : "a"(addr));
}

static __always_inline int umwait(unsigned long timeout, unsigned int state)
{
	uint8_t r;
	uint32_t timeout_low = (uint32_t)timeout;
	uint32_t timeout_high = (uint32_t)(timeout >> 32);

	asm volatile(".byte 0xf2, 0x48, 0x0f, 0xae, 0xf1\t\n"
		"setc %0\t\n"
		: "=r"(r)
		: "c"(state), "a"(timeout_low), "d"(timeout_high));
	return r;
}

static __always_inline int dsa_submit(struct dto_wq *wq,
	struct dsa_hw_desc *hw)
{
	int retry;
	//LOG_TRACE("desc flags: 0x%x, opcode: 0x%x\n", hw->flags, hw->opcode);
	__builtin_ia32_sfence();
	for (int r = 0; r < ENQCMD_MAX_RETRIES; ++r) {
		retry = enqcmd(hw, wq->wq_portal);
		if (!retry)
			return SUCCESS;
	}
	return RETRIES;
}

#endif