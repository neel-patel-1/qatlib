#include "timer_utils.h"
#include <cstdlib>
#include <atomic>
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

  RPS: completed_requests / exe_time

  buffers that will be divied up upon request -- these can be pre-allocated
  We do care about data access overhead




  emul_non_blocking_ax -- meets offload times we specify?

  submit_flag <- while this is set an offload is waiting to be observed

  we need a copy of the host's descriptor from inside the portal
*/
#define STATUS_SUCCESS 0
#define STATUS_FAIL 1
#define OFFLOAD_RECEIVED 1
#define OFFLOAD_REQUESTED 0
std::atomic<int> submit_flag;
std::atomic<int> submit_status;
typedef struct _offload_entry{
  uint64_t start_time;
  uint64_t comp_time;
  struct _offload_entry *next;
} offload_entry;
typedef struct _ax_params {
  int max_inflights;
  uint64_t offload_time;
} ax_params;


void nonblocking_emul_ax(void *arg){
  ax_params *args = (ax_params *)arg;
  bool offload_in_flight = false;
  offload_entry *in_flight_ents = NULL;
  offload_entry *last_offload_ent = NULL;
  uint64_t next_offload_completion_time = 0;
  int max_inflight = args->max_inflights;
  uint64_t offload_time = args->offload_time;
  int in_flight = 0;

  if(offload_in_flight){
    /*
      if time is up for the earliest submitted offload
        notify the offloader
      if we have any other offloads in flight
        make sure we let ourselves know
        make sure we start checking the current time against the time for the next earliest submitted offload
    */
    in_flight_ents = in_flight_ents->next;
    if( in_flight_ents != NULL ){
      offload_in_flight = true;
      next_offload_completion_time = in_flight_ents->comp_time;
    }

  }
  if(submit_flag.load() == OFFLOAD_REQUESTED){
    uint64_t start_time = sampleCoderdtsc();
    /* ***All post processing off critical path to get closest to host-observed offload latency
    if can_accept_offload:
      add offload entry to in flight set
    else:
      notify submitter of failure
    */
    if(in_flight < max_inflight ){
      submit_status = STATUS_SUCCESS;
      submit_flag = OFFLOAD_RECEIVED; /*received submission*/
      offload_entry *new_ent = (offload_entry *)malloc(sizeof(offload_entry));
      new_ent->start_time = start_time;
      new_ent->comp_time = start_time + offload_time;
      last_offload_ent->next = new_ent;
      last_offload_ent = new_ent;
      in_flight++;
    } else {
      submit_status = STATUS_FAIL;
      submit_flag = OFFLOAD_RECEIVED; /*received submission*/
    }

  }
}

int main(){


  return 0;
}