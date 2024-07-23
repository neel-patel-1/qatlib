#include "runners.h"

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
      payload_alloc,
      payload_free,
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