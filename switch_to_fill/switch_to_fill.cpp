#include "timer_utils.h"
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


  8:57 - 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17 - 9:44

  Switch-To-Fill Scheduling:
  - Count completed requests to know when to terminate

  RPS: completed_requests / exe_time

  buffers that will be divied up upon request -- these can be pre-allocated
  We do care about data access overhead




  emul_non_blocking_ax -- meets offload times we specify?

  submit_flag <- while this is set an offload is waiting to be observed

*/
#define STATUS_SUCCESS 0
#define STATUS_FAIL 1
#define OFFLOAD_RECEIVED 1
#define OFFLOAD_REQUESTED 0
std::atomic<int> submit_flag;
std::atomic<int> submit_status;
typedef struct _offload_entry{
  uint64_t start_time;
  struct _offload_entry *next;
} offload_entry;
typedef struct _ax_params {
  int max_inflights;
} ax_params;


void nonblocking_emul_ax(void *arg){
  bool offload_in_flight = false;
  offload_entry *in_flight_ents = NULL;
  uint64_t next_offload_completion_time = 0;
  if(offload_in_flight){
    /*
      if time is up for the earliest submitted offload
        notify the offloader
      if we have any other offloads in flight
        make sure we let ourselves know
        make sure we start checking the current time against the time for the next earliest submitted offload
    */

  }
  if(submit_flag.load() == OFFLOAD_REQUESTED){
    uint64_t start_time = sampleCoderdtsc();
    /* ***All post processing off critical path to get closest to host-observed offload latency
    if can_accept_offload:
      add offload entry to in flight set
    else:
      notify submitter of failure
    */
    submit_flag = OFFLOAD_RECEIVED; /*received submission*/
  }
}

int main(){


  return 0;
}