#ifndef REQUEST_EXEC
#define REQUEST_EXEC

#include "router_request_args.h"
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
  fcontext_state_t *self, uint64_t *exetime, int idx);
void execute_yielding_requests_closed_system_request_breakdown(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, timed_offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state,
  fcontext_state_t *self,
  uint64_t *off_times, uint64_t *yield_to_resume_times, uint64_t *hash_times, int idx);
void execute_yielding_requests_closed_system_request_breakdown(
  int total_requests,
  ax_comp *comps, timed_offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state,
  uint64_t *off_times, uint64_t *yield_to_resume_times, uint64_t *hash_times, int idx);
void execute_yielding_requests_best_case(
  int total_requests,
  ax_comp *comps, timed_offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state,
  fcontext_state_t *self,
  uint64_t *off_times, uint64_t *yield_to_resume_times, uint64_t *hash_times, int idx);
void execute_yielding_requests_interleaved(
  int total_requests,
  ax_comp *comps, timed_offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state,
  fcontext_state_t *self,
  fcontext_state_t *interleaved_state,
  fcontext_transfer_t interleaved_xfer,
  uint64_t *off_times, uint64_t *yield_to_resume_times, uint64_t *hash_times, int idx);
void execute_yielding_requests_multiple_filler_requests(
  int total_requests,
  ax_comp *comps, timed_offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state,
  fcontext_state_t **filler_req_state,
  uint64_t *off_times, uint64_t *yield_to_resume_times, uint64_t *hash_times, int idx);

void execute_blocking_requests_closed_system_with_sampling(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state, fcontext_state_t *self,
  uint64_t *exetime, int idx);
void execute_blocking_requests_closed_system_request_breakdown(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, timed_offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state, fcontext_state_t *self,
  uint64_t *off_times, uint64_t *wait_times, uint64_t *hash_times, int idx);
void execute_blocking_requests_closed_system_request_breakdown(
  int total_requests,
  timed_offload_request_args **off_args,
  fcontext_state_t **off_req_state,
  uint64_t *off_times, uint64_t *wait_times, uint64_t *kernel2_time,
  int idx);

void execute_cpu_requests_closed_system_with_sampling(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, cpu_request_args **off_args,
  fcontext_state_t **off_req_state, fcontext_state_t *self,
  uint64_t *rps, int idx);
void execute_cpu_requests_closed_system_request_breakdown(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, timed_cpu_request_args **off_args,
  fcontext_state_t **off_req_state, fcontext_state_t *self,
  uint64_t *deser_times, uint64_t *hash_times, int idx);

void execute_gpcore_requests_closed_system_with_sampling(
  int total_requests,
  gpcore_request_args **off_args,
  fcontext_state_t **off_req_state,
  uint64_t *exetime, int idx);
void execute_gpcore_requests_closed_system_request_breakdown(
  int total_requests,
  timed_gpcore_request_args **off_args,
  fcontext_state_t **off_req_state,
  uint64_t *kernel1, uint64_t *kernel2, int idx);

#endif