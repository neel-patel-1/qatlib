#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"
#include "iaa_offloads.h"
#include "ch3_hash.h"
#include "runners.h"
#include "gpcore_compress.h"
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
#include "decompress_and_scatter_request.h"

int input_size = 16384; /* sizes the feature buffer */
int num_accesses = 10; /* tells number of accesses */


int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(int argc, char **argv){
  int wq_id = 0;
  int dev_id = 1;
  int wq_type = SHARED;
  int rc;
  int itr = 1;
  int total_requests = 1;
  int opt;
  bool no_latency = false;

  while((opt = getopt(argc, argv, "t:i:r:s:q:a:od:")) != -1){
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
      default:
        break;
    }
  }

  initialize_iaa_wq(dev_id, wq_id, wq_type);

  std::string append_string = payload;
  while(payload.size() < input_size){
    payload += append_string;
  }

  if(! no_latency ){
    run_gpcore_request_brkdown(
      cpu_decomp_and_scatter_stamped,
      cpu_compressed_payload_allocator,
      free_cpu_compressed_payloads,
      itr, total_requests
    );

    run_blocking_offload_request_brkdown(
      blocking_decomp_and_scatter_request_stamped,
      alloc_offload_decomp_and_scatter_args_timed,
      free_offload_decomp_and_scatter_args_timed,
      itr, total_requests
    );
  }

  run_gpcore_offeredLoad(
    cpu_decomp_and_scatter,
    cpu_compressed_payload_allocator,
    free_cpu_compressed_payloads,
    itr, total_requests
  );

  run_blocking_offered_load(
    blocking_decomp_and_scatter_request,
    alloc_offload_decomp_and_scatter_args,
    free_offload_decomp_and_scatter_args,
    total_requests, itr
  );

  run_yielding_offered_load(
    yielding_decomp_and_scatter,
    alloc_offload_decomp_and_scatter_args,
    free_offload_decomp_and_scatter_args,
    10, total_requests, itr
  );

  free_iaa_wq();


}