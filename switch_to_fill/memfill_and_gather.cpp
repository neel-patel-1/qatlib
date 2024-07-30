#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"
#include "iaa_offloads.h"
#include "ch3_hash.h"
#include "runners.h"
#include "gather_scatter.h"

extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
  #include <zlib.h>
}
#include "dsa_offloads.h"
#include "submit.hpp"
#include <algorithm>
#include "memfill_gather.h"
#include "gather_scatter.h"

int input_size = 100; /* sizes the feature buffer */
int num_accesses = 10; /* tells number of accesses */

void (*compute_on_input)(void *, int);

int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(int argc, char **argv){
  int wq_id = 0;
  int dev_id = 0;
  int wq_type = SHARED;
  int rc;
  int itr = 100;
  int total_requests = 1000;
  bool no_latency = false;
  bool no_thrpt = false;
  int opt;

  while((opt = getopt(argc, argv, "t:i:r:s:q:a:od:h")) != -1){
    switch(opt){
      case 't':
        total_requests = atoi(optarg);
        break;
      case 'i':
        itr = atoi(optarg);
        break;
      case 'q':
        input_size = atoi(optarg);
        break;
      case 'a':
        num_accesses = atoi(optarg);
        break;
      case 'o':
        no_latency = true;
        break;
      case 'd':
        dev_id = atoi(optarg);
        break;
      case 'h':
        no_thrpt = true;
        break;
      default:
        break;
    }
  }

  initialize_dsa_wq(dev_id, wq_id, wq_type);

  if(! no_latency ){
    run_gpcore_request_brkdown(
      cpu_memcpy_and_compute_stamped,
      alloc_cpu_memcpy_and_compute_args,
      free_cpu_memcpy_and_compute_args,
      itr, total_requests
    );

    run_blocking_offload_request_brkdown(
      blocking_memcpy_and_compute_stamped,
      alloc_offload_memfill_and_gather_args_timed,
      free_offload_memfill_and_gather_args,
      itr, total_requests
    );
  }
  run_gpcore_offeredLoad(
    cpu_memcpy_and_compute,
    alloc_cpu_memcpy_and_compute_args,
    free_cpu_memcpy_and_compute_args,
    itr, total_requests
  );
  run_blocking_offered_load(
    blocking_memcpy_and_compute,
    alloc_offload_memfill_and_gather_args,
    free_offload_memfill_and_gather_args,
    total_requests, itr
  );
  run_yielding_offered_load(
    yielding_memcpy_and_compute,
    alloc_offload_memfill_and_gather_args,
    free_offload_memfill_and_gather_args,
    10, total_requests, itr
  );

}