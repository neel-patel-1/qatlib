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
#include "dsa_offloads.h"
#include "submit.hpp"
#include "memcpy_dp_request.h"

#include "probe_point.h"
#include "filler_dp.h"
#include "filler_antagonist.h"

int input_size = 16384;

void (*input_populate)(char **);
void (*compute_on_input)(void *, int);

float *item_buf;
float *user_filler_buf;

static inline void input_gen_dp(char **p_buf){
  char *buf = (char *)malloc(input_size);
  for(int i = 0; i < input_size; i++){
    buf[i] = rand() % 256;
  }
  *p_buf = buf;
}


static inline void dot_product(void *user_buf, int bytes){
  float sum = 0;
  float *v1 = (float *)item_buf;
  float *v2 = (float *)user_buf;
  int size = bytes / sizeof(float);

  LOG_PRINT(LOG_DEBUG, "Dot Product\n");
  for(int i=0; i < size; i++){
    sum += v1[i] * v2[i];
  }
  LOG_PRINT(LOG_DEBUG, "Dot Product: %f\n", sum);

}

int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(int argc, char **argv){


  int wq_id = 0;
  int dev_id = 0;
  int wq_type = SHARED;
  int rc;
  int itr = 100;
  int total_requests = 1000;
  int opt;
  bool no_latency = false;
  bool no_thrpt = false;

  input_populate = input_gen_dp;
  compute_on_input = dot_product;

  while((opt = getopt(argc, argv, "t:i:r:s:q:d:h")) != -1){
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

  initialize_dsa_wq(dev_id, wq_id, wq_type);

  LOG_PRINT(LOG_PERF, "Input size: %d\n", input_size);
  if(!no_latency){
    run_gpcore_request_brkdown(
      cpu_memcpy_and_compute_stamped,
      alloc_cpu_memcpy_and_compute_args,
      free_cpu_memcpy_and_compute_args,
      itr,
      total_requests
    );
    run_blocking_offload_request_brkdown(
      blocking_memcpy_and_compute_stamped,
      alloc_offload_memcpy_and_compute_args,
      free_offload_memcpy_and_compute_args,
      itr,
      total_requests
    );
    run_yielding_interleaved_request_brkdown(
      yielding_memcpy_and_compute_stamped,
      dotproduct_interleaved,
      alloc_offload_memcpy_and_compute_args,
      free_offload_memcpy_and_compute_args,
      itr,
      total_requests
    );
    run_yielding_interleaved_request_brkdown(
      yielding_memcpy_and_compute_stamped,
      antagonist_interleaved,
      alloc_offload_memcpy_and_compute_args,
      free_offload_memcpy_and_compute_args,
      itr,
      total_requests
    );

    input_populate((char **)&user_filler_buf); /* populate a buffer for the filler to use for its dp */

  }

  int num_exe_time_samples_per_run = 10; /* 10 samples per iter*/

  if(! no_thrpt){
    run_gpcore_offeredLoad(
        cpu_memcpy_and_compute,
        alloc_cpu_memcpy_and_compute_args,
        free_cpu_memcpy_and_compute_args,
        itr,
        total_requests
      );


    run_blocking_offered_load(
      blocking_memcpy_and_compute,
      alloc_offload_memcpy_and_compute_args,
      free_offload_memcpy_and_compute_args,
      total_requests,
      itr
    );

    run_yielding_offered_load(
      yielding_memcpy_and_compute,
        alloc_offload_memcpy_and_compute_args,
        free_offload_memcpy_and_compute_args,
        num_exe_time_samples_per_run,
        total_requests,
        itr
    );
  }

  free_dsa_wq();
  return 0;

}