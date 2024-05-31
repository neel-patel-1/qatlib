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

  struct task_node * task_node;
  uint8_t **mini_bufs;
  uint8_t **dst_mini_bufs;
  int num_bufs = 1024;
  int xfer_size = 256;

  mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  dst_mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  for(int i=0; i<num_bufs; i++){
    mini_bufs[i] = (uint8_t *)malloc(xfer_size);
    dst_mini_bufs[i] = (uint8_t *)malloc(xfer_size);
    for(int j=0; j<xfer_size; j++){
      __builtin_prefetch((const void*) mini_bufs[i] + j);
      __builtin_prefetch((const void*) dst_mini_bufs[i] + j);
    }
  }
  acctest_alloc_multiple_tasks(dsa, num_bufs);
  task_node = dsa->multi_task_node;

  int idx=0;
  /* How fast can we complete 1024 256B offloads to different memory locations? */

  while(task_node){
    prepare_memcpy_task(task_node->tsk, dsa,mini_bufs[idx], xfer_size,dst_mini_bufs[idx]);
    idx++;
    task_node = task_node->next;
  }

  task_node = dsa->multi_task_node;
  uint64_t start = sampleCoderdtsc();
  /* All data is in the cache and src/dst buffers are either both close to DSA or both far away */
  while(task_node){
    acctest_desc_submit(dsa, task_node->tsk->desc);
    while(task_node->tsk->comp->status == 0){
      _mm_pause();
    }
    task_node = task_node->next;
  }
  uint64_t end = sampleCoderdtsc();
  printf("Time taken for 1024 256B offloads: %lu\n", end-start);




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


  createThreadPinned(&tid,submit_thread,NULL,20);
  pthread_join(tid,NULL);

  // chainingDeflateAndCrcComparison(dcInstHandles,sessionHandles);
exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}