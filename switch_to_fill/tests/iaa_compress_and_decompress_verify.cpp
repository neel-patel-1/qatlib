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
  int itr = 100;
  int total_requests = 1000;

  uint32_t bufsize = 1024;

  // run_gpcore_request_brkdown(
  //   cpu_decompress_and_hash_stamped,
  //   compressed_mc_req_allocator,
  //   compressed_mc_req_free,
  //   itr,
  //   total_requests
  // );

  // return 0;

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

  /* compress */
  prepare_iaa_compress_desc_with_preallocated_comp(
    hw, (uint64_t) src1, (uint64_t) src2, (uint64_t) dst1,
    (uint64_t) comp, bufsize);

  acctest_desc_submit(iaa, hw);

  acctest_wait_on_desc_timeout(comp, iaa, 1000);

  compressed_size = comp->iax_output_size;

  LOG_PRINT( LOG_DEBUG, "Compressed size: %d\n", compressed_size);

  /* validate */
  iaa_do_decompress(src1_decomp, dst1, compressed_size,
     &decompressed_size);

  if(memcmp(src1, src1_decomp, bufsize) != 0){
    LOG_PRINT(LOG_ERR, "Decompressed data does not match original data\n");
    return -1;
  }

  /* decompress */
  prepare_iaa_decompress_desc_with_preallocated_comp(
    hw, (uint64_t) dst1, (uint64_t) src1_decomp,
    (uint64_t) comp, compressed_size);

  acctest_desc_submit(iaa, hw);

  acctest_wait_on_desc_timeout(comp, iaa, 1000);
  if(comp->status != IAX_COMP_SUCCESS){
    return comp->status;
  }
  if(memcmp(src1, src1_decomp, bufsize) != 0){
    LOG_PRINT(LOG_ERR, "Decompressed data does not match original data\n");
    return -1;
  }
  if(bufsize - comp->iax_output_size){
    LOG_PRINT(LOG_ERR, "Decompressed size: %u does not match original size %u\n",
      comp->iax_output_size, bufsize);
    return -1;
  }
  LOG_PRINT(LOG_DEBUG, "Decompressed size: %u\n", comp->iax_output_size);


  acctest_free_task(iaa);
  acctest_free(iaa);
}