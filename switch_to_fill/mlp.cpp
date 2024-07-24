#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"
#include "iaa_offloads.h"

extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
}

void cpu_decompress_and_hash(fcontext_transfer_t arg){
  timed_gpcore_request_args* args = (timed_gpcore_request_args *)arg.data;

  int id = args->id;

}

int gLogLevel = LOG_DEBUG;
bool gDebugParam = false;
int main(){
  int opcode = 67; /* compress opcode */
  int wq_id = 0;
  int dev_id = 3;
  int wq_type = SHARED;
  int rc;
  int itr = 1;

  int bufsize = 1024;

  uint64_t pattern = 0x98765432abcdef01;
  char *src1 = (char *) aligned_alloc(32,bufsize);
  char *dst1 = (char *) aligned_alloc(32,IAA_COMPRESS_MAX_DEST_SIZE);
  char *src2 =
    (char *) aligned_alloc(32, IAA_COMPRESS_SRC2_SIZE);
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


  hw->flags = 0x5000eUL;
  hw->opcode = 0x43;
  hw->src_addr = (uint64_t) src1;
  hw->dst_addr = (uint64_t) dst1;
  hw->xfer_size = bufsize;

  hw->completion_addr = (uint64_t) comp;
  hw->iax_compr_flags = 14;
  hw->iax_src2_addr = (uint64_t) src2;
  hw->iax_src2_xfer_size = IAA_COMPRESS_AECS_SIZE;
  hw->iax_max_dst_size = bufsize;

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