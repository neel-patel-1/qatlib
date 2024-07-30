#include "decompress_and_scatter_request.h"
#include "payload_gen.h"
#include "wait.h"


std::string payload = "/region/cluster/foo:key|#|etc";

void yielding_decomp_and_scatter_stamped(
  fcontext_transfer_t arg){
  timed_offload_request_args *args =
      (timed_offload_request_args *) arg.data;

    char *src = args->src_payload;
    float *dst = (float *)(args->dst_payload);
    int *indir_arr = (int *)(args->aux_payload);
    int id = args->id;
    uint64_t src_size = args->src_size;
    uint64_t dst_size = args->dst_size;
    uint64_t *ts0 = args->ts0;
    uint64_t *ts1 = args->ts1;
    uint64_t *ts2 = args->ts2;
    uint64_t *ts3 = args->ts3;

    struct hw_desc *desc = args->desc;
    ax_comp *comp = args->comp;

    ts0[id] = sampleCoderdtsc();
    prepare_iaa_decompress_desc_with_preallocated_comp(
      desc, (uint64_t)src, (uint64_t)dst,
      (uint64_t)comp, (uint64_t)src_size);
    blocking_iaa_submit(iaa, desc);

    ts1[id] = sampleCoderdtsc();
    fcontext_swap(arg.prev_context, NULL);

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

void yielding_decomp_and_scatter(
  fcontext_transfer_t arg){
  offload_request_args *args =
      (offload_request_args *) arg.data;

    char *src = args->src_payload;
    float *dst = (float *)(args->dst_payload);
    int *indir_arr = (int *)(args->aux_payload);
    int id = args->id;
    uint64_t src_size = args->src_size;
    uint64_t dst_size = args->dst_size;

    struct hw_desc *desc = args->desc;
    ax_comp *comp = args->comp;

    prepare_iaa_decompress_desc_with_preallocated_comp(
      desc, (uint64_t)src, (uint64_t)dst,
      (uint64_t)comp, (uint64_t)src_size);
    blocking_iaa_submit(iaa, desc);
    fcontext_swap(arg.prev_context, NULL);


    if(comp->status != IAX_COMP_SUCCESS){
      LOG_PRINT(LOG_ERR, "Decompress failed: 0x%x\n", comp->status);
    }
    LOG_PRINT(LOG_VERBOSE, "Decompressed size: %d\n", comp->iax_output_size);

    for(int i=0; i<num_accesses; i++){
      dst[indir_arr[i]] = 1.0;
    }

    requests_completed ++;
    fcontext_swap(arg.prev_context, NULL);
}

void blocking_decomp_and_scatter_request(
  fcontext_transfer_t arg){
  offload_request_args *args =
      (offload_request_args *) arg.data;

    char *src = args->src_payload;
    float *dst = (float *)(args->dst_payload);
    int *indir_arr = (int *)(args->aux_payload);
    int id = args->id;
    uint64_t src_size = args->src_size;
    uint64_t dst_size = args->dst_size;

    struct hw_desc *desc = args->desc;
    ax_comp *comp = args->comp;

    prepare_iaa_decompress_desc_with_preallocated_comp(
      desc, (uint64_t)src, (uint64_t)dst,
      (uint64_t)comp, (uint64_t)src_size);
    blocking_iaa_submit(iaa, desc);

    spin_on(comp);
    if(comp->status != IAX_COMP_SUCCESS){
      LOG_PRINT(LOG_ERR, "Decompress failed: 0x%x\n", comp->status);
    }
    LOG_PRINT(LOG_VERBOSE, "Decompressed size: %d\n", comp->iax_output_size);

    for(int i=0; i<num_accesses; i++){
      dst[indir_arr[i]] = 1.0;
    }


    requests_completed ++;
    fcontext_swap(arg.prev_context, NULL);
}

void blocking_decomp_and_scatter_request_stamped(
  fcontext_transfer_t arg){
  timed_offload_request_args *args =
      (timed_offload_request_args *) arg.data;

    char *src = args->src_payload;
    float *dst = (float *)(args->dst_payload);
    int *indir_arr = (int *)(args->aux_payload);
    int id = args->id;
    uint64_t pre_submit, pre_wait, pre_scatter, post_scatter;
    uint64_t *ts0 = args->ts0;
    uint64_t *ts1 = args->ts1;
    uint64_t *ts2 = args->ts2;
    uint64_t *ts3 = args->ts3;
    uint64_t src_size = args->src_size;
    uint64_t dst_size = args->dst_size;

    struct hw_desc *desc = args->desc;
    ax_comp *comp = args->comp;

    pre_submit = sampleCoderdtsc();

    prepare_iaa_decompress_desc_with_preallocated_comp(
      desc, (uint64_t)src, (uint64_t)dst,
      (uint64_t)comp, (uint64_t)src_size);
    blocking_iaa_submit(iaa, desc);

    pre_wait = sampleCoderdtsc();
    spin_on(comp);
    if(comp->status != IAX_COMP_SUCCESS){
      LOG_PRINT(LOG_ERR, "Decompress failed: 0x%x\n", comp->status);
    }
    LOG_PRINT(LOG_VERBOSE, "Decompressed size: %d\n", comp->iax_output_size);

    pre_scatter = sampleCoderdtsc();
    for(int i=0; i<num_accesses; i++){
      dst[indir_arr[i]] = 1.0;
    }

    post_scatter = sampleCoderdtsc();

    requests_completed ++;
    ts0[id] = pre_submit;
    ts1[id] = pre_wait;
    ts2[id] = pre_scatter;
    ts3[id] = post_scatter;
    fcontext_swap(arg.prev_context, NULL);
}

void free_offload_decomp_and_scatter_args(
  int total_requests,
  offload_request_args *** p_off_args
){
  offload_request_args **off_args = *p_off_args;
  for(int i = 0; i < total_requests; i++){
    free(off_args[i]->src_payload);
    free(off_args[i]->dst_payload);
    free(off_args[i]->desc);
    free(off_args[i]);
  }
  free(off_args);
  free(glob_indir_arr);
  glob_indir_arr = NULL;
}

void alloc_offload_decomp_and_scatter_args(int total_requests,
  offload_request_args *** p_off_args,
  ax_comp *comps){
  offload_request_args **off_args =
    (offload_request_args **)malloc(total_requests * sizeof(offload_request_args *));

  if(glob_indir_arr == NULL){
    /* gen the indir arr */
    indirect_array_gen(&glob_indir_arr);

  }

  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
  for(int i = 0; i < total_requests; i++){
    off_args[i] = (offload_request_args *)malloc(sizeof(offload_request_args));

    off_args[i]->comp = &(comps[i]);
    off_args[i]->id = i;

    /* comp'd source */
    avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
    off_args[i]->src_payload = (char *)malloc(input_size * sizeof(char));
    gpcore_do_compress((void *) off_args[i]->src_payload,
          (void *) payload.c_str(), payload.size(), &avail_out);
    off_args[i]->src_size = avail_out;
    LOG_PRINT(LOG_VERBOSE, "Compressed size: %d\n", avail_out);

    /* decompressed dst */
    off_args[i]->dst_payload = (char *)malloc(IAA_COMPRESS_MAX_DEST_SIZE);
    off_args[i]->dst_size = IAA_COMPRESS_MAX_DEST_SIZE;

    off_args[i]->aux_payload = (char *)glob_indir_arr;

    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
  }

  *p_off_args = off_args;
}

void alloc_offload_decomp_and_scatter_args_timed( /* is this scatter ?*/
  int total_requests,
  timed_offload_request_args *** p_off_args,
  ax_comp *comps, uint64_t *ts0,
  uint64_t *ts1, uint64_t *ts2,
  uint64_t *ts3
){

  timed_offload_request_args **off_args =
    (timed_offload_request_args **)malloc(total_requests * sizeof(timed_offload_request_args *));


  if(glob_indir_arr == NULL){
    /* gen the indir arr */
    indirect_array_gen(&glob_indir_arr);

  }

  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
  for(int i = 0; i < total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));

    off_args[i]->comp = &(comps[i]);
    off_args[i]->id = i;

    /* comp'd source */
    avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
    off_args[i]->src_payload = (char *)malloc(input_size * sizeof(char));
    gpcore_do_compress((void *) off_args[i]->src_payload,
          (void *) payload.c_str(), payload.size(), &avail_out);
    off_args[i]->src_size = avail_out;
    LOG_PRINT(LOG_VERBOSE, "Compressed size: %d\n", avail_out);

    /* decompressed dst */
    off_args[i]->dst_payload = (char *)malloc(IAA_COMPRESS_MAX_DEST_SIZE);
    off_args[i]->dst_size = IAA_COMPRESS_MAX_DEST_SIZE;

    off_args[i]->aux_payload = (char *)glob_indir_arr;

    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));

    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
  }

  *p_off_args = off_args;
}

