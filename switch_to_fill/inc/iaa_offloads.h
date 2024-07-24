#ifndef IAA_OFFLOADS_H
#define IAA_OFFLOADS_H

#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"


extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
}

extern struct acctest_context *iaa;

void initialize_iaa_wq(int dev_id,
  int wq_id, int wq_type);
void free_iaa_wq();
void prepare_iaa_decompress_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src1, uint64_t dst1,
  uint64_t comp, uint64_t xfer_size );
void prepare_iaa_compress_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src1, uint64_t src2, uint64_t dst1,
  uint64_t comp, uint64_t xfer_size );

#endif