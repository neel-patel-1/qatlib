#ifndef MEMCPY_DP_REQUEST_H
#define MEMCPY_DP_REQUEST_H
#include <cstdlib>
#include <cstdio>
extern "C" {
  #include "fcontext.h"
  #include "accel_test.h"
}
#include "decompress_and_scatter_request.h"
#include "submit.hpp"
#include <string>

#include "dsa_offloads.h"

/* from mlp.cpp */
extern int input_size;
extern void (*input_populate)(char **);
extern void (*compute_on_input)(void *, int);
extern float *item_buf;


void free_cpu_memcpy_and_compute_args(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);

void cpu_memcpy_and_compute_stamped(fcontext_transfer_t arg);

void alloc_offload_memcpy_and_compute_args(
  int total_requests,
  timed_offload_request_args*** p_off_args,
  ax_comp *comps,
  uint64_t *ts0,
  uint64_t *ts1,
  uint64_t *ts2,
  uint64_t *ts3
);
void alloc_offload_memcpy_and_compute_args(
  int total_requests,
  offload_request_args *** p_off_args,
  ax_comp *comps
);
void free_offload_memcpy_and_compute_args(
  int total_requests,
  timed_offload_request_args*** p_off_args
);
void free_offload_memcpy_and_compute_args(
  int total_requests,
  offload_request_args *** p_off_args
);

void blocking_memcpy_and_compute_stamped(fcontext_transfer_t arg);
void blocking_memcpy_and_compute(
  fcontext_transfer_t arg
);

void yielding_memcpy_and_compute_stamped(fcontext_transfer_t arg);
void yielding_memcpy_and_compute(
  fcontext_transfer_t arg
);

void cpu_memcpy_and_compute(
  fcontext_transfer_t arg
);
void alloc_cpu_memcpy_and_compute_args(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);

#endif