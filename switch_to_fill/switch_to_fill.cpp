#include "timer_utils.h"
#include <pthread.h>
#include "print_utils.h"
#include "status.h"
#include <cstdlib>
#include <atomic>
#include <list>
#include <cstring>
#include <x86intrin.h>

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
  uint64_t next_offload_completion_time = 0;
  int max_inflight = args->max_inflights;
  uint64_t offload_time = args->offload_time;
  int in_flight = 0;
  ax_comp *next_completed_offload_comp = NULL;
  bool *keep_running = args->ax_running;

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
        PRINT_DBG("Offload duration: %ld\n",
          cur_time - offload_in_flight_list.front()->start_time);
      }

      /*
        if we have any other offloads in flight
          make sure we let ourselves know
          make sure we start checking the current time against the time for the next earliest submitted offload
      */
      offload_in_flight_list.pop_front();
      if( ! offload_in_flight_list.empty() ){
        offload_in_flight = true;
        next_offload_completion_time = offload_in_flight_list.front()->comp_time;
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
        submit_status = SUBMIT_SUCCESS;
        struct completion_record *comp_addr = (struct completion_record *)compl_addr.load();
        submit_flag = OFFLOAD_RECEIVED; /*received submission*/

        offload_entry *new_ent = new offload_entry(start_time, start_time + offload_time, comp_addr);
        offload_in_flight_list.push_back(new_ent);
        in_flight++;
        offload_in_flight = true;
        PRINT_DBG("Offload accepted\n");
      } else {
        submit_status = SUBMIT_FAIL;
        submit_flag = OFFLOAD_RECEIVED; /*received submission*/
        PRINT_DBG("Failed to accept offload\n");
      }

    }
  }
  return NULL;
}

void blocking_offload(){
  struct completion_record * comp = (struct completion_record *)malloc(sizeof(struct completion_record));
  comp->status = COMP_STATUS_PENDING;
  compl_addr = (uint64_t)comp;
  submit_flag = OFFLOAD_REQUESTED;
  while(submit_flag.load() == OFFLOAD_REQUESTED){
    _mm_pause();
  }

  while(comp->status == COMP_STATUS_PENDING){
    _mm_pause();
  }
}

int main(){
  gDebugParam = true;
  pthread_t ax_td;

  bool ax_running = true;
  ax_params *params = (ax_params *)malloc(sizeof(ax_params));
  params->max_inflights = 2;
  params->offload_time = 2100;
  params->ax_running = &ax_running;
  create_thread_pinned(&ax_td, nonblocking_emul_ax, (void *)params, 0);

  blocking_offload();

  ax_running = false;

  pthread_join(ax_td, NULL);

  return 0;
}