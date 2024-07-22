#include "request_executors.h"


void execute_yielding_requests_closed_system_with_sampling(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state,
  fcontext_state_t *self)
{

  int next_unstarted_req_idx = 0;
  int next_request_offload_to_complete_idx = 0;
  int sampling_interval = 0;


  sampling_interval_completion_times[0] = sampleCoderdtsc(); /* start time */
  sampling_interval++;

  while(requests_completed < total_requests){
    if(comps[next_request_offload_to_complete_idx].status == COMP_STATUS_COMPLETED){
      fcontext_swap(offload_req_xfer[next_request_offload_to_complete_idx].prev_context, NULL);
      next_request_offload_to_complete_idx++;
      if(requests_completed % requests_sampling_interval == 0 && requests_completed > 0){
        sampling_interval_completion_times[sampling_interval] = sampleCoderdtsc();
        sampling_interval++;
      }
    } else if(next_unstarted_req_idx < total_requests){
      offload_req_xfer[next_unstarted_req_idx] =
        fcontext_swap(off_req_state[next_unstarted_req_idx]->context, off_args[next_unstarted_req_idx]);
      next_unstarted_req_idx++;
    }
  }
}

void execute_blocking_requests_closed_system_with_sampling(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, offload_request_args **off_args,
  fcontext_transfer_t *offload_req_xfer,
  fcontext_state_t **off_req_state, fcontext_state_t *self)
{
  uint64_t start, end;

  start = sampleCoderdtsc();
  int next_unstarted_req_idx = 0;
  while(requests_completed < total_requests){
    if(comps[next_unstarted_req_idx].status != COMP_STATUS_PENDING){
      PRINT_ERR("Request already completed\n");
    }
    fcontext_swap(off_req_state[next_unstarted_req_idx]->context, off_args[next_unstarted_req_idx]);
    next_unstarted_req_idx++;
  }

  end = sampleCoderdtsc();
  PRINT_DBG("BlockingRequests: %d Cycles: %ld\n", total_requests,  end - start);
}

void execute_cpu_requests_closed_system_with_sampling(
  int requests_sampling_interval, int total_requests,
  uint64_t *sampling_interval_completion_times, int sampling_interval_timestamps,
  ax_comp *comps, cpu_request_args **off_args,
  fcontext_state_t **off_req_state, fcontext_state_t *self)
{

  uint64_t start, end;

  start = sampleCoderdtsc();

  int next_unstarted_req_idx = 0;
  while(requests_completed < total_requests){
    fcontext_swap(off_req_state[next_unstarted_req_idx]->context, off_args[next_unstarted_req_idx]);
    next_unstarted_req_idx++;
  }

  end = sampleCoderdtsc();

  PRINT_DBG("CPURequests: %d Cycles: %ld\n", total_requests,  end - start);
}