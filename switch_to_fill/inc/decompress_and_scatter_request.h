#ifndef DECOMPRESS_AND_HASH_REQUEST_HPP
#define DECOMPRESS_AND_HASH_REQUEST_HPP
#include "gpcore_compress.h"
extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
  #include <zlib.h>
}
#include "print_utils.h"
#include "ch3_hash.h"
#include <string>
#include "submit.hpp"
#include "gather_scatter.h"

/* defined in decomp_and_scatter.cpp */
extern int input_size;
extern int num_accesses;
extern std::string payload;
extern int *glob_indir_arr;

void blocking_decomp_and_scatter_request(
  fcontext_transfer_t arg);
void blocking_decomp_and_scatter_request_stamped(
  fcontext_transfer_t arg);

void yielding_decomp_and_scatter_stamped(
  fcontext_transfer_t arg);
void yielding_decomp_and_scatter(
  fcontext_transfer_t arg);

void free_offload_decomp_and_scatter_args(
  int total_requests,
  offload_request_args *** p_off_args
);
void free_offload_decomp_and_scatter_args_timed(
  int total_requests,
  timed_offload_request_args *** p_off_args
);

void alloc_offload_decomp_and_scatter_args(int total_requests,
  offload_request_args *** p_off_args,
  ax_comp *comps);
void alloc_offload_decomp_and_scatter_args_timed( /* is this scatter ?*/
  int total_requests,
  timed_offload_request_args *** p_off_args,
  ax_comp *comps, uint64_t *ts0,
  uint64_t *ts1, uint64_t *ts2,
  uint64_t *ts3
);

void cpu_decomp_and_scatter(
  fcontext_transfer_t arg
);
void cpu_decomp_and_scatter_stamped(
  fcontext_transfer_t arg
);

void free_cpu_compressed_payloads(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
void cpu_compressed_payload_allocator(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
#endif