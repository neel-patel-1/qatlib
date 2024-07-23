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
#include "timer_utils.h"
#include "stats.h"
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

void blocking_simple_ranker_request_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;
  node *head = (node *)args->dst_payload;
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


int gLogLevel = LOG_PERF;
bool gDebugParam = true;
extern int pl_len;
int main(){
  pthread_t ax_td;
  bool ax_running = true;
  int offload_time = 16313;
  int max_inflight = 128;

  int iter = 10;
  int sampling_interval = 1000;
  int total_requests = 1000;

  pl_len = 128;
  uint64_t *exetime;
  uint64_t *kernel1, *kernel2;
  exetime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  kernel1 = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  kernel2 = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++){
    gpcore_request_breakdown(
      cpu_simple_ranker_request_stamped,
      allocate_posting_lists,
      free_posting_lists,
      total_requests,
      kernel1, kernel2, i
    );
  }
  uint64_t kernel1mean = avg_from_array(kernel1, iter);
  uint64_t kernel1median = median_from_array(kernel1, iter);
  uint64_t kernel1stddev = stddev_from_array(kernel1, iter);
  LOG_PRINT( LOG_PERF, "Kernel1 Mean: %lu Median: %lu Stddev: %lu\n", kernel1mean, kernel1median, kernel1stddev);
  uint64_t kernel2mean = avg_from_array(kernel2, iter);
  uint64_t kernel2median = median_from_array(kernel2, iter);
  uint64_t kernel2stddev = stddev_from_array(kernel2, iter);
  LOG_PRINT( LOG_PERF, "Kernel2 Mean: %lu Median: %lu Stddev: %lu\n", kernel2mean, kernel2median, kernel2stddev);


  uint64_t *offloadtime;
  uint64_t *waittime, *posttime;
  offloadtime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  waittime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  posttime = (uint64_t *)malloc(sizeof(uint64_t) * iter);

  offload_time = median_from_array(kernel1, iter); /* walker takes same amount of time*/

  start_non_blocking_ax(&ax_td, &ax_running, offload_time, max_inflight);

  for(int i=0; i<iter; i++){
    blocking_ax_request_breakdown(
      blocking_simple_ranker_request_stamped,
      allocate_pre_intersected_posting_lists_llc,
      free_pre_intersected_posting_lists_llc,
      total_requests,
      offloadtime, waittime, posttime, i
    );
  }
  print_mean_median_stdev(offloadtime, iter, "Offload");
  print_mean_median_stdev(waittime, iter, "Wait");
  print_mean_median_stdev(posttime, iter, "Post");

  uint64_t *cpu_exe_time;
  cpu_exe_time = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++){
    gpcore_closed_loop_test(
      cpu_simple_ranker_request,
      allocate_posting_lists,
      free_posting_lists,
      sampling_interval, total_requests, cpu_exe_time, i
    );
  }
  print_mean_median_stdev(cpu_exe_time, iter, "CPUExecution");
  double exetimemean = avg_from_array(cpu_exe_time, iter);
  double rpsmean = (double)total_requests / (exetimemean / 2100000000.0);
  LOG_PRINT( LOG_PERF, "CPU RPS Mean: %f\n", rpsmean);

  uint64_t *blocking_exe_time;
  blocking_exe_time = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++){
    blocking_ax_closed_loop_test(
      blocking_simple_ranker_request,
      allocate_pre_intersected_posting_lists_llc,
      free_pre_intersected_posting_lists_llc,
      sampling_interval, total_requests, blocking_exe_time, i
    );
  }
  print_mean_median_stdev(blocking_exe_time, iter, "BlockingExecution");
  exetimemean = avg_from_array(blocking_exe_time, iter);
  rpsmean = (double)total_requests / (exetimemean / 2100000000.0);
  LOG_PRINT( LOG_PERF, "Blocking RPS Mean: %f\n", rpsmean);

  stop_non_blocking_ax(&ax_td, &ax_running);

}