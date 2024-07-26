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

void blocking_request_offered_load_synchronized(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx,
  pthread_barrier_t *test_start_barrier,
  pthread_barrier_t *tear_down_barrier
)
{
  using namespace std;
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;

  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];


  requests_completed = 0;

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* allocate request args */
  offload_args_allocator(total_requests, &off_args, comps);

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  /* synchronize before starting */
  pthread_barrier_wait(test_start_barrier);

  execute_blocking_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    exetime, idx);

  pthread_barrier_wait(tear_down_barrier);
  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);

  /* synchronize before exiting */

  fcontext_destroy(self);
}

void run_blocking_offered_load_synchronized(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int total_requests,
  int itr,
  pthread_barrier_t *test_start_barrier,
  pthread_barrier_t *tear_down_barrier){

  // this function executes all requests from start to finish only taking stamps at beginning and end
  uint64_t exetime[itr];
  for(int i = 0; i < itr; i++){
    blocking_request_offered_load_synchronized(
      request_fn,
      offload_args_allocator,
      offload_args_free,
      total_requests,
      total_requests,
      exetime,
      i,
      test_start_barrier,
      tear_down_barrier
    );
  }

  mean_median_stdev_rps(
    exetime, itr, total_requests, "BlockingOffloadRPS"
  );
}

void yielding_request_offered_load_synchronized(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx,
  pthread_barrier_t *test_start_barrier,
  pthread_barrier_t *tear_down_barrier
)
{
  using namespace std;
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;

  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];


  requests_completed = 0;

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* allocate request args */
  offload_args_allocator(total_requests, &off_args, comps);

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  pthread_barrier_wait(test_start_barrier);

  execute_yielding_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    exetime, idx);

  pthread_barrier_wait(tear_down_barrier);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);


  fcontext_destroy(self);
}


void run_yielding_offered_load_synchronized(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int num_exe_time_samples_per_run,
  int total_requests, int iter,
  pthread_barrier_t *test_start_barrier,
  pthread_barrier_t *tear_down_barrier){

  /*
    we take exe time samples periodically as a fixed number of requests complete
    This enables discarding results collected during the latter phase of the test
    where the system has ran out of work to execute during blocking stalls
  */
  int num_requests_before_stamp = total_requests / num_exe_time_samples_per_run;
  int total_exe_time_samples = iter * num_exe_time_samples_per_run;
  uint64_t exetime[total_exe_time_samples];

  /* this function takes multiple samples */
  for(int i = 0; i < iter; i++){
    yielding_request_offered_load_synchronized(
      request_fn,
      offload_args_allocator,
      offload_args_free,
      num_requests_before_stamp,
      total_requests,
      exetime,
      i * num_exe_time_samples_per_run,
      test_start_barrier,
      tear_down_barrier
    );
  }

  for(int i=0; i<total_exe_time_samples; i++){
    LOG_PRINT(LOG_DEBUG, "ExeTime: %lu\n", exetime[i]);
  }
  mean_median_stdev_rps(
    exetime, total_exe_time_samples, num_requests_before_stamp, "SwitchToFillRPS"
  );
}

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
    (int, offload_request_args***,
      ax_comp *comps);
  void (* offload_args_free)(int, offload_request_args***);

  void (* cpu_payload_alloc)(int, char****);
  void (* cpu_payload_free)(int, char****);

  pthread_barrier_t *test_start_barrier;
  pthread_barrier_t *tear_down_barrier;

  int num_exe_time_samples_per_run;
} runner_thread_args;

void *runner_thread_fn(void *arg){
  runner_thread_args *args = (runner_thread_args *)arg;
  fcontext_fn_t request_fn = args->request_fn;
  int itr = args->itr;
  int total_requests = args->total_requests;
  enum runner_type runner_type = args->runner_type;
  pthread_barrier_t *test_start_barrier = args->test_start_barrier;
  pthread_barrier_t *tear_down_barrier = args->tear_down_barrier;

  switch(runner_type){
    case GPCORE:
      // run_gpcore_request_brkdown(
      //   request_fn,
      //   args->cpu_payload_alloc,
      //   args->cpu_payload_free,
      //   itr,
      //   total_requests
      // );
      break;
    case BLOCKING:
      run_blocking_offered_load_synchronized(
        request_fn,
        args->offload_args_allocator,
        args->offload_args_free,
        itr,
        total_requests,
        test_start_barrier,
        tear_down_barrier
      );
      break;
    case YIELDING:
      run_yielding_offered_load_synchronized(
        request_fn,
        args->offload_args_allocator,
        args->offload_args_free,
        args->num_exe_time_samples_per_run,
        total_requests,
        itr,
        test_start_barrier,
        tear_down_barrier
      );
      break;
  }

  return NULL;
}

