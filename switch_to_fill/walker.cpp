#include "timer_utils.h"
#include <pthread.h>
#include "print_utils.h"
#include "status.h"
#include "emul_ax.h"
#include "thread_utils.h"
#include "offload.h"
extern "C"{
#include "fcontext.h"
}
#include <cstdlib>
#include <atomic>
#include <list>
#include <cstring>
#include <x86intrin.h>
#include <string>


#include "posting_list.h"
#include "test_harness.h"
/*
  blocking linked list merge request:
    (same as router)
    ll_simple
    ll_dynamic

*/
void blocking_simple_ranker_request(fcontext_transfer_t arg){
  offload_request_args *args = (offload_request_args *)arg.data;
  node *head = (node *)args->dst_payload;
  ax_comp *comp = args->comp;
  int id = args->id;

  int status = submit_offload(comp, (char *)head);
  if(status == STATUS_FAIL){
    return;
  }
  while(comp->status == COMP_STATUS_PENDING){
    _mm_pause();
  }

  ll_simple(head);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void cpu_ranker_request(fcontext_transfer_t arg){
  gpcore_request_args *args = (gpcore_request_args *)arg.data;
  node *plist1_head = (node *)(args->inputs[0]);
  node *plist2_head = (node *)(args->inputs[1]);

  ll_simple(plist1_head);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}


int gLogLevel = LOG_DEBUG;
bool gDebugParam = true;
int main(){
  pthread_t ax_td;
  bool ax_running = true;
  int offload_time = 1200;
  int max_inflight = 128;

  int sampling_interval = 1000;
  int total_requests = 10000;
  uint64_t *exetime;

  start_non_blocking_ax(&ax_td, &ax_running, offload_time, max_inflight);

  exetime = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  gpcore_closed_loop_test(
    cpu_ranker_request,
    allocate_posting_lists,
    free_posting_lists ,
    sampling_interval, total_requests, exetime, 0
  );
  // blocking_ax_closed_loop_test(
  //   blocking_simple_ranker_request,
  //   allocate_pre_intersected_posting_lists_llc,
  //   free_pre_intersected_posting_lists_llc,
  //   total_requests, total_requests, exetime, 0
  // );

  stop_non_blocking_ax(&ax_td, &ax_running);

}