#include "test_harness.h"
#include "print_utils.h"

extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
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
  iaa_compress_multi_task_nodes(iaa);
  rc = iaa_task_result_verify_task_nodes(iaa, 0);
  acctest_free_task(iaa);
  acctest_free(iaa);
}