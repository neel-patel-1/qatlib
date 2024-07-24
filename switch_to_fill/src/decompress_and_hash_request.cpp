#include "decompress_and_hash_request.hpp"

#include "print_utils.h"
void cpu_decompress_and_hash_stamped(fcontext_transfer_t arg){
  timed_gpcore_request_args* args = (timed_gpcore_request_args *)arg.data;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  char *src1 = args->inputs[0];
  char *dst1 = args->inputs[1];
  int compressed_size = *((int *) args->inputs[2]);

  int decompressed_size = IAA_COMPRESS_MAX_DEST_SIZE;
    /* dst is provisioned by allocator to have max dest size */

  ts0[id] = sampleCoderdtsc();
  decompressed_size = gpcore_do_decompress(dst1, src1, compressed_size, &decompressed_size);
  ts1[id] = sampleCoderdtsc();

  uint32_t hash = furc_hash(dst1, decompressed_size, 16);
  LOG_PRINT(LOG_DEBUG, "Hashing: %s\n", dst1);
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
  std::string query = "/region/cluster/foo:key|#|etc";
  char *** ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads = (char ***) malloc(total_requests * sizeof(char **));

  int avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
  int num_inputs_per_request = 3;
  for(int i = 0; i < total_requests; i++){
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i] =
      (char **) malloc(sizeof(char *) * num_inputs_per_request); /* only one input to each request */

    /* CPU request needs a src payload to decompress */
    avail_out = IAA_COMPRESS_MAX_DEST_SIZE;
    ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0] =
      (char *) malloc(avail_out);
    // compress((Bytef *)(ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]), (uLongf *)&compressed_size,
    //   (const Bytef*)(query.c_str()), query.size());
    gpcore_do_compress((void *) (ptr_toArrOfPtrs_toArrOfPtrs_toInputPayloads[i][0]),
      (void *) query.c_str(), query.size(), &avail_out);

    LOG_PRINT(LOG_DEBUG, "Compressed size: %d\n", avail_out);

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