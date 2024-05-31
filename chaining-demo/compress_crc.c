#include <stdio.h>
#include "cpa_types.h"
#include "icp_sal_user.h"
#include "cpa.h"
#include "cpa_dc.h"
#include "qae_mem.h"
#include "cpa_sample_utils.h"

#include "dc_inst_utils.h"
#include "thread_utils.h"

#include "buffer_prepare_funcs.h"
#include "hw_comp_crc_funcs.h"
#include "sw_comp_crc_funcs.h"

#include "print_funcs.h"

#include <zlib.h>


#include "idxd.h"
#include "dsa.h"

#include "dsa_funcs.h"

#include "validate_compress_and_crc.h"

#include "accel_test.h"

#include "sw_chain_comp_crc_funcs.h"
#include "smt-thread-exps.h"

#include "tests.h"

#include <xmmintrin.h>


int gDebugParam = 1;

void *submit_thread(void *arg){
  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int dev_id = 1;
  int wq_id = 0;
  int opcode = 16;
  int wq_type = ACCFG_WQ_SHARED;
  int rc;
  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;
  if (!dsa)
		return -ENOMEM;
  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;

  struct task_node *multi_task_node;

  /* How fast can we complete 1024 256B offloads to different memory locations? */
  /* All data is in the cache and src/dst buffers are either both close to DSA or both far away */
  int xfer_size = 256;
  uint8_t *src_buf = (uint8_t *)malloc(xfer_size);
  uint8_t *dst_buf = (uint8_t *)malloc(xfer_size);
  for(int i=0; i<xfer_size; i++){
    __builtin_prefetch((const void*) src_buf + i);
    __builtin_prefetch((const void*) dst_buf + i);
  }

  acctest_free(dsa);
}


int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;

  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);


  pthread_t tid;

  createThreadPinned(&tid,submit_thread,NULL,10);
  pthread_join(tid,NULL);




  // chainingDeflateAndCrcComparison(dcInstHandles,sessionHandles);
exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}