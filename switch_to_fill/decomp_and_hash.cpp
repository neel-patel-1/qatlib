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

enum runner_type {
  GPCORE,
  BLOCKING,
  YIELDING
};

typedef struct _runner_thread_args{
  enum runner_type runner_type;
  int itr;
  int total_requests;
  fcontext_fn_t request_fn;
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3);
  void (* offload_args_free)(int, timed_offload_request_args***);
  void (* cpu_payload_alloc)(int, char****);
  void (* cpu_payload_free)(int, char****);
} runner_thread_args;

void *runner_thread_fn(void *arg){
  runner_thread_args *args = (runner_thread_args *)arg;
  fcontext_fn_t request_fn = args->request_fn;
  int itr = args->itr;
  int total_requests = args->total_requests;
  enum runner_type runner_type = args->runner_type;

  switch(runner_type){
    case GPCORE:
      run_gpcore_request_brkdown(
        request_fn,
        args->cpu_payload_alloc,
        args->cpu_payload_free,
        itr,
        total_requests
      );
      break;
    case BLOCKING:
      run_blocking_offload_request_brkdown(
        request_fn,
        args->offload_args_allocator,
        args->offload_args_free,
        itr,
        total_requests
      );
      break;
    case YIELDING:
      run_yielding_request_brkdown(
        request_fn,
        args->offload_args_allocator,
        args->offload_args_free,
        itr,
        total_requests
      );
      break;
  }

  return NULL;
}

int gLogLevel = LOG_DEBUG;
bool gDebugParam = false;
int main(){


  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;
  int rc;
  int itr = 1;
  int total_requests = 1000;
  initialize_iaa_wq(dev_id, wq_id, wq_type);

  // int query_sizes[] = {31, 256, 1024, 4096, 16384, 65536}; // 10 x 10000
  // int query_sizes[] = {256 * 1024, 1024 * 1024}; // 10 x 100
  // int query_sizes[] = {31, 256, 1024, 4096, 16384, 65536,
  //   256 * 1024, 1024 * 1024}; // 100 * 100
  std::string append_string = query;
  pthread_t runner_thread;
  runner_thread_args args;
  args.itr = itr;
  args.total_requests = total_requests;


  int query_sizes[] = {65536}; /* testing a single large size -- trigger software fallback*/

  for (auto query_size : query_sizes){
    LOG_PRINT(LOG_PERF, "QuerySize %d\n", query_size);
    while(query.size() < query_size){
      query += append_string;
    }
    // run_gpcore_request_brkdown(
    //   cpu_decompress_and_hash_stamped,
    //   compressed_mc_req_allocator,
    //   compressed_mc_req_free,
    //   itr,
    //   total_requests
    // );
    run_blocking_offload_request_brkdown(
      blocking_decompress_and_hash_request_stamped,
      alloc_decomp_and_hash_offload_args_stamped,
      free_decomp_and_hash_offload_args_stamped,
      itr,
      total_requests
    );
    run_yielding_request_brkdown(
      yielding_decompress_and_hash_request_stamped,
      alloc_decomp_and_hash_offload_args_stamped,
      free_decomp_and_hash_offload_args_stamped,
      itr,
      total_requests
    );
    int exetime_samples_per_run = 10;
    // run_gpcore_offeredLoad(
    //   cpu_decompress_and_hash,
    //   compressed_mc_req_allocator,
    //   compressed_mc_req_free,
    //   itr,
    //   total_requests
    // );
    // run_blocking_offered_load(
    //   blocking_decompress_and_hash_request,
    //   alloc_decomp_and_hash_offload_args,
    //   free_decomp_and_hash_offload_args,
    //   total_requests,
    //   itr
    // );
    // run_yielding_offered_load(
    //   yielding_decompress_and_hash_request,
    //   alloc_decomp_and_hash_offload_args,
    //   free_decomp_and_hash_offload_args,
    //   exetime_samples_per_run,
    //   total_requests,
    //   itr
    // );
  }

  free_iaa_wq();
}