#include "test_harness.h"
#include "print_utils.h"

extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
}

int gLogLevel = LOG_MONITOR;
bool gDebugParam = false;
int main(){
  struct task_node *tsk_node;
  struct acctest_context *iaa;
  int tflags = TEST_FLAGS_BOF;
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
  memcpy(src2, iaa_compress_aecs, IAA_COMPRESS_AECS_SIZE);

  memset_pattern(src1, pattern, bufsize);

  iaa = acctest_init(tflags);
  rc = acctest_alloc(iaa, wq_type, dev_id, wq_id);
  if(rc != ACCTEST_STATUS_OK){
    return rc;
  }
  rc = acctest_alloc_multiple_tasks(iaa, itr);
  if(rc != ACCTEST_STATUS_OK){
    return rc;
  }
  tsk_node = iaa->multi_task_node;
  while(tsk_node){
    struct task *tsk = tsk_node->tsk;
    rc = init_task(tsk, tflags, opcode, bufsize );
    if(rc != ACCTEST_STATUS_OK){
      return rc;
    }
    tsk_node = tsk_node->next;
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

  // iaa_compress_multi_task_nodes(iaa);
  // rc = iaa_task_result_verify_task_nodes(iaa, 0);
  acctest_free_task(iaa);
  acctest_free(iaa);
}