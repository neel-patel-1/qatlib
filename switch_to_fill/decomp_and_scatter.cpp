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

int input_size = 16384; /* sizes the feature buffer */
int num_accesses = 10; /* tells number of accesses */

void cpu_compressed_payload_allocator(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
  std::string payload = "/region/cluster/foo:key|#|etc";
  char *** ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = (char ***) malloc(total_requests * sizeof(char **));
  std::string append_string = payload;
  while(payload.size() < input_size){
    payload += append_string;
  }

  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
  int num_inputs_per_request = 6;
  for(int i = 0; i < total_requests; i++){
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i] =
      (char **) malloc(sizeof(char *) * num_inputs_per_request); /* only one input to each request */

    /* CPU request needs a src payload to decompress */
    avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0] =
      (char *) malloc(avail_out);
    gpcore_do_compress((void *) (ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]),
      (void *) payload.c_str(), payload.size(), &avail_out);

    LOG_PRINT(LOG_VERBOSE, "Compressed size: %d\n", avail_out);

    /* and a dst payload to decompress into */
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1] =
      (char *) malloc(IAA_COMPRESS_MAX_DEST_SIZE);

    /* and an indirection array */
    indirect_array_gen((int **)&(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][3]));

    /* and the size of the compressed payload for decomp */
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2] =
      (char *) malloc(sizeof(int));
    *(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]) = avail_out;

    /* and the number of accesses to make */
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][4] =
      (char *) malloc(sizeof(int));
    *(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][4]) = num_accesses;

    /* and the size we should expect for the decomp'd buf */
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][5] =
      (char *) malloc(sizeof(int));

    *(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][5]) = input_size;
  }

  *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
}

void free_cpu_compressed_payloads(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
  char ***ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
  for(int i = 0; i < total_requests; i++){
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]);
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1]);
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]);
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][3]);
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][4]);

    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i]);
  }
  free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
}

void cpu_decomp_and_scatter(
  fcontext_transfer_t arg
){
  timed_gpcore_request_args *args = (timed_gpcore_request_args *)arg.data;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  char *src1 = args->inputs[0];
  float *dst1 = (float *)(args->inputs[1]);
  int compressed_size = *((int *) args->inputs[2]);
  int *indir_arr = (int *) args->inputs[3];
  int num_accesses = *((int *) args->inputs[4]);
  int decomp_size = *((int *) args->inputs[5]);

  uLong decompressed_size = IAA_COMPRESS_MAX_DEST_SIZE;
    /* dst is provisioned by allocator to have max dest size */

  ts0[id] = sampleCoderdtsc();
  int rc = gpcore_do_decompress(dst1, src1, compressed_size, &decompressed_size);
  if(rc != 0){
    LOG_PRINT(LOG_ERR, "Error Decompressing\n");
  }
  ts1[id] = sampleCoderdtsc();

  for(int i=0; i<num_accesses; i++){
    dst1[indir_arr[i]] = 1.0;
  }
  ts2[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}


int gLogLevel = LOG_DEBUG;
bool gDebugParam = false;
int main(int argc, char **argv){
  int wq_id = 0;
  int dev_id = 2;
  int wq_type = SHARED;
  int rc;
  int itr = 10;
  int total_requests = 100;
  int opt;

  while((opt = getopt(argc, argv, "t:i:r:s:q:a:")) != -1){
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
      default:
        break;
    }
  }

  initialize_iaa_wq(dev_id, wq_id, wq_type);

  run_gpcore_request_brkdown(
    cpu_decomp_and_scatter,
    cpu_compressed_payload_allocator,
    free_cpu_compressed_payloads,
    itr, total_requests
  );

  free_iaa_wq();


}