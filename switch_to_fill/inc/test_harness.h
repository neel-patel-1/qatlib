#ifndef TEST_HARNESS
#define TEST_HARNESS

extern "C" {
  #include "fcontext.h"
}

#include "emul_ax.h"
#include "offload.h"
#include "context_management.h"

extern int requests_completed;

void blocking_ax_closed_loop_test(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char***),
  void (* payload_free)(int, char***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
  );
void blocking_ax_request_breakdown(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char***),
  void (* payload_free)(int, char***),
  int total_requests,
  uint64_t *offload_time, uint64_t *wait_time, uint64_t *kernel2_time, int idx
);

void gpcore_closed_loop_test(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char****),
  void (* payload_free)(int, char****),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
);
void gpcore_request_breakdown(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char****),
  void (* payload_free)(int, char****),
  int total_requests,
  uint64_t *kernel1, uint64_t *kernel2, int idx
);

void yielding_offload_ax_closed_loop_test(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char***),
  void (* payload_free)(int, char***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
);
void yielding_request_breakdown(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char***),
  void (* payload_free)(int, char***),
  int total_requests,
  uint64_t *offload_time, uint64_t *wait_time, uint64_t *kernel2_time, int idx
);

#endif