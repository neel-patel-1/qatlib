#include "timer_utils.h"
#include <pthread.h>
#include "print_utils.h"
#include "ch3_hash.h"
#include "status.h"
#include "router.pb.h"
#include "emul_ax.h"
#include "thread_utils.h"
#include "offload.h"

extern "C"{
#include "fcontext.h"
}
#include <cstdlib>
#include <atomic>
#include <list>
#include <cstring>
#include <x86intrin.h>
#include <string>

#include "router_requests.h"
#include "request_executors.h"
#include "context_management.h"
#include "rps.h"
#include "stats.h"

/*
  End-to-End Evaluation
  - Blocking vs. Switch to Fill
  - Accelerator Data access overhead

  Accelerators
  - Deser
  - Pointer-chasing
  - MLP followed by (user,item) ranking

  Output generation/post-processing:
  - preallocated buffers: as long as we know where to access them for post-processing

  Switch-To-Fill Scheduling:
  - Count completed requests to know when to terminate
*/


using namespace std;

bool gDebugParam = false;
int gLogLevel = LOG_PERF;

/*
  exetime needs to be (total_requests / requests_sampling_interval) x test_iterations

  the caller needs to ensure the "early yielder underutilization" problem does not
  impact the reported offered load.

  caller must tune the sampling interval and choose the exetime samples that are not impacted
  by underutilization

  indexing into the array: caller must ensure they increment the start_idx by
    (total_requests / requests_sampling_interval)
*/
void yielding_ax_router_closed_loop_test(int requests_sampling_interval,
  int total_requests, uint64_t *exetime, int start_idx){
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;
  string query = "/region/cluster/foo:key|#|etc";

  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];

  requests_completed = 0;
  allocate_pre_deserialized_dsa_payloads(total_requests, &dst_bufs, query);

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* Pre-allocate the request args */
  allocate_offload_requests(total_requests, &off_args, comps, dst_bufs);

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, yielding_router_request);

  execute_yielding_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    exetime, start_idx);


  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);
  for(int i=0; i<total_requests; i++){
    free(off_args[i]);
    free(dst_bufs[i]);
  }
  free(off_args);
  free(dst_bufs);

  fcontext_destroy(self);
}


