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