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
#define COMP_STATUS_COMPLETED 1
#define COMP_STATUS_PENDING 0
std::atomic<int> submit_flag;
std::atomic<int> submit_status;
typedef struct completion_record{
  int status;
} ax_comp;
typedef struct _offload_entry{
  uint64_t start_time;
  uint64_t comp_time;
  struct _offload_entry *next;
  ax_comp *comp;
} offload_entry;
typedef struct _desc{
  int32_t id;
  ax_comp *comp;
  uint8_t rsvd[52];
} desc;
typedef struct _ax_params {
  int max_inflights;
  uint64_t offload_time;
} ax_params;


void nonblocking_emul_ax(void *arg){
  ax_params *args = (ax_params *)arg;
  bool offload_in_flight = false;
  offload_entry *earliest_inflight_ent = NULL;
  offload_entry *last_offload_ent = NULL;
  uint64_t next_offload_completion_time = 0;
  int max_inflight = args->max_inflights;
  uint64_t offload_time = args->offload_time;
  int in_flight = 0;
  ax_comp *next_completed_offload_comp = NULL;

  if(offload_in_flight){
    /*
      if time is up for the earliest submitted offload
        notify the offloader
    */
    uint64_t cur_time = sampleCoderdtsc();
    if(cur_time >= next_offload_completion_time){
      next_completed_offload_comp->status = COMP_STATUS_COMPLETED;
      in_flight--;
    }

    /*
      if we have any other offloads in flight
        make sure we let ourselves know
        make sure we start checking the current time against the time for the next earliest submitted offload
    */
    earliest_inflight_ent = earliest_inflight_ent->next;
    if( earliest_inflight_ent != NULL ){
      offload_in_flight = true;
      next_offload_completion_time = earliest_inflight_ent->comp_time;
    } else {
      offload_in_flight = false;
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