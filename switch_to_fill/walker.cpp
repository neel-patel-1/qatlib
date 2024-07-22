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

typedef struct _ranker_offload_args{
  node *list_head;
  ax_comp *comp;
} ranker_offload_args;


/*
  blocking linked list merge request:
    (same as router)
    ll_simple
    ll_dynamic

*/
void blocking_simple_ranker_request(ranker_offload_args *arg){
  ranker_offload_args *args = (ranker_offload_args *)arg;
  node *head = args->list_head; /* get, but wait to touch */
  ax_comp *comp = args->comp;

  int status = submit_offload(comp, (char *)head);
  if(status == STATUS_FAIL){
    return;
  }
  while(comp->status == COMP_STATUS_PENDING){
    _mm_pause();
  }

  ll_simple(head);
}





int gLogLevel = LOG_DEBUG;
bool gDebugParam = true;
int main(){
  pthread_t ax_td;
  bool ax_running = true;
  int offload_time = 1200;
  int max_inflight = 128;

  start_non_blocking_ax(&ax_td, &ax_running, offload_time, max_inflight);

  node *head = build_llc_ll(10);
  ranker_offload_args *args = (ranker_offload_args *)malloc(sizeof(ranker_offload_args));
  args->list_head = head;

  ax_comp *comp = (ax_comp *)malloc(sizeof(ax_comp));
  comp->status = COMP_STATUS_PENDING;
  args->comp = comp;

  blocking_simple_ranker_request(args);

  free_ll(head);

  stop_non_blocking_ax(&ax_td, &ax_running);

}