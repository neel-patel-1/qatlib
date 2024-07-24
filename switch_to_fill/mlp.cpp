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


void prepare_compress_iaa_compress_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src1, uint64_t src2, uint64_t dst1,
  uint64_t comp, uint64_t xfer_size )
{
  hw->flags = 0x5000eUL;
  hw->opcode = 0x43;
  hw->src_addr = src1;
  hw->dst_addr = dst1;
  hw->xfer_size = xfer_size;

  hw->completion_addr = comp;
  hw->iax_compr_flags = 14;
  hw->iax_src2_addr = src2;
  hw->iax_src2_xfer_size = IAA_COMPRESS_AECS_SIZE;
  hw->iax_max_dst_size = IAA_COMPRESS_MAX_DEST_SIZE;
}

void alloc_src_and_dst_compress_bufs(char **src1, char **dst1, char **src2,
  int input_size){
  *src1 = (char *) aligned_alloc(32, 1024);
  *dst1 = (char *) aligned_alloc(32, IAA_COMPRESS_MAX_DEST_SIZE);
  *src2 = (char *) aligned_alloc(32, IAA_COMPRESS_SRC2_SIZE);
}

int gLogLevel = LOG_DEBUG;
bool gDebugParam = false;
int main(){


  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;
  int rc;
  int itr = 1;
  int total_requests = 1;

  int bufsize = 1024;

  run_gpcore_request_brkdown(
    cpu_decompress_and_hash_stamped,
    compressed_mc_req_allocator,
    compressed_mc_req_free,
    itr,
    total_requests
  );

  return 0;

  uint64_t pattern = 0x98765432abcdef01;
  char *src1, *dst1, *src2;
  alloc_src_and_dst_compress_bufs(&src1, &dst1, &src2, bufsize);

  char *src1_decomp =
    (char *) aligned_alloc(32, IAA_COMPRESS_MAX_DEST_SIZE);
  int decompressed_size = IAA_COMPRESS_MAX_DEST_SIZE;
  int compressed_size = 0;
  memcpy(src2, iaa_compress_aecs, IAA_COMPRESS_AECS_SIZE);
  memset_pattern(src1, pattern, bufsize);

  initialize_iaa_wq(dev_id, wq_id, wq_type);
  if(rc != ACCTEST_STATUS_OK){
    return rc;
  }

  struct hw_desc *hw;
  hw = (struct hw_desc *) malloc(sizeof(struct hw_desc));
  memset(hw, 0, sizeof(struct hw_desc));
  ax_comp *comp =
    (ax_comp *) aligned_alloc(iaa->compl_size, sizeof(ax_comp));

  prepare_compress_iaa_compress_desc_with_preallocated_comp(
    hw, (uint64_t) src1, (uint64_t) src2, (uint64_t) dst1,
    (uint64_t) comp, bufsize);

  acctest_desc_submit(iaa, hw);

  acctest_wait_on_desc_timeout(comp, iaa, 1000);

  compressed_size = comp->iax_output_size;

  LOG_PRINT( LOG_DEBUG, "Compressed size: %d\n", compressed_size);

  iaa_do_decompress(src1_decomp, dst1, compressed_size,
     &decompressed_size);

  if(memcmp(src1, src1_decomp, bufsize) != 0){
    LOG_PRINT(LOG_ERR, "Decompressed data does not match original data\n");
    return -1;
  }

  acctest_free_task(iaa);
  acctest_free(iaa);
}