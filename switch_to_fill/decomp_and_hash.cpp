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

void execute_yielding_requests_best_case(
  int total_requests,
  ax_comp *comps, timed_offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state,
  fcontext_state_t *self,
  uint64_t *off_times, uint64_t *yield_to_resume_times, uint64_t *hash_times, int idx)
{

  /*
  while(reqs < total)
  xfer = start_yielding_req
  wait_for_yielding_reqs_comp
  resume_yielding_req(xfer)
  */
  while(requests_completed < total_requests){
    offload_req_xfer[requests_completed] =
        fcontext_swap(off_req_state[requests_completed]->context, off_args[requests_completed]);
    while(comps[requests_completed].status == COMP_STATUS_COMPLETED){
      _mm_pause();
    }
    fcontext_swap(offload_req_xfer[requests_completed].prev_context, NULL);
    LOG_PRINT(LOG_DEBUG, "Request %d completed\n", requests_completed);
  }

  /* get the time stamps from one of the requests */
  uint64_t *ts0 = off_args[0]->ts0;
  uint64_t *ts1 = off_args[0]->ts1;
  uint64_t *ts2 = off_args[0]->ts2;
  uint64_t *ts3 = off_args[0]->ts3;
  uint64_t diff[total_requests];
  avg_samples_from_arrays(diff, off_times[idx], ts1, ts0, requests_completed);
  LOG_PRINT( LOG_DEBUG, "Offload time: %lu\n", off_times[idx]);
  avg_samples_from_arrays(diff, yield_to_resume_times[idx], ts2, ts1, requests_completed);
  LOG_PRINT( LOG_DEBUG, "YieldToResumeDelay: %lu\n", yield_to_resume_times[idx]);
  avg_samples_from_arrays(diff, hash_times[idx], ts3, ts2, requests_completed);
  LOG_PRINT( LOG_DEBUG, "HashTime: %lu\n", hash_times[idx]);
}

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
){
  using namespace std;
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  timed_offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;

  int requests_sampling_interval = total_requests;
  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t *ts0, *ts1, *ts2, *ts3;

  ts0 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts1 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts2 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts3 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);

  requests_completed = 0;

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* allocate request args */
  offload_args_allocator(total_requests, &off_args, comps,
    ts0, ts1, ts2, ts3);

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  execute_yielding_requests_best_case(
    total_requests,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    offload_time, wait_time, kernel2_time, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);

  fcontext_destroy(self);
}

void run_yielding_best_case_request_brkdown(
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
    yielding_best_case_request_breakdown(
      req_fn,
      offload_args_allocator,
      offload_args_free,
      total_requests,
      offloadtime, waittime, posttime, i
    );
  }
  print_mean_median_stdev(offloadtime, iter, "OffloadSwitchToFill");
  print_mean_median_stdev(waittime, iter, "YieldToResume");
  print_mean_median_stdev(posttime, iter, "PostProcessingSwitchToFill");
}


int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(int argc, char **argv){

  int wq_id = 0;
  int dev_id = 2;
  int wq_type = SHARED;
  int rc;
  int itr = 100;
  int total_requests = 1000;
  int opt;
  bool no_latency = false;
  int query_size = 64;

  while((opt = getopt(argc, argv, "t:i:r:s:q:d:")) != -1){
    switch(opt){
      case 't':
        total_requests = atoi(optarg);
        break;
      case 'i':
        itr = atoi(optarg);
        break;
      case 'o':
        no_latency = true;
        break;
      case 'q':
        query_size = atoi(optarg);
        break;
      case 'd':
        dev_id = atoi(optarg);
        break;
      default:
        break;
    }
  }
  initialize_iaa_wq(dev_id, wq_id, wq_type);


  std::string append_string = query;
  while(query.size() < query_size){
    query += append_string;
  }

  if(! no_latency){
    run_gpcore_request_brkdown(
      cpu_decompress_and_hash_stamped,
      compressed_mc_req_allocator,
      compressed_mc_req_free,
      itr,
      total_requests
    );
    run_blocking_offload_request_brkdown(
      blocking_decompress_and_hash_request_stamped,
      alloc_decomp_and_hash_offload_args_stamped,
      free_decomp_and_hash_offload_args_stamped,
      itr,
      total_requests
    );
    run_yielding_best_case_request_brkdown(
      yielding_decompress_and_hash_request_stamped,
      alloc_decomp_and_hash_offload_args_stamped,
      free_decomp_and_hash_offload_args_stamped,
      itr,
      total_requests
    );
  }

    int exetime_samples_per_run = 10;
    run_gpcore_offeredLoad(
      cpu_decompress_and_hash,
      compressed_mc_req_allocator,
      compressed_mc_req_free,
      itr,
      total_requests
    );
    run_blocking_offered_load(
      blocking_decompress_and_hash_request,
      alloc_decomp_and_hash_offload_args,
      free_decomp_and_hash_offload_args,
      total_requests,
      itr
    );
    run_yielding_offered_load(
      yielding_decompress_and_hash_request,
      alloc_decomp_and_hash_offload_args,
      free_decomp_and_hash_offload_args,
      exetime_samples_per_run,
      total_requests,
      itr
    );


  free_iaa_wq();
}