#include "decompress_and_hash_request.hpp"

#include "print_utils.h"
#include "wait.h"

std::string query = "/region/cluster/foo:key|#|etc";

void cpu_decompress_and_hash_stamped(fcontext_transfer_t arg){
  timed_gpcore_request_args* args = (timed_gpcore_request_args *)arg.data;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  char *src1 = args->inputs[0];
  char *dst1 = args->inputs[1];
  int compressed_size = *((int *) args->inputs[2]);

  uLong decompressed_size = IAA_COMPRESS_MAX_DEST_SIZE;
    /* dst is provisioned by allocator to have max dest size */

  ts0[id] = sampleCoderdtsc();
  int rc = gpcore_do_decompress(dst1, src1, compressed_size, &decompressed_size);
  if(rc != 0){
    LOG_PRINT(LOG_ERR, "Error Decompressing\n");
  }
  ts1[id] = sampleCoderdtsc();

  uint32_t hash = furc_hash(dst1, decompressed_size, 16);
  LOG_PRINT(LOG_VERBOSE, "Hashing: %ld bytes\n", decompressed_size);
  ts2[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);

}

void compressed_mc_req_free(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){
  char *** ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
  int num_inputs_per_request = 2;
  for(int i = 0; i < total_requests; i++){
    for(int j=0; j<num_inputs_per_request; j++){
      free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][j]);
    }
    free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i]);
  }
  free(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads);
}

void compressed_mc_req_allocator(int total_requests,
  char ****ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads){

  char *** ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = (char ***) malloc(total_requests * sizeof(char **));

  // std::string query = gen_compressible_string("/region/cluster/foo:key|#|etc", input_size);

  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
  int num_inputs_per_request = 3;
  for(int i = 0; i < total_requests; i++){
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i] =
      (char **) malloc(sizeof(char *) * num_inputs_per_request); /* only one input to each request */

    /* CPU request needs a src payload to decompress */
    avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0] =
      (char *) malloc(avail_out);

    gpcore_do_compress((void *) (ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]),
      (void *) query.c_str(), query.size(), &avail_out);

    LOG_PRINT(LOG_VERBOSE, "Compressed size: %d\n", avail_out);

    /* and a dst payload to decompress into */
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][1] =
      (char *) malloc(IAA_COMPRESS_MAX_DEST_SIZE);

    /* and the size of the compressed payload for decomp */
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2] =
      (char *) malloc(sizeof(int));
    *(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][2]) = avail_out;
  }

  *ptr_toPtr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads;
}

void alloc_decomp_and_hash_offload_args_stamped(int total_requests,
  timed_offload_request_args*** p_off_args, ax_comp *comps,
  uint64_t *ts0, uint64_t *ts1, uint64_t *ts2, uint64_t *ts3){
  timed_offload_request_args **off_args =
    (timed_offload_request_args **)malloc(sizeof(timed_offload_request_args *) * total_requests);

  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE; /* using 4MB allocator */

  for(int i=0; i<total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->comp = &(comps[i]);
    off_args[i]->id = i;

    /* Offload request needs a src payload to decompress */
    avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
    /* compress query into src payload */
    off_args[i]->src_payload = (char *)malloc(avail_out);
    gpcore_do_compress((void *) off_args[i]->src_payload,
      (void *) query.c_str(), query.size(), &avail_out);

    LOG_PRINT(LOG_VERBOSE, "Compressed Size: %d\n", avail_out);

    /* and a dst payload to decompress into */
    off_args[i]->dst_payload = (char *)malloc(IAA_COMPRESS_MAX_DEST_SIZE);

    /* and the size of the compressed payload for decomp */
    off_args[i]->src_size = (uint64_t)avail_out;

    /* and the size of the original payload for hash */
    off_args[i]->dst_size = query.size();

    /* and a preallocated desc for prep and submit */
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));

    /* and timestamps */
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
  }
  *p_off_args = off_args;
}

void alloc_decomp_and_hash_offload_args(int total_requests,
  offload_request_args*** p_off_args, ax_comp *comps){
  offload_request_args **off_args =
    (offload_request_args **)malloc(sizeof(offload_request_args *) * total_requests);

  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE; /* using 4MB allocator */
  // std::string query = gen_compressible_string("/region/cluster/foo:key|#|etc", input_size);

  for(int i=0; i<total_requests; i++){
    off_args[i] = (offload_request_args *)malloc(sizeof(offload_request_args));
    off_args[i]->comp = &(comps[i]);
    off_args[i]->id = i;

    /* Offload request needs a src payload to decompress */
    avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
    /* compress query into src payload */
    off_args[i]->src_payload = (char *)malloc(avail_out);
    gpcore_do_compress((void *) off_args[i]->src_payload,
      (void *) query.c_str(), query.size(), &avail_out);

    LOG_PRINT(LOG_VERBOSE, "Compressed Size: %d\n", avail_out);

    /* and a dst payload to decompress into */
    off_args[i]->dst_payload = (char *)malloc(IAA_COMPRESS_MAX_DEST_SIZE);

    /* and the size of the compressed payload for decomp */
    off_args[i]->src_size = (uint64_t)avail_out;

    /* and the size of the original payload for hash */
    off_args[i]->dst_size = query.size();

    /* and a preallocated desc for prep and submit */
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));

  }
  *p_off_args = off_args;
}