int main(){
  gDebugParam = true;
  pthread_t ax_td;
  bool ax_running = true;
  int offload_time = 511;
  int max_inflight = 128;
  start_non_blocking_ax(&ax_td, &ax_running, offload_time, max_inflight);

  int iter = 10;
  int requests_sampling_interval = 1000, total_requests = 10000;

  char**dst_bufs;
  string query = "/region/cluster/foo:key|#|etc";
  uint64_t *off_times, *wait_times, *hash_times;
  uint64_t *deser_times;

  uint64_t *exetime;

  exetime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++)
    cpu_router_closed_loop_test(total_requests, total_requests, exetime, i);

  uint64_t exetimemean = avg_from_array(exetime, iter);
  uint64_t exetimemedian = median_from_array(exetime, iter);
  uint64_t exetimestddev = stddev_from_array(exetime, iter);
  double rpsmean = (double)total_requests / (exetimemean / 2100000000.0);
  double rpsmedian = (double)total_requests / (exetimemedian / 2100000000.0);

  LOG_PRINT( LOG_PERF, "CPU ExeTime Mean: %lu Median: %lu Stddev: %lu\n", exetimemean, exetimemedian, exetimestddev);
  LOG_PRINT( LOG_PERF, "CPU RPS Mean: %f Median: %f\n", rpsmean, rpsmedian);

  exetime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++){
    blocking_ax_router_closed_loop_test(total_requests, total_requests,
      exetime, i);
  }
  exetimemean = avg_from_array(exetime, iter);
  exetimemedian = median_from_array(exetime, iter);
  exetimestddev = stddev_from_array(exetime, iter);
  rpsmean = (double)total_requests / (exetimemean / 2100000000.0);
  rpsmedian = (double)total_requests / (exetimemedian / 2100000000.0);

  LOG_PRINT( LOG_PERF, "Blocking ExeTime Mean: %lu Median: %lu Stddev: %lu\n", exetimemean, exetimemedian, exetimestddev);
  LOG_PRINT( LOG_PERF, "Blocking RPS Mean: %f Median: %f\n", rpsmean, rpsmedian);
  free(exetime);

  int num_yield_samples = total_requests / requests_sampling_interval * iter;
  int num_yield_samples_per_iter = total_requests / requests_sampling_interval;
  exetime = (uint64_t *)malloc(sizeof(uint64_t) * num_yield_samples);
  for(int i=0; i<iter; i++)
    yielding_ax_router_closed_loop_test(requests_sampling_interval,
      total_requests, exetime, i * num_yield_samples_per_iter);
  exetimemean = avg_from_array(exetime, num_yield_samples);
  exetimemedian = median_from_array(exetime, num_yield_samples);
  exetimestddev = stddev_from_array(exetime, num_yield_samples);
  rpsmean = (double)requests_sampling_interval / (exetimemean / 2100000000.0);
  rpsmedian = (double)requests_sampling_interval / (exetimemedian / 2100000000.0);

  LOG_PRINT( LOG_PERF, "SwitchToFill ExeTime Mean: %lu Median: %lu Stddev: %lu\n", exetimemean, exetimemedian, exetimestddev);
  LOG_PRINT( LOG_PERF, "SwitchToFill RPS Mean: %f Median: %f\n", rpsmean, rpsmedian);


  off_times = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  wait_times = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  hash_times = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++)
    blocking_ax_router_request_breakdown_test(total_requests, total_requests,
      off_times, wait_times, hash_times, i);

  uint64_t off_mean = avg_from_array(off_times, iter);
  uint64_t off_median = median_from_array(off_times, iter);
  uint64_t off_stddev = stddev_from_array(off_times, iter);
  uint64_t wait_mean = avg_from_array(wait_times, iter);
  uint64_t wait_median = median_from_array(wait_times, iter);
  uint64_t wait_stddev = stddev_from_array(wait_times, iter);
  uint64_t hash_mean = avg_from_array(hash_times, iter);
  uint64_t hash_median = median_from_array(hash_times, iter);
  uint64_t hash_stddev = stddev_from_array(hash_times, iter);

  LOG_PRINT( LOG_PERF, "Blocking Offload Mean: %lu Median: %lu Stddev: %lu\n", off_mean, off_median, off_stddev);
  LOG_PRINT( LOG_PERF, "Blocking Wait Mean: %lu Median: %lu Stddev: %lu\n", wait_mean, wait_median, wait_stddev);
  LOG_PRINT( LOG_PERF, "Blocking Hash Mean: %lu Median: %lu Stddev: %lu\n", hash_mean, hash_median, hash_stddev);

  for(int i=0; i<iter; i++)
    yielding_ax_router_request_breakdown_closed_loop_test(requests_sampling_interval, total_requests,
      off_times, wait_times, hash_times, i);

  off_mean = avg_from_array(off_times, iter);
  off_median = median_from_array(off_times, iter);
  off_stddev = stddev_from_array(off_times, iter);
  wait_mean = avg_from_array(wait_times, iter);
  wait_median = median_from_array(wait_times, iter);
  wait_stddev = stddev_from_array(wait_times, iter);
  hash_mean = avg_from_array(hash_times, iter);
  hash_median = median_from_array(hash_times, iter);
  hash_stddev = stddev_from_array(hash_times, iter);

  LOG_PRINT( LOG_PERF, "Yielding Offload Mean: %lu Median: %lu Stddev: %lu\n", off_mean, off_median, off_stddev);
  LOG_PRINT( LOG_PERF, "Yielding YieldToResumeDelay Mean: %lu Median: %lu Stddev: %lu\n", wait_mean, wait_median, wait_stddev);
  LOG_PRINT( LOG_PERF, "Yielding Hash Mean: %lu Median: %lu Stddev: %lu\n", hash_mean, hash_median, hash_stddev);

  free(off_times);
  free(wait_times);
  free(hash_times);

  deser_times = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  hash_times = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++)
    cpu_router_request_breakdown(total_requests, total_requests,
      deser_times, hash_times, i);

  uint64_t deser_mean = avg_from_array(deser_times, iter);
  uint64_t deser_median = median_from_array(deser_times, iter);
  uint64_t deser_stddev = stddev_from_array(deser_times, iter);
  hash_mean = avg_from_array(hash_times, iter);
  hash_median = median_from_array(hash_times, iter);
  hash_stddev = stddev_from_array(hash_times, iter);

  LOG_PRINT( LOG_PERF, "Deser Mean: %lu Median: %lu Stddev: %lu\n", deser_mean, deser_median, deser_stddev);
  LOG_PRINT( LOG_PERF, "Hash Mean: %lu Median: %lu Stddev: %lu\n", hash_mean, hash_median, hash_stddev);

  free(deser_times);
  free(hash_times);


  stop_non_blocking_ax(&ax_td, &ax_running);


  return 0;
}