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

#include <numa.h>
#include <sys/user.h>

int gDebugParam = 1;

struct task * on_node_task_alloc(struct acctest_context *ctx, int desc_node, int cr_node){
  struct task *tsk;

  tsk = malloc(sizeof(struct task));
  if (!tsk)
		return NULL;
	memset(tsk, 0, sizeof(struct task));

  tsk->desc = numa_alloc_onnode(sizeof(struct hw_desc), desc_node);
  if (!tsk->desc) {
    free(tsk);
    return NULL;
  }
  memset(tsk->desc, 0, sizeof(struct hw_desc));

	/* page fault test, alloc 4k size */
  /* https://stackoverflow.com/questions/8154162/numa-aware-cache-aligned-memory-allocation
  Cache lines are typically 64 bytes. Since 4096 is a multiple of 64, anything that comes back from numa_alloc_*() will already be memaligned at the cache level
  */
	if (ctx->is_evl_test)
		tsk->comp = numa_alloc_onnode(PAGE_SIZE, cr_node);
	else
		tsk->comp = numa_alloc_onnode(sizeof(struct completion_record), cr_node);
	if (!tsk->comp) {
		free(tsk->desc);
		free_task(tsk);
		return NULL;
	}
	memset(tsk->comp, 0, sizeof(struct completion_record));

	return tsk;
}

typedef struct mini_buf_test_args {
  uint32_t flags;
  int desc_node;
  int cr_node;
  int src_buf_node;
  int dst_buf_node;
} mbuf_targs;

void *submit_thread(void *arg){
  mbuf_targs *t_args = (mbuf_targs *) arg;
  int desc_node = t_args->desc_node;
  int cr_node = t_args->cr_node;
  int src_buf_node = t_args->src_buf_node;
  int dst_buf_node = t_args->dst_buf_node;

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
    mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, src_buf_node);
    dst_mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, dst_buf_node);
    for(int j=0; j<xfer_size; j++){
      __builtin_prefetch((const void*) mini_bufs[i] + j);
      __builtin_prefetch((const void*) dst_mini_bufs[i] + j);
    }
  }


  /* int on_node_alloc_multiple_tasks(dsa, num_bufs); */
  struct task_node *tmp_tsk_node;
  int cnt = 0;
  while(cnt < num_bufs){
    tmp_tsk_node = dsa->multi_task_node;
    dsa->multi_task_node = (struct task_node *)malloc(sizeof(struct task_node));
    if (!dsa->multi_task_node)
			return -ENOMEM;

    dsa->multi_task_node->tsk = on_node_task_alloc(dsa, desc_node, cr_node);
    if (!dsa->multi_task_node->tsk)
			return -ENOMEM;
    dsa->multi_task_node->next = tmp_tsk_node;
    cnt++;
  }


  task_node = dsa->multi_task_node;

  int idx=0;
  /* How fast can we complete 1024 256B offloads to different memory locations? */

  while(task_node){
    prepare_memcpy_task(task_node->tsk, dsa,mini_bufs[idx], xfer_size,dst_mini_bufs[idx]);
    task_node->tsk->desc->flags |= t_args->flags;
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

  int dsa_node = 1;
  int remote_node = 0;
  pthread_t tid;

  mbuf_targs targs;
  targs.flags =  IDXD_OP_FLAG_CC;
  targs.desc_node = dsa_node;
  targs.cr_node = dsa_node;
  targs.src_buf_node = dsa_node;
  targs.dst_buf_node = dsa_node;
  createThreadPinned(&tid,submit_thread,&targs,20);
  pthread_join(tid,NULL);

  targs.desc_node = remote_node;
  targs.cr_node = remote_node;
  targs.src_buf_node = dsa_node;
  targs.dst_buf_node = dsa_node;
  createThreadPinned(&tid,submit_thread,&targs,20);
  pthread_join(tid,NULL);

  targs.flags =  IDXD_OP_FLAG_CC;
  targs.desc_node = dsa_node;
  targs.cr_node = dsa_node;
  targs.src_buf_node = remote_node;
  targs.dst_buf_node = remote_node;
  createThreadPinned(&tid,submit_thread,&targs,20);
  pthread_join(tid,NULL);

  targs.flags =  IDXD_OP_FLAG_CC;
  targs.desc_node = remote_node;
  targs.cr_node = remote_node;
  targs.src_buf_node = remote_node;
  targs.dst_buf_node = remote_node;
  createThreadPinned(&tid,submit_thread,&targs,20);
  pthread_join(tid,NULL);



  // chainingDeflateAndCrcComparison(dcInstHandles,sessionHandles);
exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}