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
#include "walker_requests.h"
/*
  blocking linked list merge request:
    (same as router)
    ll_simple
    ll_dynamic

*/

uint64_t run_gpcore_request_brkdown(fcontext_fn_t req_fn,
  void (*payload_alloc)(int,char****),
  void (*payload_free)(int,char****),
  int iter, int total_requests)
{
  uint64_t *exetime;
  uint64_t *kernel1, *kernel2;
  uint64_t kernel1_time;
  exetime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  kernel1 = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  kernel2 = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++){
    gpcore_request_breakdown(
      req_fn,
      allocate_posting_lists,
      free_posting_lists,
      total_requests,
      kernel1, kernel2, i
    );
  }
  print_mean_median_stdev(kernel1, iter, "Kernel1");
  print_mean_median_stdev(kernel2, iter, "Kernel2");
  kernel1_time = median_from_array(kernel1, iter);
  return kernel1_time;
}

void run_blocking_offload_request_brkdown(fcontext_fn_t req_fn,
  void (*payload_alloc)(int,char***),
  void (*payload_free)(int,char***),
  int iter, int total_requests)
{
  uint64_t *offloadtime;
  uint64_t *waittime, *posttime;
  offloadtime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  waittime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  posttime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++){
    blocking_ax_request_breakdown(
      req_fn,
      payload_alloc,
      payload_free,
      total_requests,
      offloadtime, waittime, posttime, i
    );
  }
  print_mean_median_stdev(offloadtime, iter, "Offload");
  print_mean_median_stdev(waittime, iter, "Wait");
  print_mean_median_stdev(posttime, iter, "Post");
}

void run_yielding_request_brkdown(fcontext_fn_t req_fn,
  void (*payload_alloc)(int,char***),
  void (*payload_free)(int,char***),
  int iter, int total_requests)
{
  uint64_t *offloadtime;
  uint64_t *waittime, *posttime;
  offloadtime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  waittime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  posttime = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++){
    yielding_request_breakdown(
      req_fn,
      payload_alloc,
      payload_free,
      total_requests,
      offloadtime, waittime, posttime, i
    );
  }
  print_mean_median_stdev(offloadtime, iter, "OffloadSwToFill");
  print_mean_median_stdev(waittime, iter, "YieldToResumeLatency");
  print_mean_median_stdev(posttime, iter, "PostSwToFill");
}

void run_gpcore_blocking_swtofill_simple_rank(int new_pl,
  int total_requests, int sampling_interval, int iter){
  pl_len = new_pl;
  uint64_t cpu_intersect_time;
  cpu_intersect_time =
    run_gpcore_request_brkdown(
      cpu_simple_ranker_request_stamped,
      allocate_posting_lists,
      free_posting_lists,
      100, 1000
    );

  int max_inflight = 128;
  pthread_t ax_td;
  bool ax_running = true;
  start_non_blocking_ax(&ax_td, &ax_running, cpu_intersect_time, max_inflight);

  run_blocking_offload_request_brkdown(
    blocking_simple_ranker_request_stamped,
    allocate_pre_intersected_posting_lists_llc,
    free_pre_intersected_posting_lists_llc,
    100, 1000
  );

  run_yielding_request_brkdown(
    yielding_simple_ranker_request_stamped,
    allocate_pre_intersected_posting_lists_llc,
    free_pre_intersected_posting_lists_llc,
    100, 1000
  );

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
  mean_median_stdev_rps(cpu_exe_time, iter, total_requests, "GPCoreRPS");

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
  mean_median_stdev_rps(blocking_exe_time, iter, total_requests, "BlockingOffloadRPS");


  uint64_t *yielding_exe_time;
  yielding_exe_time = (uint64_t *)malloc(sizeof(uint64_t) * iter);
  for(int i=0; i<iter; i++){
    yielding_offload_ax_closed_loop_test(
      yielding_simple_ranker_request,
      allocate_pre_intersected_posting_lists_llc,
      free_pre_intersected_posting_lists_llc,
      sampling_interval, total_requests, yielding_exe_time, i
    );
  }
  mean_median_stdev_rps(yielding_exe_time, iter, total_requests, "SwitchToFillRPS");

  stop_non_blocking_ax(&ax_td, &ax_running);

}

int gLogLevel = LOG_MONITOR;
bool gDebugParam = true;
extern int pl_len;
int main(){
  pthread_t ax_td;
  bool ax_running = true;
  int offload_time = 16313;
  int max_inflight = 128;

  int iter = 1;
  int sampling_interval = 1000;
  int total_requests = 1000;

  uint64_t cpu_intersect_time;

  for (pl_len = 32; pl_len<=512; pl_len*=2){
    printf("PL_LEN: %d\n", pl_len);
    run_gpcore_blocking_swtofill_simple_rank(pl_len, total_requests, sampling_interval, iter);
  }

}