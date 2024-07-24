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

void free_iaa_wq(){
  acctest_free(iaa);
}

void prepare_iaa_compress_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src1, uint64_t src2, uint64_t dst1,
  uint64_t comp, uint64_t xfer_size )
{
  memset(hw, 0, sizeof(struct hw_desc));
  hw->flags = 0x5000eUL;
  hw->opcode = 0x43;
  hw->src_addr = src1;
  hw->dst_addr = dst1;
  hw->xfer_size = xfer_size;

  memset((void *)comp, 0, sizeof(ax_comp));
  hw->completion_addr = comp;
  hw->iax_compr_flags = 14;
  hw->iax_src2_addr = src2;
  hw->iax_src2_xfer_size = IAA_COMPRESS_AECS_SIZE;
  hw->iax_max_dst_size = IAA_COMPRESS_MAX_DEST_SIZE;
}

void prepare_iaa_decompress_desc_with_preallocated_comp(
  struct hw_desc *hw, uint64_t src1, uint64_t dst1,
  uint64_t comp, uint64_t xfer_size )
{
  memset(hw, 0, sizeof(struct hw_desc));
  hw->flags = 14;
  hw->opcode = IAX_OPCODE_DECOMPRESS;
  hw->src_addr = src1;
  hw->dst_addr = dst1;
  hw->xfer_size = xfer_size;

  memset((void *)comp, 0, sizeof(ax_comp));
  hw->completion_addr = comp;
  hw->iax_decompr_flags = 31;
  hw->iax_src2_addr = 0x0;
  hw->iax_src2_xfer_size = 0;
  hw->iax_max_dst_size = IAA_COMPRESS_MAX_DEST_SIZE;
}