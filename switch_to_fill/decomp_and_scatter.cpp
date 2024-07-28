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

std::string gen_compressible_string(std::string payload){
  std::string append_string = payload;
  while(payload.size() < input_size){
    payload += append_string;
  }
  return payload;
}

void cpu_compressed_payload_allocator(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
  std::string payload = "/region/cluster/foo:key|#|etc";
  char *** ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = (char ***) malloc(total_requests * sizeof(char **));

  payload = gen_compressible_string(payload);

  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
  int num_inputs_per_request = 4;
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

    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i]);
  }
  free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
}

void offload_compressed_payload_allocator(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
  char *** ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = (char ***) malloc(total_requests * sizeof(char **));
  int num_inputs_per_request = 4;
  for(int i = 0; i < total_requests; i++){
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i] =
      (char **) malloc(sizeof(char *) * num_inputs_per_request); /* only one input to each request */
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0] =
      (char *) malloc(IAA_COMPRESS_MAX_DEST_SIZE);
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1] =
      (char *) malloc(IAA_COMPRESS_MAX_DEST_SIZE);
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2] =
      (char *) malloc(sizeof(int));
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][3] =
      (char *) malloc(num_accesses * sizeof(int));
  }
  *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
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

void alloc_offload_decomp_and_scatter_args( /* is this scatter ?*/
  int total_requests,
  timed_offload_request_args *** p_off_args,
  ax_comp *comps, uint64_t *ts0,
  uint64_t *ts1, uint64_t *ts2,
  uint64_t *ts3
){

  timed_offload_request_args **off_args =
    (timed_offload_request_args **)malloc(total_requests * sizeof(timed_offload_request_args *));

  std::string payload = gen_compressible_string("/region/cluster/foo:key|#|etc");
  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE;


  for(int i = 0; i < total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));

    /* comp'd source */
    off_args[i]->src_payload = (char *)malloc(input_size * sizeof(char));
    gpcore_do_compress((void *) off_args[i]->src_payload,
          (void *) payload.c_str(), payload.size(), &avail_out);

    LOG_PRINT(LOG_VERBOSE, "Compressed size: %d\n", avail_out);

    /* decompressed dst */
    off_args[i]->dst_payload = (char *)malloc(IAA_COMPRESS_MAX_DEST_SIZE);

    /* size forr decomp*/
    off_args[i]->src_size = avail_out;

    indirect_array_gen((int **)&(off_args[i]->aux_payload)); /* use dst buf to house the indirect array*/
    off_args[i]->src_size =  input_size;
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
    off_args[i]->comp = &comps[i];
    off_args[i]->id = i;
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
  }

  *p_off_args = off_args;
}

void free_offload_decomp_and_scatter_args(
  int total_requests,
  timed_offload_request_args *** p_off_args
){
  timed_offload_request_args **off_args = *p_off_args;
  for(int i = 0; i < total_requests; i++){
    free(off_args[i]->src_payload);
    free(off_args[i]->dst_payload);
    free(off_args[i]->aux_payload);
    free(off_args[i]->desc);
    free(off_args[i]);
  }
  free(off_args);
}

void blocking_decomp_and_scatter_request_stamped(
  fcontext_transfer_t arg){
  timed_offload_request_args *args =
      (timed_offload_request_args *) arg.data;

    char *src = args->src_payload;
    float *dst = (float *)(args->dst_payload);
    int *indir_arr = (int *)(args->aux_payload);
    int id = args->id;
    uint64_t *ts0 = args->ts0;
    uint64_t *ts1 = args->ts1;
    uint64_t *ts2 = args->ts2;
    uint64_t *ts3 = args->ts3;
    uint64_t src_size = args->src_size;
    uint64_t dst_size = args->dst_size;

    struct hw_desc *desc = args->desc;
    ax_comp *comp = args->comp;

    ts0[id] = sampleCoderdtsc();
    prepare_iaa_decompress_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)comp, (uint64_t)src_size);
    if(iaa_submit(iaa, desc) == false){
      LOG_PRINT(LOG_VERBOSE, "SoftwareFallback\n");

      int rc = gpcore_do_decompress((void *)dst, (void *)src, src_size, &dst_size);
      if(rc != 0){
        LOG_PRINT(LOG_ERR, "Error Decompressing\n");
      }
      comp->status = IAX_COMP_SUCCESS;
    }

    ts1[id] = sampleCoderdtsc();
    while(comp->status == IAX_COMP_NONE){
      _mm_pause();
    }
    if(comp->status != IAX_COMP_SUCCESS){
      LOG_PRINT(LOG_ERR, "Decompress failed: 0x%x\n", comp->status);
    }
    LOG_PRINT(LOG_VERBOSE, "Decompressed size: %d\n", comp->iax_output_size);

    ts2[id] = sampleCoderdtsc();

    for(int i=0; i<num_accesses; i++){
      dst[indir_arr[i]] = 1.0;
    }

    ts3[id] = sampleCoderdtsc();

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

  struct hw_desc *desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
  ax_comp *comp = (ax_comp *)malloc(sizeof(ax_comp));
  std::string str =
    gen_compressible_string("/region/cluster/foo:key|#|etc").c_str();
  char *src = (char *)malloc(str.size());
  memcpy(src, str.c_str(), str.size());

  float *dst = (float *)malloc(IAA_COMPRESS_MAX_DEST_SIZE);
  uLong dst_size = IAA_COMPRESS_MAX_DEST_SIZE;

  prepare_iaa_decompress_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)comp, (uint64_t)src);
  if (iaa_submit(iaa, desc) == false){
    LOG_PRINT(LOG_VERBOSE, "SoftwareFallback\n");

    int rc = gpcore_do_decompress((void *)dst, (void *)src, str.size(), &dst_size);
    if(rc != 0){
      LOG_PRINT(LOG_ERR, "Error Decompressing\n");
    }
    comp->status = IAX_COMP_SUCCESS;
  }

  while(comp->status == IAX_COMP_NONE){
      _mm_pause();
  }
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Decompress failed: 0x%x\n", comp->status);
  }
  LOG_PRINT(LOG_VERBOSE, "Decompressed size: %d\n", comp->iax_output_size);


  // run_gpcore_request_brkdown(
  //   cpu_decomp_and_scatter,
  //   cpu_compressed_payload_allocator,
  //   free_cpu_compressed_payloads,
  //   itr, total_requests
  // );

  // run_blocking_offload_request_brkdown(
  //   blocking_decomp_and_scatter_request_stamped,
  //   alloc_offload_decomp_and_scatter_args,
  //   free_offload_decomp_and_scatter_args,
  //   itr, total_requests
  // );

  free_iaa_wq();


}