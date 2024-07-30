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
#include "filler_antagonist.h"

#include "probe_point.h"
void antagonist_interleaved(fcontext_transfer_t arg);

int input_size = 16384; /* sizes the feature buffer */
int num_accesses = 10; /* tells number of accesses */
int *glob_indir_arr = NULL;
float *glob_dst_buf;

void scatter_interleaved(fcontext_transfer_t arg){
  volatile int glb = 0;
  init_probe(arg);
  fcontext_transfer_t parent_pointer;
  while(1){
    for(int i=0; i<num_accesses; i++){
      glob_dst_buf[glob_indir_arr[i]] = 1.0;
      probe_point();
    }
    LOG_PRINT( LOG_VERBOSE, "Antagonist finished one loop\n");
  }
}


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
  bool no_thrpt = false;


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
      case 'h':
        no_thrpt = true;
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

    glob_dst_buf = (float *)malloc(sizeof(float) * input_size); /* alloc filler's buf*/
    run_yielding_interleaved_request_brkdown(
      yielding_decomp_and_scatter_stamped,
      scatter_interleaved,
      alloc_offload_decomp_and_scatter_args_timed,
      free_offload_decomp_and_scatter_args_timed,
      itr,
      total_requests
    );
    run_yielding_interleaved_request_brkdown(
      yielding_decomp_and_scatter_stamped,
      antagonist_interleaved,
      alloc_offload_decomp_and_scatter_args_timed,
      free_offload_decomp_and_scatter_args_timed,
      itr,
      total_requests
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