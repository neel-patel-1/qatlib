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

void blocking_ax_request_breakdown(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char***),
  void (* payload_free)(int, char***),
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

  /* pre-allocate the payloads */
  payload_allocator(total_requests, &dst_bufs);

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* Pre-allocate the request args */
  allocate_timed_offload_requests(total_requests, &off_args, comps, dst_bufs, ts0, ts1, ts2, ts3);

  /* Pre-create the contexts */
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  execute_blocking_requests_closed_system_request_breakdown(
    total_requests, off_args,
    off_req_state, offload_time, wait_time, kernel2_time, idx);

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

void gpcore_closed_loop_test(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char****),
  void (* payload_free)(int, char****),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
)
{
  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];

  fcontext_state_t *self = fcontext_create_proxy();
  char ***dst_bufs;
  fcontext_state_t **cpu_req_state;
  gpcore_request_args **gpcore_args;

  payload_allocator(total_requests, &dst_bufs);
  cpu_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);
  create_contexts(cpu_req_state, total_requests, request_fn);

  gpcore_args = (gpcore_request_args **)malloc(sizeof(gpcore_request_args *) * total_requests);
  for(int i=0; i<total_requests; i++){
    gpcore_args[i] = (gpcore_request_args *)malloc(sizeof(gpcore_request_args));
    gpcore_args[i]->inputs = (char **)(dst_bufs[i]);
    gpcore_args[i]->id = i;
  }

  requests_completed = 0;
  execute_gpcore_requests_closed_system_with_sampling(
    total_requests, gpcore_args, cpu_req_state, exetime, idx );

  free_contexts(cpu_req_state, total_requests);
  payload_free(total_requests, &dst_bufs);

  fcontext_destroy(self);
}


void gpcore_request_breakdown(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char****),
  void (* payload_free)(int, char****),
  int total_requests,
  uint64_t *kernel1, uint64_t *kernel2, int idx
)
{
  int sampling_intervals = 1;
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];

  fcontext_state_t *self = fcontext_create_proxy();
  char ***dst_bufs;
  fcontext_state_t **cpu_req_state;
  timed_gpcore_request_args **gpcore_args;
  uint64_t *ts0, *ts1, *ts2;

  payload_allocator(total_requests, &dst_bufs);
  cpu_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);
  create_contexts(cpu_req_state, total_requests, request_fn);

  ts0 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts1 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts2 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);

  gpcore_args = (timed_gpcore_request_args **)malloc(sizeof(timed_gpcore_request_args *) * total_requests);
  for(int i=0; i<total_requests; i++){
    gpcore_args[i] = (timed_gpcore_request_args *)malloc(sizeof(timed_gpcore_request_args));
    gpcore_args[i]->inputs = (char **)(dst_bufs[i]);
    gpcore_args[i]->id = i;
    gpcore_args[i]->ts0 = ts0;
    gpcore_args[i]->ts1 = ts1;
    gpcore_args[i]->ts2 = ts2;
  }

  requests_completed = 0;
  execute_gpcore_requests_closed_system_request_breakdown(
    total_requests, gpcore_args, cpu_req_state, kernel1, kernel2, idx );


  free_contexts(cpu_req_state, total_requests);
  payload_free(total_requests, &dst_bufs);

  fcontext_destroy(self);
}

void yielding_offload_ax_closed_loop_test(
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
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  execute_yielding_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    exetime, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);
  for(int i=0; i<total_requests; i++){
    free(off_args[i]);
  }
  free(off_args);

  payload_free(total_requests, &dst_bufs);

  fcontext_destroy(self);
}

void yielding_request_breakdown(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char***),
  void (* payload_free)(int, char***),
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
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];
  uint64_t *ts0, *ts1, *ts2, *ts3;

  ts0 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts1 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts2 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts3 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);

  requests_completed = 0;

  /* pre-allocate the payloads */
  payload_allocator(total_requests, &dst_bufs);

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* Pre-allocate the request args */
  allocate_timed_offload_requests(total_requests, &off_args, comps, dst_bufs,
    ts0, ts1, ts2, ts3);

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);

  execute_yielding_requests_closed_system_request_breakdown(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    offload_time, wait_time, kernel2_time, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);
  for(int i=0; i<total_requests; i++){
    free(off_args[i]);
  }
  free(off_args);

  payload_free(total_requests, &dst_bufs);

  fcontext_destroy(self);
}

void yielding_request_breakdown(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)
    (int, timed_offload_request_args***,
      ax_comp *comps, uint64_t *ts0,
      uint64_t *ts1, uint64_t *ts2,
      uint64_t *ts3),
  void (* offload_args_free)(int, timed_offload_request_args***),
  int total_requests,
  uint64_t *offload_time, uint64_t *wait_time, uint64_t *kernel2_time, int idx
)
{
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

  execute_yielding_requests_closed_system_request_breakdown(
    total_requests,
    comps, off_args,
    offload_req_xfer, off_req_state,
    offload_time, wait_time, kernel2_time, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);

  fcontext_destroy(self);
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

void yielding_multiple_filler_request_breakdown(
  fcontext_fn_t request_fn,
  fcontext_fn_t filler_fn,
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
  fcontext_state_t **filler_req_state;

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
  filler_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, request_fn);
  create_contexts(filler_req_state, total_requests, filler_fn);

  execute_yielding_requests_multiple_filler_requests(
    total_requests,
    comps, off_args,
    offload_req_xfer,
    off_req_state,
    filler_req_state,
    offload_time, wait_time, kernel2_time, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free_contexts(filler_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);

  fcontext_destroy(self);
}

void yielding_same_interleaved_request_breakdown(
  fcontext_fn_t request_fn,
  fcontext_fn_t interleave_fn, /* should take a comp record as arg and check it every 200 cycles */
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
  fcontext_transfer_t interleaved_xfer;
  fcontext_state_t *interleaved_state;

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
  interleaved_state = fcontext_create(interleave_fn);

  create_contexts(off_req_state, total_requests, request_fn);

  execute_yielding_requests_interleaved(
    total_requests,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    interleaved_state, interleaved_xfer,
    offload_time, wait_time, kernel2_time, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);

  fcontext_destroy(self);
}



void yielding_request_offered_load(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
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

  execute_yielding_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    exetime, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);

  fcontext_destroy(self);
}

void blocking_request_offered_load(
  fcontext_fn_t request_fn,
  void (* offload_args_allocator)(int, offload_request_args***, ax_comp *comps),
  void (* offload_args_free)(int, offload_request_args***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
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

  execute_blocking_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    exetime, idx);

  /* teardown */
  free_contexts(off_req_state, total_requests);
  free(offload_req_xfer);
  free(comps);

  offload_args_free(total_requests, &off_args);

  fcontext_destroy(self);
}