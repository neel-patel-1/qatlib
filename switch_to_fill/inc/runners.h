#ifndef RUNNERS_H
#define RUNNERS_H

#include "status.h"
#include "emul_ax.h"
extern "C"{
#include "fcontext.h"
}

#include "test_harness.h"
#include "stats.h"

uint64_t run_gpcore_request_brkdown(fcontext_fn_t req_fn,
  void (*payload_alloc)(int,char****),
  void (*payload_free)(int,char****),
  int iter, int total_requests);
void run_gpcore_offeredLoad(
  fcontext_fn_t req_fn,
  void (*payload_alloc)(int,char****),
  void (*payload_free)(int,char****),
  int iter, int total_requests);

void run_blocking_offload_request_brkdown(fcontext_fn_t req_fn,
  void (*payload_alloc)(int,char***),
  void (*payload_free)(int,char***),
  int iter, int total_requests);
void run_blocking_offload_request_brkdown(
  fcontext_fn_t req_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int iter, int total_requests
);
void run_blocking_offered_load(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int total_requests,
  int itr);

void run_yielding_request_brkdown(fcontext_fn_t req_fn,
  void (*payload_alloc)(int,char***),
  void (*payload_free)(int,char***),
  int iter, int total_requests);
void run_yielding_request_brkdown(
  fcontext_fn_t req_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int iter, int total_requests
);
void run_yielding_offered_load(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int num_exe_time_samples_per_run,
  int total_requests, int iter);
void run_yielding_best_case_request_brkdown(
  fcontext_fn_t req_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int iter, int total_requests
);
void run_yielding_interleaved_request_brkdown(
  fcontext_fn_t req_fn,
  fcontext_fn_t inter_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int iter, int total_requests
);
void run_yielding_multiple_filler_request_brkdown(
  fcontext_fn_t req_fn,
  fcontext_fn_t inter_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int iter, int total_requests
);


#endif