#ifndef MEMFILL_GATHER
#define MEMFILL_GATHER
#include "print_utils.h"
#include "dsa_offloads.h"
extern "C" {
  #include "fcontext.h"
  #include "accel_test.h"
}
#include <string>
#include <cstdlib>
#include <cstdio>
#include "submit.hpp"
#include "payload_gen.h"
#include "gather_scatter.h"

void alloc_offload_memfill_and_gather_args_timed( /* is this scatter ?*/
  int total_requests,
  timed_offload_request_args *** p_off_args,
  ax_comp *comps, uint64_t *ts0,
  uint64_t *ts1, uint64_t *ts2,
  uint64_t *ts3
);
void alloc_offload_memfill_and_gather_args( /* is this scatter ?*/
  int total_requests,
  offload_request_args *** p_off_args,
  ax_comp *comps
);
void free_offload_memfill_and_gather_args(
  int total_requests,
  timed_offload_request_args *** p_off_args
);
void free_offload_memfill_and_gather_args(
  int total_requests,
  offload_request_args *** p_off_args
);
void free_cpu_memcpy_and_compute_args(
  int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads
);
void alloc_cpu_memcpy_and_compute_args(
  int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);

void blocking_memcpy_and_compute_stamped(
  fcontext_transfer_t arg
);
void blocking_memcpy_and_compute(
  fcontext_transfer_t arg
);
void yielding_memcpy_and_compute(
  fcontext_transfer_t arg
);

void cpu_memcpy_and_compute_stamped(
  fcontext_transfer_t arg
);
void cpu_memcpy_and_compute(
  fcontext_transfer_t arg
);

#endif