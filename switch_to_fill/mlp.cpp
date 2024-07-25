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

int input_size = 16384;

void initialize_dsa_wq(int dev_id, int wq_id, int wq_type){
  int tflags = TEST_FLAGS_BOF;
  int rc;

  iaa = acctest_init(tflags);
  rc = acctest_alloc(iaa, wq_type, dev_id, wq_id);
  if(rc != ACCTEST_STATUS_OK){
    LOG_PRINT( LOG_ERR, "Error allocating work queue\n");
    return;
  }
}

void free_dsa_wq(){
  acctest_free(iaa);
}

static inline void input_gen(char **p_buf){
  char *buf = (char *)malloc(input_size);
  for(int i = 0; i < input_size; i++){
    buf[i] = rand() % 256;
  }
  *p_buf = buf;
}


static inline void compute(void *buf, int size){
  int i;
  char *p = (char *)buf;
  for(i = 0; i < size; i++){
    p[i] = p[i] + 1;
  }
}

void alloc_cpu_memcpy_and_compute_args(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
    char ***ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = (char ***)malloc(total_requests * sizeof(char **));
    for(int i = 0; i < total_requests; i++){
      ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i] = (char **)malloc(3 * sizeof(char *));
      // ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0] = (char *)malloc(input_size * sizeof(char));
      input_gen(&ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]);
      ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1] = (char *)malloc(input_size * sizeof(char));
      ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2] = (char *)malloc(sizeof(int));
      *((int *) ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]) = input_size;
    }

    *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
}

void free_cpu_memcpy_and_compute_args(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
    char *** ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
    for(int i = 0; i < total_requests; i++){
      free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]);
      free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1]);
      free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]);
      free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i]);
    }
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
}

void cpu_memcpy_and_compute_stamped(fcontext_transfer_t arg){
  timed_gpcore_request_args* args = (timed_gpcore_request_args *)arg.data;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  char *src1 = args->inputs[0];
  char *dst1 = args->inputs[1];
  int buf_size = *((int *) args->inputs[2]);

  ts0[id] = sampleCoderdtsc();
  memcpy(dst1, src1, buf_size);
  ts1[id] = sampleCoderdtsc();

  compute(dst1, buf_size);
  ts2[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

// void alloc_offload_memcpy_and_compute_args(
//   int total_requests,
//   timed_offload_request_args*** p_off_args,
//   ax_comp *comps,
//   uint64_t *ts0,
//   uint64_t *ts1,
//   uint64_t *ts2,
//   uint64_t *ts3
// ){

//   timed_offload_request_args **off_args = (timed_offload_request_args **)malloc(total_requests * sizeof(timed_offload_request_args *));
//   for(int i = 0; i < total_requests; i++){
//     off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
//     off_args[i]->src_payload = (char *)malloc(input_size * sizeof(char));
//     off_args[i]->dst_payload = (char *)malloc(input_size * sizeof(char));
//     off_args[i]->inputs[2] = (char *)malloc(sizeof(int));
//     *((int *) off_args[i]->inputs[2]) = input_size;
//     off_args[i]->ts0 = ts0;
//     off_args[i]->ts1 = ts1;
//     off_args[i]->ts2 = ts2;
//     off_args[i]->ts3 = ts3;
//     off_args[i]->id = i;
//   }

//   *p_off_args = off_args;
// }

// void free_offload_memcpy_and_compute_args(
//   int total_requests,
//   timed_offload_request_args*** p_off_args
// ){
//   timed_offload_request_args **off_args = *p_off_args;
//   for(int i = 0; i < total_requests; i++){
//     free(off_args[i]->inputs[0]);
//     free(off_args[i]->inputs[1]);
//     free(off_args[i]->inputs[2]);
//     free(off_args[i]->inputs);
//     free(off_args[i]);
//   }
//   free(off_args);
// }



int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(){


  int wq_id = 0;
  int dev_id = 2;
  int wq_type = SHARED;
  int rc;
  int itr = 10;
  int total_requests = 100;
  initialize_dsa_wq(dev_id, wq_id, wq_type);

  run_gpcore_request_brkdown(
    cpu_memcpy_and_compute_stamped,
    alloc_cpu_memcpy_and_compute_args,
    free_cpu_memcpy_and_compute_args,
    itr,
    total_requests
  );
  // run_blocking_offload_request_brkdown(
  //   blocking_decompress_and_hash_request_stamped,
  //   alloc_decomp_and_hash_offload_args,
  //   free_decomp_and_hash_offload_args,
  //   itr,
  //   total_requests
  // );
  // run_yielding_request_brkdown(
  //   yielding_decompress_and_hash_request_stamped,
  //   alloc_decomp_and_hash_offload_args,
  //   free_decomp_and_hash_offload_args,
  //   itr,
  //   total_requests
  // );

  free_dsa_wq();
}