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
#include "test_harness.h"


void cpu_decompress_and_hash_stamped(fcontext_transfer_t arg);
void compressed_mc_req_free(int total_requests, char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
void compressed_mc_req_allocator(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
void alloc_decomp_and_hash_offload_args(int total_requests,
  timed_offload_request_args*** p_off_args, ax_comp *comps,
  uint64_t *ts0, uint64_t *ts1, uint64_t *ts2, uint64_t *ts3);
void free_decomp_and_hash_offload_args(int total_requests, timed_offload_request_args*** off_args);


#endif