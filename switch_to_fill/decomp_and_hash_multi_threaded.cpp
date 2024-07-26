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


int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(){


  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;
  int rc;
  int itr = 100;
  int total_requests = 1000;
  initialize_iaa_wq(dev_id, wq_id, wq_type);

  // int query_sizes[] = {31, 256, 1024, 4096, 16384, 65536}; // 10 x 10000
  // int query_sizes[] = {256 * 1024, 1024 * 1024}; // 10 x 100
  int query_sizes[] = {31, 256, 1024, 4096, 16384, 65536,
    256 * 1024, 1024 * 1024}; // 100 * 100
  std::string append_string = query;

  int query_size = 4096;
  while(query.size() < query_size){
    query += append_string;
  }

  for(int i=0; i<10; i++){
  // for (auto query_size : query_sizes){
    LOG_PRINT(LOG_PERF, "QuerySize %d\n", query_size);


    run_blocking_offload_request_brkdown(
      blocking_decompress_and_hash_request_stamped,
      alloc_decomp_and_hash_offload_args_stamped,
      free_decomp_and_hash_offload_args_stamped,
      itr,
      total_requests
    );

  }

  free_iaa_wq();
}