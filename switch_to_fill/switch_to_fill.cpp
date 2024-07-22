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

void yielding_ax_router_closed_loop_test(int requests_sampling_interval,
  int total_requests){
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
  allocate_pre_deserialized_payloads(total_requests, &dst_bufs, query);

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
    offload_req_xfer, off_req_state, self);

  calculate_rps_from_samples(
    sampling_interval_completion_times,
    sampling_intervals,
    requests_sampling_interval,
    2100000000);

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

void yielding_ax_router_request_breakdown_closed_loop_test(int requests_sampling_interval,
  int total_requests){
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  timed_offload_request_args **off_args;
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
  off_args = (timed_offload_request_args **)malloc(sizeof(timed_offload_request_args *) * total_requests);
  uint64_t *ts0, *ts1, *ts2, *ts3;
  ts0 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts1 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts2 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts3 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);

  for(int i=0; i<total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->comp = &comps[i];
    off_args[i]->dst_payload = dst_bufs[i];
    off_args[i]->id = i;
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
  }

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, yielding_router_request_stamp);

  execute_yielding_requests_closed_system_request_breakdown(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self);

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

void blocking_ax_router_closed_loop_test(int requests_sampling_interval, int total_requests){
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
  allocate_pre_deserialized_payloads(total_requests, &dst_bufs, query);

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* Pre-allocate the request args */
  allocate_offload_requests(total_requests, &off_args, comps, dst_bufs);

  /* Pre-create the contexts */
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, blocking_router_request);

  execute_blocking_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    NULL, off_req_state, self);

  calculate_rps_from_samples(
    sampling_interval_completion_times,
    sampling_intervals,
    requests_sampling_interval,
    2100000000);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(comps);
  for(int i=0; i<total_requests; i++){
    free(off_args[i]);
    free(dst_bufs[i]);
  }
  free(off_args);
  free(dst_bufs);

  fcontext_destroy(self);
}


void cpu_router_closed_loop_test(int requests_sampling_interval, int total_requests, uint64_t *rps, int idx){
  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];

  fcontext_state_t *self = fcontext_create_proxy();
  router::RouterRequest **serializedMCReqs;
  serializedMCReqs = (router::RouterRequest **)malloc(sizeof(router::RouterRequest *) * total_requests);
  string **serializedMCReqStrings = (string **)malloc(sizeof(string *) * total_requests);
  for(int i=0; i<total_requests; i++){
    serializedMCReqs[i] = new router::RouterRequest(); /*preallocated request obj*/
    serializedMCReqStrings[i] = new string();
    serialize_request(serializedMCReqs[i], serializedMCReqStrings[i]);
  }

  cpu_request_args **cpu_args;
  cpu_args = (cpu_request_args **)malloc(sizeof(cpu_request_args *) * total_requests);
  for(int i=0; i<total_requests; i++){
    cpu_args[i] = (cpu_request_args *)malloc(sizeof(cpu_request_args));
    cpu_args[i]->request = serializedMCReqs[i];
    cpu_args[i]->serialized = serializedMCReqStrings[i];
  }

  fcontext_state_t **cpu_req_state;
  cpu_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);
  create_contexts(cpu_req_state, total_requests, cpu_router_request);

  requests_completed = 0;
  execute_cpu_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    NULL, cpu_args,
    cpu_req_state, self,
    rps, idx);

  calculate_rps_from_samples(
    sampling_interval_completion_times,
    sampling_intervals,
    requests_sampling_interval,
    2100000000);

  free_contexts(cpu_req_state, total_requests);
  for(int i=0; i<total_requests; i++){
    delete serializedMCReqs[i];
    delete serializedMCReqStrings[i];
    free(cpu_args[i]);
  }
  free(cpu_args);
  free(serializedMCReqs);
  free(serializedMCReqStrings);

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

  // for(int i=0; i<iter; i++)
  //   yielding_ax_router_request_breakdown_closed_loop_test(requests_sampling_interval, total_requests);

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

  LOG_PRINT( LOG_PERF, "Offload Mean: %lu Median: %lu Stddev: %lu\n", off_mean, off_median, off_stddev);
  LOG_PRINT( LOG_PERF, "Wait Mean: %lu Median: %lu Stddev: %lu\n", wait_mean, wait_median, wait_stddev);
  LOG_PRINT( LOG_PERF, "Hash Mean: %lu Median: %lu Stddev: %lu\n", hash_mean, hash_median, hash_stddev);

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