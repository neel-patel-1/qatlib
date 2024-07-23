#include "emul_ax.h"
#include "print_utils.h"
#include "timer_utils.h"
#include <list>

std::atomic<int> submit_flag;
std::atomic<int> submit_status;
std::atomic<uint64_t> compl_addr;
std::atomic<uint64_t> p_dst_buf;
std::atomic<uint64_t> total_offloads;


extern bool gDebugParam;

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
  uint64_t totalOffloads = 0;
  uint64_t offloadDurationSum = 0;
  uint64_t rejected_offloads = 0;

  if(LOG_MONITOR == gLogLevel){ /* export total offloads for curious requestors if monitoring is enabled*/
    total_offloads = 0;
  }

  submit_flag = OFFLOAD_RECEIVED;

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

        if(gLogLevel == LOG_MONITOR){ /* export total offloads for curious requestors if monitoring is enabled*/
          total_offloads++;
        }
      } else {
        submit_status = SUBMIT_FAIL;
        submit_flag = OFFLOAD_RECEIVED; /*received submission*/
        rejected_offloads++;
      }

    }
  }

  if(gLogLevel == LOG_PERF){
    if(totalOffloads > 0){
      LOG_PRINT(LOG_PERF, "Avg_Offload_Duration: %ld Rejected_Offloads: %ld\n",
              offloadDurationSum / totalOffloads, rejected_offloads );
    }
  }
  return NULL;
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