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
){
  using namespace std;
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  timed_offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;

  int sampling_intervals = 1;
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];
  uint64_t *ts0, *ts1, *ts2, *ts3;

  ts0 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts1 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts2 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts3 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);

  requests_completed = 0;

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  offload_args_allocator(total_requests, &off_args, comps,
    ts0, ts1, ts2, ts3);

  /* Pre-create the contexts */
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  execute_blocking_requests_closed_system_request_breakdown(
    total_requests, off_args,
    off_req_state, offload_time, wait_time, kernel2_time, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(comps);
  offload_args_free(total_requests, &off_args);
  free(ts0);
  free(ts1);
  free(ts2);
  free(ts3);

  fcontext_destroy(self);
}

void alloc_decomp_and_hash_offload_args(int total_requests,
  timed_offload_request_args*** p_off_args, ax_comp *comps,
  uint64_t *ts0, uint64_t *ts1, uint64_t *ts2, uint64_t *ts3){
  timed_offload_request_args **off_args =
    (timed_offload_request_args **)malloc(sizeof(timed_offload_request_args *) * total_requests);

  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE; /* using 4MB allocator */
  std::string query = "/region/cluster/foo:key|#|etc";
  for(int i=0; i<total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->comp = &(comps[i]);
    off_args[i]->id = i;

    /* Offload request needs a src payload to decompress */
    avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
    /* compress query into src payload */
    off_args[i]->src_payload = (char *)malloc(avail_out);
    gpcore_do_compress((void *) off_args[i]->src_payload,
      (void *) query.c_str(), query.size(), &avail_out);

    LOG_PRINT(LOG_DEBUG, "Compressed Size: %d\n", avail_out);

    /* and a dst payload to decompress into */
    off_args[i]->dst_payload = (char *)malloc(IAA_COMPRESS_MAX_DEST_SIZE);

    /* and the size of the compressed payload for decomp */
    off_args[i]->src_size = (uint64_t)avail_out;

    /* and a preallocated desc for prep and submit */
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));

    /* and timestamps */
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
  }
  *p_off_args = off_args;
}

void free_decomp_and_hash_offload_args(int total_requests, timed_offload_request_args*** off_args){
  timed_offload_request_args **off_args_ptr = *off_args;
  for(int i=0; i<total_requests; i++){
    free(off_args_ptr[i]->src_payload);
    free(off_args_ptr[i]->dst_payload);
    free(off_args_ptr[i]->desc);
    free(off_args_ptr[i]);
  }
  free(off_args_ptr);
}

void alloc_src_and_dst_compress_bufs(char **src1, char **dst1, char **src2,
  int input_size){
  *src1 = (char *) aligned_alloc(32, 1024);
  *dst1 = (char *) aligned_alloc(32, IAA_COMPRESS_MAX_DEST_SIZE);
  *src2 = (char *) aligned_alloc(32, IAA_COMPRESS_SRC2_SIZE);
}

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


  struct hw_desc *desc = args->desc;
  ax_comp *comp = args->comp;

  ts0[id] = sampleCoderdtsc();
  /* prep hw desc */
  prepare_iaa_decompress_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)comp, (uint64_t)src_size);
  acctest_desc_submit(iaa, desc);

  ts1[id] = sampleCoderdtsc();
  acctest_wait_on_desc_timeout(comp, iaa, 1000);
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Decompress failed: %d\n", comp->status);
  }

  ts2[id] = sampleCoderdtsc();
  /* hash the decompressed payload */
  LOG_PRINT(LOG_DEBUG, "Hashing: %s\n", dst);
  uint32_t hash = furc_hash((char *)dst, src_size, 16);

  ts3[id] = sampleCoderdtsc();

  requests_completed ++;

}

void run_blocking_offload_request_brkdown(
  fcontext_fn_t req_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int iter, int total_requests
){
  uint64_t *offloadtime;
  uint64_t *waittime, *posttime;
  offloadtime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  waittime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  posttime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++){
    blocking_ax_request_breakdown(
      req_fn,
      offload_args_allocator,
      offload_args_free,
      total_requests,
      offloadtime, waittime, posttime, i
    );
  }
  print_mean_median_stdev(offloadtime, iter, "Offload");
  print_mean_median_stdev(waittime, iter, "Wait");
  print_mean_median_stdev(posttime, iter, "Post");
}


int gLogLevel = LOG_DEBUG;
bool gDebugParam = false;
int main(){


  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;
  int rc;
  int itr = 1;
  int total_requests = 1;
  initialize_iaa_wq(dev_id, wq_id, wq_type);

  // run_gpcore_request_brkdown(
  //   cpu_decompress_and_hash_stamped,
  //   compressed_mc_req_allocator,
  //   compressed_mc_req_free,
  //   itr,
  //   total_requests
  // );
  run_blocking_offload_request_brkdown(
    blocking_decompress_and_hash_request_stamped,
    alloc_decomp_and_hash_offload_args,
    free_decomp_and_hash_offload_args,
    itr,
    total_requests
  );


  // return 0;


  acctest_free_task(iaa);
  acctest_free(iaa);
}