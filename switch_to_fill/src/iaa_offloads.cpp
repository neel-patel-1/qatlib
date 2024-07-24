#include "iaa_offloads.h"
#include "print_utils.h"
#include <cstdio>

struct acctest_context *iaa;

void initialize_iaa_wq(int dev_id, int wq_id, int wq_type){
  int tflags = TEST_FLAGS_BOF;
  int rc;

  iaa = acctest_init(tflags);
  rc = acctest_alloc(iaa, wq_type, dev_id, wq_id);
  if(rc != ACCTEST_STATUS_OK){
    LOG_PRINT( LOG_ERR, "Error allocating work queue\n");
    return;
  }
}

void prepare_compress_iaa_compress_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src1, uint64_t src2, uint64_t dst1,
  uint64_t comp, uint64_t xfer_size, uint64_t iaa_max_dst_size
)
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
  hw->iax_max_dst_size = iaa_max_dst_size;
}