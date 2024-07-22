#ifndef REQUEST_EXEC
#define REQUEST_EXEC

#include "router_requests.h"
#include "offload.h"
extern "C" {
  #include "fcontext.h"
}
#include "print_utils.h"
#include "timer_utils.h"

void execute_yielding_requests_closed_system_with_sampling(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state,
  fcontext_state_t *self);

void execute_blocking_requests_closed_system_with_sampling(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state, fcontext_state_t *self);

void execute_cpu_requests_closed_system_with_sampling(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, cpu_request_args **off_args,
  fcontext_state_t **off_req_state, fcontext_state_t *self);

#endif