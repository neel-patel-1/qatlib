#include "timer_utils.h"
#include <pthread.h>
#include "print_utils.h"
#include "ch3_hash.h"
#include "status.h"

extern "C"{
#include "fcontext.h"
}
#include <cstdlib>
#include <atomic>
#include <list>
#include <cstring>
#include <x86intrin.h>
#include <string>

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
#define SUBMIT_SUCCESS 0
#define SUBMIT_FAIL 1
#define OFFLOAD_RECEIVED 1
#define OFFLOAD_REQUESTED 0
#define COMP_STATUS_COMPLETED 1
#define COMP_STATUS_PENDING 0
std::atomic<int> submit_flag;
std::atomic<int> submit_status;
std::atomic<uint64_t> compl_addr;
std::atomic<uint64_t> p_dst_buf;
int offloads_completed = 0;
bool gDebugParam = false;
typedef struct completion_record{
  int status;
} ax_comp;
class offload_entry{
  public:
  offload_entry(){
    start_time = 0;
    comp_time = 0;
    comp = NULL;
  }
  offload_entry(uint64_t start, uint64_t compl_time, ax_comp *c){
    start_time = start;
    comp_time = compl_time;
    comp = c;
  }
  uint64_t start_time;
  uint64_t comp_time;
  int32_t id;
  ax_comp *comp;
};
typedef struct _ax_params {
  int max_inflights;
  uint64_t offload_time;
  bool *ax_running;
} ax_params;
typedef struct _offload_request_args{
  ax_comp *comp;
  char *dst_payload;
} offload_request_args;


using namespace std;

int create_thread_pinned(pthread_t *thread, void *(*func)(void*), void *arg, int coreId){
    pthread_attr_t attr;
  cpu_set_t cpuset;
  struct sched_param param;

  int status = pthread_attr_init(&attr);

  if(status != 0){
    PRINT_DBG( "Error initializing thread attributes\n");
    return STATUS_FAIL;
  }

  CPU_ZERO(&cpuset);
  CPU_SET(coreId, &cpuset);
  status = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
  if(status != 0){
    PRINT_DBG( "Error setting thread affinity\n");
    pthread_attr_destroy(&attr);
    return STATUS_FAIL;
  }

  status = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  if(status != 0){
    PRINT_DBG( "Error setting thread scheduling inheritance\n");
    pthread_attr_destroy(&attr);
    return STATUS_FAIL;
  }

  if (pthread_attr_setschedpolicy(
        &attr, SCHED_RR) != 0)
  {
      PRINT_DBG(
              "Failed to set scheduling policy for thread!\n");
      pthread_attr_destroy(&attr);
      return STATUS_FAIL;
  }

  memset(&param, 0, sizeof(param));
  param.sched_priority = 15;
  if (pthread_attr_setschedparam(&attr, &param) != 0)
  {
      PRINT_DBG(
              "Failed to set the sched parameters attribute!\n");
      pthread_attr_destroy(&attr);
      return STATUS_FAIL;
  }

  if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0)
  {
      PRINT_DBG(
              "Failed to set the dettachState attribute!\n");
      pthread_attr_destroy(&attr);
      return STATUS_FAIL;
  }
  if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM) != 0)
  {
      PRINT_DBG("Failed to set the attribute!\n");
      pthread_attr_destroy(&attr);
      return STATUS_FAIL;
  }
  if(pthread_create(thread, &attr, func, arg) != 0){
    PRINT_DBG( "Error creating thread\n");
    PRINT_DBG( "Creating thread with NULL attributes\n");
    pthread_create(thread, NULL, func, arg);
  }
  return STATUS_SUCCESS;
}


void *nonblocking_emul_ax(void *arg){
  ax_params *args = (ax_params *)arg;
  bool offload_in_flight = false;
  std::list<offload_entry*> offload_in_flight_list;
  uint64_t next_offload_completion_time = UINT64_MAX;
  int max_inflight = args->max_inflights;
  uint64_t offload_time = args->offload_time;
  int in_flight = 0;
  ax_comp *next_completed_offload_comp = NULL;
  bool *keep_running = args->ax_running;

  /* Debugging offload duration */
  uint64_t offloadDurationSum = 0;
  uint64_t totalOffloads = 0;
  uint64_t rejected_offloads = 0;

  while(*keep_running){

    if(offload_in_flight){
      /*
        if time is up for the earliest submitted offload
          notify the offloader
      */
      uint64_t cur_time = sampleCoderdtsc();
      if(cur_time >= next_offload_completion_time){
        next_completed_offload_comp = offload_in_flight_list.front()->comp;
        next_completed_offload_comp->status = COMP_STATUS_COMPLETED;
        in_flight--;

        if( true == gDebugParam){
          offloadDurationSum += cur_time - offload_in_flight_list.front()->start_time;
        }


          /*
            if we have any other offloads in flight
              make sure we let ourselves know
              make sure we start checking the current time against the time for the next earliest submitted offload
          */
        offload_in_flight_list.pop_front();
        if( offload_in_flight_list.size() > 0 ){
          offload_in_flight = true;
          next_offload_completion_time = offload_in_flight_list.front()->comp_time;
        } else {
          offload_in_flight = false;
        }
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
        submit_status = SUBMIT_SUCCESS;
        struct completion_record *comp_addr = (struct completion_record *)compl_addr.load();
        submit_flag = OFFLOAD_RECEIVED; /*received submission*/

        uint64_t comp_time = start_time + offload_time;
        offload_entry *new_ent = new offload_entry(start_time, comp_time, comp_addr);
        offload_in_flight_list.push_back(new_ent);
        in_flight++;
        offload_in_flight = true;
        if(offload_in_flight_list.size()  == 1 ){
          next_offload_completion_time = comp_time;
        }
        totalOffloads++;
      } else {
        submit_status = SUBMIT_FAIL;
        submit_flag = OFFLOAD_RECEIVED; /*received submission*/
        rejected_offloads++;
      }

    }
  }

  PRINT_DBG("Avg_Offload_Duration: %ld Rejected_Offloads: %ld\n",
          offloadDurationSum / totalOffloads, rejected_offloads );
  return NULL;
}