void free_decomp_and_hash_offload_args_stamped(int total_requests, timed_offload_request_args*** off_args){
  timed_offload_request_args **off_args_ptr = *off_args;
  for(int i=0; i<total_requests; i++){
    free(off_args_ptr[i]->src_payload);
    free(off_args_ptr[i]->dst_payload);
    free(off_args_ptr[i]->desc);
    free(off_args_ptr[i]);
  }
  free(off_args_ptr);
}

void free_decomp_and_hash_offload_args(int total_requests,
  offload_request_args*** off_args){
  offload_request_args **off_args_ptr = *off_args;
  for(int i=0; i<total_requests; i++){
    free(off_args_ptr[i]->src_payload);
    free(off_args_ptr[i]->dst_payload);
    free(off_args_ptr[i]->desc);
    free(off_args_ptr[i]);
  }
  free(off_args_ptr);
}

void yielding_decompress_and_hash_request_stamped(
  fcontext_transfer_t arg){

  timed_offload_request_args *args =
    (timed_offload_request_args *) arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
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
  /* prep hw desc */
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
  /* hash the decompressed payload */
  LOG_PRINT(LOG_VERBOSE, "Hashing: %ld bytes\n", dst_size);
  uint32_t hash = furc_hash((char *)dst, dst_size, 16);

  ts3[id] = sampleCoderdtsc();
  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void blocking_decompress_and_hash_request_stamped(
  fcontext_transfer_t arg){

  timed_offload_request_args *args =
    (timed_offload_request_args *) arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
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
  /* prep hw desc */
  prepare_iaa_decompress_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)comp, (uint64_t)src_size);
  blocking_iaa_submit(iaa, desc);

  ts1[id] = sampleCoderdtsc();
  spin_on(comp);
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Decompress failed: 0x%x\n", comp->status);
  }
  LOG_PRINT(LOG_VERBOSE, "Decompressed size: %d\n", comp->iax_output_size);

  ts2[id] = sampleCoderdtsc();
  /* hash the decompressed payload */
  LOG_PRINT(LOG_VERBOSE, "Hashing: %ld bytes\n", dst_size);
  uint32_t hash = furc_hash((char *)dst, dst_size, 16);

  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void blocking_decompress_and_hash_request(
  fcontext_transfer_t arg){

  offload_request_args *args =
    (offload_request_args *) arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
  int id = args->id;
  uint64_t src_size = args->src_size;
  uint64_t dst_size = args->dst_size;


  struct hw_desc *desc = args->desc;
  ax_comp *comp = args->comp;

  /* prep hw desc */
  prepare_iaa_decompress_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)comp, (uint64_t)src_size);
  blocking_iaa_submit(iaa, desc);

  while(comp->status == IAX_COMP_NONE){
    _mm_pause();
  }
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Decompress failed: 0x%x\n", comp->status);
  }
  LOG_PRINT(LOG_VERBOSE, "Decompressed size: %d\n", comp->iax_output_size);

  /* hash the decompressed payload */
  LOG_PRINT(LOG_VERBOSE, "Hashing: %ld bytes\n", dst_size);
  uint32_t hash = furc_hash((char *)dst, dst_size, 16);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void cpu_decompress_and_hash(fcontext_transfer_t arg){
  timed_gpcore_request_args* args = (timed_gpcore_request_args *)arg.data;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  char *src1 = args->inputs[0];
  char *dst1 = args->inputs[1];
  int compressed_size = *((int *) args->inputs[2]);

  uLong decompressed_size = IAA_COMPRESS_MAX_DEST_SIZE;
    /* dst is provisioned by allocator to have max dest size */

  int rc = gpcore_do_decompress(dst1, src1, compressed_size, &decompressed_size);
  if(rc != 0){
    LOG_PRINT(LOG_ERR, "Error Decompressing\n");
  }

  uint32_t hash = furc_hash(dst1, decompressed_size, 16);
  LOG_PRINT(LOG_VERBOSE, "Hashing: %ld bytes\n", decompressed_size);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);

}

void yielding_decompress_and_hash_request(fcontext_transfer_t arg){
  offload_request_args* args = (offload_request_args *)arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;
  int id = args->id;
  uint64_t src_size = args->src_size;
  uint64_t dst_size = args->dst_size;

  struct hw_desc *desc = args->desc;
  ax_comp *comp = args->comp;

  /* prep hw desc */
  prepare_iaa_decompress_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)comp, (uint64_t)src_size);
  blocking_iaa_submit(iaa, desc);

  fcontext_swap(arg.prev_context, NULL);
  if(comp->status != IAX_COMP_SUCCESS){
    LOG_PRINT(LOG_ERR, "Decompress failed: %d\n", comp->status);
  }
  LOG_PRINT(LOG_VERBOSE, "Decompressed size: %d\n", comp->iax_output_size);

  /* hash the decompressed payload */
  LOG_PRINT(LOG_VERBOSE, "Hashing: %ld bytes\n", dst_size);
  uint32_t hash = furc_hash((char *)dst, dst_size, 16);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}