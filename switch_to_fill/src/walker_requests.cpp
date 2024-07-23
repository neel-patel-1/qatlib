#include "walker_requests.h"
#include "posting_list.h"
#include "timer_utils.h"
#include "test_harness.h"
#include "status.h"

extern uint64_t total_offloads;


void cpu_simple_ranker_request_stamped(fcontext_transfer_t arg){
  timed_gpcore_request_args *args = (timed_gpcore_request_args *)arg.data;
  node *plist1_head = (node *)(args->inputs[0]);
  node *plist2_head = (node *)(args->inputs[1]);
  node *result_head = (node *)(args->inputs[2]);

  int id = args->id;

  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  ts0[id] = sampleCoderdtsc();
  intersect_posting_lists(result_head, plist1_head, plist2_head);
  ts1[id] = sampleCoderdtsc();

  ll_simple(result_head);
  ts2[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void cpu_simple_ranker_request(fcontext_transfer_t arg){
  gpcore_request_args *args = (gpcore_request_args *)arg.data;
  node *plist1_head = (node *)(args->inputs[0]);
  node *plist2_head = (node *)(args->inputs[1]);
  node *result_head = (node *)(args->inputs[2]);

  intersect_posting_lists(result_head, plist1_head, plist2_head);

  ll_simple(result_head);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void blocking_simple_ranker_request_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;
  node *head = (node *)(args->dst_payload);
  ax_comp *comp = args->comp;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  int id = args->id;

  ts0[id] = sampleCoderdtsc();
  int status = submit_offload(comp, (char *)head);
  if(status == STATUS_FAIL){
    return;
  }
  ts1[id] = sampleCoderdtsc();

  while(comp->status == COMP_STATUS_PENDING){
    _mm_pause();
  }
  ts2[id] = sampleCoderdtsc();

  ll_simple(head);
  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

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

void yielding_simple_ranker_request(fcontext_transfer_t arg){
  offload_request_args *args = (offload_request_args *)arg.data;
  node *head = (node *)args->dst_payload;
  ax_comp *comp = args->comp;
  int id = args->id;

  int status = submit_offload(comp, (char *)head);
  if(status == STATUS_FAIL){
    return;
  }
  fcontext_swap(arg.prev_context, NULL);

  ll_simple(head);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void yielding_simple_ranker_request_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;
  node *head = (node *)args->dst_payload;
  ax_comp *comp = args->comp;
  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;

  uint64_t offloads_before_yielding;

  if(gLogLevel >= LOG_MONITOR)
  {
    offloads_before_yielding = totalOffloads;
  }

  ts0[id] = sampleCoderdtsc();
  int status = submit_offload(comp, (char *)head);
  if(status == STATUS_FAIL){
    return;
  }
  ts1[id] = sampleCoderdtsc();

  fcontext_swap(arg.prev_context, NULL);

  /* when ctx switch back (our offload completes), how many requests have started their offload phase */
  /* emul_ax exports "total_offloads" <- total offloads started */
  LOG_PRINT(LOG_MONITOR, "%ld Requests Offloaded After %d Requests Offload Duration\n",
    totalOffloads - offloads_before_yielding, id);


  ts2[id] = sampleCoderdtsc();
  ll_simple(head);
  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}