int submit_offload(ax_comp *comp, char *dst_payload){
  int num_retries = 3, retries_remaining = num_retries;
  comp->status = COMP_STATUS_PENDING;
  compl_addr = (uint64_t)comp;
  submit_flag = OFFLOAD_REQUESTED;
  p_dst_buf = (uint64_t)dst_payload;

retry:
  while(submit_flag.load() == OFFLOAD_REQUESTED){
    _mm_pause();
  }

  if(submit_status == SUBMIT_FAIL && retries_remaining > 0){
    retries_remaining --;
    goto retry;
  } else if(retries_remaining == 0){
    PRINT_DBG("Offload request failed after %d retries\n", num_retries);
    return STATUS_FAIL;
  }
  retries_remaining = num_retries;

  return STATUS_SUCCESS;
}

void router_request(fcontext_transfer_t arg){
  offload_request_args *args = (offload_request_args *)arg.data;
  struct completion_record * comp = args->comp;
  char *dst_payload = args->dst_payload;
  string query = "/region/cluster/foo:key|#|etc";

  int status = submit_offload(comp, dst_payload);
  if(status == STATUS_FAIL){
    return;
  }
  while(comp->status == COMP_STATUS_PENDING){
    _mm_pause();
  }
  furc_hash((const char *)dst_payload, query.size(), 16);
  return;
}

void non_blocking_offload(){
  struct completion_record * comp = (struct completion_record *)malloc(sizeof(struct completion_record));
  comp->status = COMP_STATUS_PENDING;
  compl_addr = (uint64_t)comp;
  submit_flag = OFFLOAD_REQUESTED;
  while(submit_flag.load() == OFFLOAD_REQUESTED){
    _mm_pause();
  }
}

void start_non_blocking_ax(pthread_t *ax_td, bool *ax_running_flag, uint64_t offload_duration, int max_inflights){
  *ax_running_flag = true;
  ax_params *params = (ax_params *)malloc(sizeof(ax_params));
  params->max_inflights = max_inflights;
  params->offload_time = offload_duration;
  params->ax_running = ax_running_flag;
  create_thread_pinned(ax_td, nonblocking_emul_ax, (void *)params, 0);

}

void stop_non_blocking_ax(pthread_t *ax_td, bool *ax_running_flag){
  *ax_running_flag = false;
  pthread_join(*ax_td, NULL);
}

int main(){
  gDebugParam = true;
  pthread_t ax_td;
  bool ax_running = true;
  int offload_time = 2100;
  start_non_blocking_ax(&ax_td, &ax_running, offload_time, 10);

  int num_requests = 1;
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_state_t *self = fcontext_create_proxy();
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;
  fcontext_transfer_t *filler_req_xfer;
  fcontext_state_t **filler_req_state;

  int next_unstarted_req_idx = 0;

  /* Pre-allocate the payloads */
  string query = "/region/cluster/foo:key|#|etc";
  dst_bufs = (char **)malloc(sizeof(char *) * num_requests);
  for(int i=0; i<num_requests; i++){
    dst_bufs[i] = (char *)malloc(sizeof(char) * query.size());
    memset(dst_bufs[i], 0, query.size());
  }

  /* Pre-allocate the CRs */
  comps = (ax_comp *)malloc(sizeof(ax_comp) * num_requests);

  /* Pre-allocate the request args */
  off_args = (offload_request_args **)
    malloc(sizeof(offload_request_args *) * num_requests);
  for(int i=0; i<num_requests; i++){
    off_args[i] = (offload_request_args *)malloc(sizeof(offload_request_args));
    off_args[i]->comp = &comps[i];
    off_args[i]->dst_payload = dst_bufs[i];
  }

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * num_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * num_requests);
  filler_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * num_requests);
  filler_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * num_requests);

  for(int i=0; i<num_requests; i++){
    off_req_state[i] = fcontext_create(router_request);
  }

  while(offloads_completed < num_requests){
    fcontext_swap(off_req_state[next_unstarted_req_idx]->context, off_args[next_unstarted_req_idx]);
  }

  stop_non_blocking_ax(&ax_td, &ax_running);

  return 0;
}