void start_multi_thread_test(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int total_requests,
  int num_exe_time_samples_per_run,
  int itr,
  int num_workers,
  enum runner_type runner_type
){
  int cores[num_workers];
  pthread_t threads[num_workers];
  pthread_barrier_t test_start_barrier;
  pthread_barrier_t tear_down_barrier;
  runner_thread_args *args;

  for(int i=0; i<num_workers; i++){
    cores[i] = i;
  }

  pthread_barrier_init(&test_start_barrier, NULL, num_workers);
  pthread_barrier_init(&tear_down_barrier, NULL, num_workers);
  for(int i=0; i<num_workers; i++){
    args = (runner_thread_args *)malloc(sizeof(runner_thread_args));
    args->itr = itr;
    args->total_requests = total_requests;
    args->request_fn = request_fn;
    args->offload_args_allocator = offload_args_allocator;
    args->offload_args_free = offload_args_free;
    args->runner_type = runner_type;
    args->itr = itr;
    args->test_start_barrier = &test_start_barrier;
    args->tear_down_barrier = &tear_down_barrier;
    args->num_exe_time_samples_per_run = num_exe_time_samples_per_run;
    int rc = create_thread_pinned(&(threads[i]), runner_thread_fn, args, cores[i]);
    if (rc){
      LOG_PRINT(LOG_ERR, "Error creating thread\n");
      exit(-1);
    }
  }
  for(auto thread: threads){
    pthread_join(thread, NULL);
  }
  pthread_barrier_destroy(&test_start_barrier);
  pthread_barrier_destroy(&tear_down_barrier);
}

int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(){

  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;
  int rc;
  int itr = 2;
  int total_requests = 100;
  initialize_iaa_wq(dev_id, wq_id, wq_type);

  std::string append_string = query;


  int num_workers = 1;
  int cores[num_workers];
  pthread_t threads[num_workers];
  pthread_barrier_t test_start_barrier;
  pthread_barrier_t tear_down_barrier;
  int query_size = 65536; /* testing a single large size -- trigger software fallback*/
  runner_thread_args *args;


  for(int i=0; i<num_workers; i++){
    cores[i] = i;
  }

  LOG_PRINT(LOG_PERF, "QuerySize %d\n", query_size);

  while(query.size() < query_size){
    query += append_string;
  }

  start_multi_thread_test(
    blocking_decompress_and_hash_request,
    alloc_decomp_and_hash_offload_args,
    free_decomp_and_hash_offload_args,
    total_requests,
    -1,
    itr,
    num_workers,
    BLOCKING
  );

  start_multi_thread_test(
    yielding_decompress_and_hash_request,
    alloc_decomp_and_hash_offload_args,
    free_decomp_and_hash_offload_args,
    total_requests,
    1,
    itr,
    num_workers,
    YIELDING
  );


    // run_gpcore_request_brkdown(
    //   cpu_decompress_and_hash_stamped,
    //   compressed_mc_req_allocator,
    //   compressed_mc_req_free,
    //   itr,
    //   total_requests
    // );
    // for (auto core: cores){
    //   args.request_fn = cpu_decompress_and_hash_stamped;
    //   args.cpu_payload_alloc = alloc_decomp_and_hash_offload_args_stamped;
    //   args.cpu_payload_free = free_decomp_and_hash_offload_args_stamped;
    //   args.runner_type = GPCORE;
    //   rc = pthread_create(&runner_thread, NULL, runner_thread_fn, &args);
    //   if (rc){
    //     LOG_PRINT(LOG_ERROR, "Error creating thread\n");
    //     exit(-1);
    //   }
    //   pthread_join(runner_thread, NULL);
    // }
    // run_blocking_offload_request_brkdown(
    //   blocking_decompress_and_hash_request_stamped,
    //   alloc_decomp_and_hash_offload_args_stamped,
    //   free_decomp_and_hash_offload_args_stamped,
    //   itr,
    //   total_requests
    // );

    // run_yielding_request_brkdown(
    //   yielding_decompress_and_hash_request_stamped,
    //   alloc_decomp_and_hash_offload_args_stamped,
    //   free_decomp_and_hash_offload_args_stamped,
    //   itr,
    //   total_requests
    // );
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


  free_iaa_wq();
}