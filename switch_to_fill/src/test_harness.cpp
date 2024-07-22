#include "test_harness.h"
#include "request_executors.h"

int requests_completed = 0;

void blocking_ax_closed_loop_test(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char***),
  void (* payload_free)(int, char***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
  ){
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

  /* pre-allocate the payloads */
  payload_allocator(total_requests, &dst_bufs);

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* Pre-allocate the request args */
  allocate_offload_requests(total_requests, &off_args, comps, dst_bufs);

  /* Pre-create the contexts */
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  execute_blocking_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    NULL, off_req_state, self, exetime, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(comps);
  for(int i=0; i<total_requests; i++){
    free(off_args[i]);
  }
  free(off_args);

  payload_free(total_requests, &dst_bufs);

  fcontext_destroy(self);
}