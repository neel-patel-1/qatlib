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




enum runner_type {
  GPCORE,
  BLOCKING,
  YIELDING
};

int input_size;


int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(int argc, char **argv){

  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;
  int rc;
  int itr = 2;
  int total_requests = 100;
  int num_exe_time_samples_per_run = 10;
  int opt;
  enum runner_type run_typ = BLOCKING;
  int runner_num;
  int query_size = 64;

  initialize_iaa_wq(dev_id, wq_id, wq_type);

  while((opt = getopt(argc, argv, "t:i:r:s:q:")) != -1){
    switch(opt){
      case 't':
        total_requests = atoi(optarg);
        break;
      case 'i':
        itr = atoi(optarg);
        break;
      case 'r':
        runner_num = atoi(optarg);
        run_typ = (enum runner_type)runner_num;
        break;
      case 's':
        num_exe_time_samples_per_run = atoi(optarg);
        break;
      case 'q':
        input_size = atoi(optarg);
        break;
      default:
        break;
    }
  }

  switch(run_typ){
    case runner_type::GPCORE:
      run_gpcore_offeredLoad(
        cpu_decompress_and_hash,
        compressed_mc_req_allocator,
        compressed_mc_req_free,
        itr,
        total_requests
      );
      break;
    case runner_type::BLOCKING:
      run_blocking_offered_load(
        blocking_decompress_and_hash_request,
        alloc_decomp_and_hash_offload_args,
        free_decomp_and_hash_offload_args,
        total_requests,
        itr
      );
      break;
    case runner_type::YIELDING:
      run_yielding_offered_load(
        yielding_decompress_and_hash_request,
        alloc_decomp_and_hash_offload_args,
        free_decomp_and_hash_offload_args,
        num_exe_time_samples_per_run,
        total_requests,
        itr
      );
      break;
  }


  free_iaa_wq();
}