void cpu_decomp_and_scatter(
  fcontext_transfer_t arg
){
  gpcore_request_args *args = (gpcore_request_args *)arg.data;

  int id = args->id;

  char *src1 = args->inputs[0];
  float *dst1 = (float *)(args->inputs[1]);
  int compressed_size = *((int *) args->inputs[2]);
  int *indir_arr = (int *) args->inputs[3];

  uLong decompressed_size = IAA_COMPRESS_MAX_DEST_SIZE;
    /* dst is provisioned by allocator to have max dest size */

  int rc = gpcore_do_decompress(dst1, src1, compressed_size, &decompressed_size);
  if(rc != 0){
    LOG_PRINT(LOG_ERR, "Error Decompressing\n");
  }

  for(int i=0; i<num_accesses; i++){
    dst1[indir_arr[i]] = 1.0;
  }

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void cpu_decomp_and_scatter_stamped(
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

void free_cpu_compressed_payloads(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
  char ***ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
  for(int i = 0; i < total_requests; i++){
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]);
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1]);
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]);

    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i]);
  }
  free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
  free(glob_indir_arr);
  glob_indir_arr = NULL;
}

void cpu_compressed_payload_allocator(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
  char *** ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = (char ***) malloc(total_requests * sizeof(char **));

  if(glob_indir_arr == NULL){
    /* gen the indir arr */
    indirect_array_gen(&glob_indir_arr);

  }

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
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][3] = (char *) glob_indir_arr;

    /* and the size of the compressed payload for decomp */
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2] =
      (char *) malloc(sizeof(int));
    *(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]) = avail_out;

  }

  *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
}

void free_offload_decomp_and_scatter_args_timed(
  int total_requests,
  timed_offload_request_args *** p_off_args
){
  timed_offload_request_args **off_args = *p_off_args;
  for(int i = 0; i < total_requests; i++){
    free(off_args[i]->src_payload);
    free(off_args[i]->dst_payload);
    free(off_args[i]->desc);
    free(off_args[i]);
  }
  free(glob_indir_arr);
  glob_indir_arr = NULL;
  free(off_args);
}