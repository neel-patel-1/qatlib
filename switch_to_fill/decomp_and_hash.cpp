#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"
#include "iaa_offloads.h"
#include "ch3_hash.h"
#include "runners.h"
#include "gpcore_compress.h"

extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
  #include <zlib.h>
}
#include "decompress_and_hash_request.hpp"
#include "probe_point.h"
#include "filler_hash.h"
#include "filler_antagonist.h"



void hash_interleaved(fcontext_transfer_t arg);
void antagonist_interleaved(fcontext_transfer_t arg);

int gLogLevel = LOG_PERF;
bool gDebugParam = false;

int input_size = 16384;
int main(int argc, char **argv){

  int wq_id = 0;
  int dev_id = 2;
  int wq_type = SHARED;
  int rc;
  int itr = 100;
  int total_requests = 1000;
  int opt;
  bool no_latency = false, no_thrpt = false;

  while((opt = getopt(argc, argv, "t:i:r:s:q:d:h")) != -1){
    switch(opt){
      case 't':
        total_requests = atoi(optarg);
        break;
      case 'i':
        itr = atoi(optarg);
        break;
      case 'o':
        no_latency = true;
        break;
      case 'h':
        no_thrpt = true;
        break;
      case 'q':
        input_size = atoi(optarg);
        break;
      case 'd':
        dev_id = atoi(optarg);
        break;
      default:
        break;
    }
  }
  initialize_iaa_wq(dev_id, wq_id, wq_type);

  std::string append_string = query;
  while(query.size() < input_size){
    query += append_string;
  }


  if(! no_latency){
    run_gpcore_request_brkdown(
      cpu_decompress_and_hash_stamped,
      compressed_mc_req_allocator,
      compressed_mc_req_free,
      itr,
      total_requests
    );
    run_blocking_offload_request_brkdown(
      blocking_decompress_and_hash_request_stamped,
      alloc_decomp_and_hash_offload_args_stamped,
      free_decomp_and_hash_offload_args_stamped,
      itr,
      total_requests
    );
    run_yielding_interleaved_request_brkdown(
      yielding_decompress_and_hash_request_stamped,
      hash_interleaved,
      alloc_decomp_and_hash_offload_args_stamped,
      free_decomp_and_hash_offload_args_stamped,
      itr,
      total_requests
    );
    run_yielding_interleaved_request_brkdown(
      yielding_decompress_and_hash_request_stamped,
      antagonist_interleaved,
      alloc_decomp_and_hash_offload_args_stamped,
      free_decomp_and_hash_offload_args_stamped,
      itr,
      total_requests
    );
  }

  if (! no_thrpt){
    int exetime_samples_per_run = 10;
    run_gpcore_offeredLoad(
      cpu_decompress_and_hash,
      compressed_mc_req_allocator,
      compressed_mc_req_free,
      itr,
      total_requests
    );
    run_blocking_offered_load(
      blocking_decompress_and_hash_request,
      alloc_decomp_and_hash_offload_args,
      free_decomp_and_hash_offload_args,
      total_requests,
      itr
    );
    run_yielding_offered_load(
      yielding_decompress_and_hash_request,
      alloc_decomp_and_hash_offload_args,
      free_decomp_and_hash_offload_args,
      exetime_samples_per_run,
      total_requests,
      itr
    );
  }

  free_iaa_wq();
}