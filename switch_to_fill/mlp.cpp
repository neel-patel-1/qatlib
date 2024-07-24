#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"
#include "iaa_offloads.h"
#include "ch3_hash.h"
#include "runners.h"
#include "gpcore_compress.h"

extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
  #include <zlib.h>
}
#include "decompress_and_hash_request.hpp"


void blocking_decompress_and_hash_request_stamped(
  fcontext_transfer_t arg){

  timed_offload_request_args *args =
    (timed_offload_request_args *) arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  uint64_t src_size = args->src_size;
  uint64_t dst_size = args->dst_size;


  struct hw_desc *desc = args->desc;
  ax_comp *comp = args->comp;

  ts0[id] = sampleCoderdtsc();
  /* prep hw desc */
  prepare_iaa_decompress_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)comp, (uint64_t)src_size);
  acctest_desc_submit(iaa, desc);

  ts1[id] = sampleCoderdtsc();
  while(comp->status == IAX_COMP_NONE){
    _mm_pause();
  }
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Decompress failed: 0x%x\n", comp->status);
  }
  LOG_PRINT(LOG_DEBUG, "Decompressed size: %d\n", comp->iax_output_size);

  ts2[id] = sampleCoderdtsc();
  /* hash the decompressed payload */
  LOG_PRINT(LOG_DEBUG, "Hashing: %s %ld bytes\n", dst, dst_size);
  uint32_t hash = furc_hash((char *)dst, dst_size, 16);

  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void yielding_decompress_and_hash_request_stamped(
  fcontext_transfer_t arg){

  timed_offload_request_args *args =
    (timed_offload_request_args *) arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  uint64_t src_size = args->src_size;
  uint64_t dst_size = args->dst_size;


  struct hw_desc *desc = args->desc;
  ax_comp *comp = args->comp;

  ts0[id] = sampleCoderdtsc();
  /* prep hw desc */
  prepare_iaa_decompress_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)comp, (uint64_t)src_size);
  acctest_desc_submit(iaa, desc);

  ts1[id] = sampleCoderdtsc();
  fcontext_swap(arg.prev_context, NULL);
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Decompress failed: %d\n", comp->status);
  }
  LOG_PRINT(LOG_DEBUG, "Decompressed size: %d\n", comp->iax_output_size);

  ts2[id] = sampleCoderdtsc();
  /* hash the decompressed payload */
  LOG_PRINT(LOG_DEBUG, "Hashing: %s %ld bytes\n", dst, dst_size);
  uint32_t hash = furc_hash((char *)dst, dst_size, 16);

  ts3[id] = sampleCoderdtsc();
  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}


int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(){


  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;
  int rc;
  int itr = 1000;
  int total_requests = 1000;
  initialize_iaa_wq(dev_id, wq_id, wq_type);

  run_gpcore_request_brkdown(
    cpu_decompress_and_hash_stamped,
    compressed_mc_req_allocator,
    compressed_mc_req_free,
    itr,
    total_requests
  );
  run_blocking_offload_request_brkdown(
    blocking_decompress_and_hash_request_stamped,
    alloc_decomp_and_hash_offload_args,
    free_decomp_and_hash_offload_args,
    itr,
    total_requests
  );
  run_yielding_request_brkdown(
    yielding_decompress_and_hash_request_stamped,
    alloc_decomp_and_hash_offload_args,
    free_decomp_and_hash_offload_args,
    itr,
    total_requests
  );

  free_iaa_wq();
}