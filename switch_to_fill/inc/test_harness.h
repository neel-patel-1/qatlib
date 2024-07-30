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
void blocking_ax_request_breakdown(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int total_requests,
  uint64_t *offload_time, uint64_t *wait_time, uint64_t *kernel2_time, int idx
);
void blocking_request_offered_load(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
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
void yielding_request_offered_load(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
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
void yielding_request_breakdown(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int total_requests,
  uint64_t *offload_time, uint64_t *wait_time, uint64_t *kernel2_time, int idx
);
void yielding_best_case_request_breakdown(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int total_requests,
  uint64_t *offload_time, uint64_t *wait_time, uint64_t *kernel2_time, int idx
);
void yielding_same_interleaved_request_breakdown(
  fcontext_fn_t request_fn,
  fcontext_fn_t interleave_fn, /* should take a comp record as arg and check it every 200 cycles */
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int total_requests,
  uint64_t *offload_time, uint64_t *wait_time, uint64_t *kernel2_time, int idx
);
void yielding_multiple_filler_request_breakdown(
  fcontext_fn_t request_fn,
  fcontext_fn_t filler_fn, /*filler should take a preemption signal argument and check it every 200 cycles */
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int total_requests,
  uint64_t *offload_time,
  uint64_t *wait_time,
  uint64_t *kernel2_time,
  int idx
);

#endif