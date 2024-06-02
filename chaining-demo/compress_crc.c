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

uint8_t **mini_bufs;
uint8_t **dst_mini_bufs;


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

typedef struct _alloc_td_args{
  int num_bufs;
  int xfer_size;
  int src_buf_node;
  int dst_buf_node;
  pthread_barrier_t *alloc_sync;
} alloc_td_args;

void * buf_alloc_td(void *arg){
  alloc_td_args *args = (alloc_td_args *) arg;
  int num_bufs = args->num_bufs;
  int xfer_size = args->xfer_size;
  int src_buf_node = args->src_buf_node;
  int dst_buf_node = args->dst_buf_node;
  pthread_barrier_t *alloc_sync = args->alloc_sync;

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
  pthread_barrier_wait(alloc_sync); /* increment the semaphore once we have alloc'd */
}

typedef struct mini_buf_test_args {
  uint32_t flags;
  int dev_id;
  int desc_node;
  int cr_node;
  int flush_desc;
  int flush_bufs;
  int num_bufs;
  int xfer_size;
  pthread_barrier_t *alloc_sync;
} mbuf_targs;

void *submit_thread(void *arg){
  mbuf_targs *t_args = (mbuf_targs *) arg;
  int desc_node = t_args->desc_node;
  int cr_node = t_args->cr_node;
  bool flush_bufs = t_args->flush_bufs;
  bool flush_desc = t_args->flush_desc;

  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int dev_id = t_args->dev_id;
  int wq_id = 0;
  int opcode = 16;
  int wq_type = ACCFG_WQ_SHARED;
  int rc;
  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;
  if (!dsa)
		return (void *)NULL;
  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return (void *)NULL;

  struct task_node * task_node;

  int num_bufs = t_args->num_bufs;
  int xfer_size = t_args->xfer_size;


  /* int on_node_alloc_multiple_tasks(dsa, num_bufs); */
  struct task_node *tmp_tsk_node;
  int cnt = 0;
  while(cnt < num_bufs){
    tmp_tsk_node = dsa->multi_task_node;
    dsa->multi_task_node = (struct task_node *)malloc(sizeof(struct task_node));
    if (!dsa->multi_task_node)
			return (void *)NULL;

    dsa->multi_task_node->tsk = on_node_task_alloc(dsa, desc_node, cr_node);
    if (!dsa->multi_task_node->tsk)
			return (void *)NULL;
    dsa->multi_task_node->next = tmp_tsk_node;
    cnt++;
  }


  task_node = dsa->multi_task_node;

  int idx=0;

  /* Wait for buffer alloc thread to create the buffers */
  pthread_barrier_t *alloc_sync = t_args->alloc_sync;
  pthread_barrier_wait(alloc_sync);
  while(task_node){
    prepare_memcpy_task(task_node->tsk, dsa,mini_bufs[idx], xfer_size,dst_mini_bufs[idx]);
    task_node->tsk->desc->flags |= t_args->flags;

    /* Flush task and src/dst */
    if(flush_desc){
      _mm_clflush(task_node->tsk->desc);
      _mm_clflush(task_node->tsk->comp);
    }
    if(flush_bufs){
      for(int i=0; i<xfer_size; i++){
        _mm_clflush(mini_bufs[idx] + i);
        _mm_clflush(dst_mini_bufs[idx] + i);
      }
    }


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




  // acctest_free(dsa);
  if(munmap(dsa->wq_reg, PAGE_SIZE)){
    err("Failed to unmap wq_reg\n");
  }
  close(dsa->fd);
  accfg_unref(dsa->ctx);
  struct task_node *tsk_node = NULL, *tmp_node = NULL;

  tsk_node = dsa->multi_task_node;
  while (tsk_node) {
    tmp_node = tsk_node->next;
    struct task *tsk = tsk_node->tsk;
    /* void free_task(struct task *tsk) */
    numa_free(tsk_node->tsk->desc, sizeof(struct hw_desc));
    numa_free(tsk_node->tsk->comp, sizeof(struct completion_record));
    mprotect(tsk_node->tsk->src1, PAGE_SIZE, PROT_READ | PROT_WRITE);
    if (tsk->opcode != IAX_OPCODE_TRANSL_FETCH) {
      numa_free(tsk->src1, tsk->xfer_size);
    } else {
      munmap(tsk->src1, tsk->xfer_size);
      close(tsk->group);
      close(tsk->container);
    }
    free(tsk->src2);
    numa_free(tsk->dst1, xfer_size);
    free(tsk->dst2);



    tsk_node->tsk = NULL;
    free(tsk_node);
    tsk_node = tmp_node;
  }
  dsa->multi_task_node = NULL;

  free(dsa);

}

CpaStatus offloadComponentLocationTest(){
  int dev_id = 0;
  int dsa_node = 0;
  int remote_node = 1;
  pthread_t submitThread, allocThread;
  pthread_barrier_t alloc_sync;

  for(int xfer_size=256; xfer_size <= 65536; xfer_size*=4){
    PRINT("Xfer size: %d\n", xfer_size);
    mbuf_targs targs;
    targs.dev_id = 0;
    targs.xfer_size = xfer_size;
    targs.num_bufs = 1024 * 10;
    targs.alloc_sync = &alloc_sync;
    targs.flush_bufs = false;
    targs.flush_desc = false;

    alloc_td_args args;
    args.num_bufs = 1024 * 10;
    args.xfer_size = xfer_size;
    args.alloc_sync = &alloc_sync;


    /* DESCRIPTOR LOCATION TEST */
    pthread_barrier_init(&alloc_sync, NULL, 2);
    /* descriptors and completion records (offloader) is on local  */
    targs.flags = IDXD_OP_FLAG_CC;
    targs.desc_node = dsa_node;
    targs.cr_node = dsa_node;

    /* buffers on local */
    args.src_buf_node = dsa_node;
    args.dst_buf_node = dsa_node;
    createThreadPinned(&allocThread,buf_alloc_td,&args,10);
    createThreadPinned(&submitThread,submit_thread,&targs,10);
    pthread_join(submitThread,NULL);

    pthread_barrier_init(&alloc_sync, NULL, 2);
    /* descriptors and completion records (offloader) is on far */
    targs.flags =  IDXD_OP_FLAG_CC;
    targs.desc_node = remote_node;
    targs.cr_node = remote_node;
    targs.flush_desc = 0;

    /* buffers on local */
    args.src_buf_node = dsa_node;
    args.dst_buf_node = dsa_node;
    createThreadPinned(&allocThread,buf_alloc_td,&args,10);
    createThreadPinned(&submitThread,submit_thread,&targs,20);
    pthread_join(submitThread,NULL);

    pthread_barrier_init(&alloc_sync, NULL, 2);
    /* descriptors and completion records (offloader) is on far  */
    targs.flags = IDXD_OP_FLAG_CC;
    targs.desc_node = remote_node;
    targs.cr_node = remote_node;

    /* buffers on far */
    args.src_buf_node = remote_node;
    args.dst_buf_node = remote_node;
    createThreadPinned(&allocThread,buf_alloc_td,&args,20);
    createThreadPinned(&submitThread,submit_thread,&targs,20);
    pthread_join(submitThread,NULL);





  }
}

#define DSA_TEST_SIZE 20000
#pragma GCC diagnostic ignored "-Wformat"

typedef struct _dwq_vs_shared_args{
  int num_bufs;
  int xfer_size;
  int flags;
} dwq_vs_shared_args;

int dwq_test(){

  dwq_vs_shared_args *t_args = (dwq_vs_shared_args *)malloc(sizeof(dwq_vs_shared_args));
  t_args->num_bufs = 128;
  t_args->xfer_size = 256;
  t_args->flags = IDXD_OP_FLAG_CC;

  struct acctest_context *dsa;
	int rc = 0;
	unsigned long buf_size = DSA_TEST_SIZE;
	int opcode = DSA_OPCODE_MEMMOVE;
	int bopcode = DSA_OPCODE_MEMMOVE;
	int tflags = TEST_FLAGS_BOF;
	int opt;
	unsigned int bsize = 0;
	char dev_type[MAX_DEV_LEN];
	int wq_id = ACCTEST_DEVICE_ID_NO_INPUT;
	int dev_id = ACCTEST_DEVICE_ID_NO_INPUT;
	int dev_wq_id = ACCTEST_DEVICE_ID_NO_INPUT;
	unsigned int num_desc = 1;
	struct evl_desc_list *edl = NULL;
	char *edl_str = NULL;

  int wq_type = DEDICATED; /*  sudo ./setup_dsa.sh -d dsa0 -w1 -md -e1 */

  dsa = acctest_init(tflags);
	dsa->dev_type = ACCFG_DEVICE_DSA;

	if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;

  if (buf_size > dsa->max_xfer_size) {
		err("invalid transfer size: %lu\n", buf_size);
		return -EINVAL;
	}

  struct task_node * task_node;

  int num_bufs = t_args->num_bufs;
  int xfer_size = t_args->xfer_size;

  acctest_alloc_multiple_tasks(dsa, num_bufs);

  int idx=0;
  uint8_t **dsa_mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  uint8_t **dsa_dst_mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  for(int i=0; i<num_bufs; i++){
    dsa_mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, 0);
    dsa_dst_mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, 0);
    for(int j=0; j<xfer_size; j++){
      __builtin_prefetch((const void*) dsa_mini_bufs[i] + j);
      __builtin_prefetch((const void*) dsa_dst_mini_bufs[i] + j);
    }
  }
  while(task_node){
    prepare_memcpy_task(task_node->tsk, dsa,mini_bufs[idx], xfer_size,dst_mini_bufs[idx]);
    task_node->tsk->desc->flags |= t_args->flags;
  }

  task_node = dsa->multi_task_node;
  uint64_t start = sampleCoderdtsc();

  while(task_node){
    acctest_desc_submit(dsa, task_node->tsk->desc);
    // while(task_node->tsk->comp->status == 0){
    //   _mm_pause();
    // }
    task_node = task_node->next;
  }
  uint64_t end = sampleCoderdtsc();
  while(task_node){
    // acctest_desc_submit(dsa, task_node->tsk->desc);
    while(task_node->tsk->comp->status == 0){
      _mm_pause();
    }
    task_node = task_node->next;
  }
  uint64_t cycles = end-start;
  acctest_free(dsa);

  return cycles;
}


int swq_test(){

  dwq_vs_shared_args *t_args = (dwq_vs_shared_args *)malloc(sizeof(dwq_vs_shared_args));
  t_args->num_bufs = 128;
  t_args->xfer_size = 256;
  t_args->flags = IDXD_OP_FLAG_CC;

  struct acctest_context *dsa;
	int rc = 0;
	unsigned long buf_size = DSA_TEST_SIZE;
	int opcode = DSA_OPCODE_MEMMOVE;
	int bopcode = DSA_OPCODE_MEMMOVE;
	int tflags = TEST_FLAGS_BOF;
	int opt;
	unsigned int bsize = 0;
	char dev_type[MAX_DEV_LEN];
	int wq_id = ACCTEST_DEVICE_ID_NO_INPUT;
	int dev_id = ACCTEST_DEVICE_ID_NO_INPUT;
	int dev_wq_id = ACCTEST_DEVICE_ID_NO_INPUT;
	unsigned int num_desc = 1;
	struct evl_desc_list *edl = NULL;
	char *edl_str = NULL;

  int wq_type = SHARED; /*  sudo ./setup_dsa.sh -d dsa0 -w1 -md -e1 */

  dsa = acctest_init(tflags);
	dsa->dev_type = ACCFG_DEVICE_DSA;

	if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;

  if (buf_size > dsa->max_xfer_size) {
		err("invalid transfer size: %lu\n", buf_size);
		return -EINVAL;
	}

  struct task_node * task_node;

  int num_bufs = t_args->num_bufs;
  int xfer_size = t_args->xfer_size;

  acctest_alloc_multiple_tasks(dsa, num_bufs);

  int idx=0;
  uint8_t **dsa_mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  uint8_t **dsa_dst_mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  for(int i=0; i<num_bufs; i++){
    dsa_mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, 0);
    dsa_dst_mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, 0);
    for(int j=0; j<xfer_size; j++){
      __builtin_prefetch((const void*) dsa_mini_bufs[i] + j);
      __builtin_prefetch((const void*) dsa_dst_mini_bufs[i] + j);
    }
  }
  while(task_node){
    prepare_memcpy_task(task_node->tsk, dsa,mini_bufs[idx], xfer_size,dst_mini_bufs[idx]);
    task_node->tsk->desc->flags |= t_args->flags;
  }

  task_node = dsa->multi_task_node;
  uint64_t start = sampleCoderdtsc();

  while(task_node){
    acctest_desc_submit(dsa, task_node->tsk->desc);
    // while(task_node->tsk->comp->status == 0){
    //   _mm_pause();
    // }
    task_node = task_node->next;
  }
  uint64_t end = sampleCoderdtsc();
  while(task_node){
    // acctest_desc_submit(dsa, task_node->tsk->desc);
    while(task_node->tsk->comp->status == 0){
      _mm_pause();
    }
    task_node = task_node->next;
  }
  uint64_t cycles = end-start;
  acctest_free(dsa);

  return cycles;
}

int dedicated_vs_shared_test(int shared){
  for(int i=0; i<10; i++){
  if(shared == 0){
    uint64_t dedicated_cycles = dwq_test();
    printf("Time taken for Dedicated 1024 256B offloads: %lu\n", dedicated_cycles);
  }

  if(shared == 1){
    uint64_t shared_cycles = swq_test();
    printf("Time taken for Shared 1024 256B offloads: %lu\n", shared_cycles);
  }
  }
}

int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;

  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];

  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);

  dwq_vs_shared_args *t_args = (dwq_vs_shared_args *)malloc(sizeof(dwq_vs_shared_args));
  t_args->num_bufs = 128;
  t_args->xfer_size = 256;
  t_args->flags = IDXD_OP_FLAG_CC;

  struct acctest_context *dsa;
	int rc = 0;
	unsigned long buf_size = DSA_TEST_SIZE;
	int opcode = DSA_OPCODE_MEMMOVE;
	int bopcode = DSA_OPCODE_MEMMOVE;
	int tflags = TEST_FLAGS_BOF;
	int opt;
	char dev_type[MAX_DEV_LEN];
	int wq_id = ACCTEST_DEVICE_ID_NO_INPUT;
	int dev_id = ACCTEST_DEVICE_ID_NO_INPUT;
	int dev_wq_id = ACCTEST_DEVICE_ID_NO_INPUT;
	unsigned int num_desc = 1;
	struct evl_desc_list *edl = NULL;
	char *edl_str = NULL;

  int wq_type = DEDICATED; /*  sudo ./setup_dsa.sh -d dsa0 -w1 -md -e1 */

  dsa = acctest_init(tflags);
	dsa->dev_type = ACCFG_DEVICE_DSA;

	if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;

  if (buf_size > dsa->max_xfer_size) {
		err("invalid transfer size: %lu\n", buf_size);
		return -EINVAL;
	}

  struct task_node * task_node;

  int num_bufs = t_args->num_bufs;
  int xfer_size = t_args->xfer_size;

  unsigned int bsize = 1;
  // acctest_alloc_multiple_tasks(dsa, num_bufs);
  /* test_batch: 41*/
  struct btask_node *btsk_node;
  struct acctest_context *ctx = dsa;
  ctx->is_batch = 1;

  btsk_node = ctx->multi_btask_node;
  rc = alloc_batch_task(ctx, bsize, num_bufs);
  int dflags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
  if ((tflags & TEST_FLAGS_BOF) && ctx->bof)
    dflags |= IDXD_OP_FLAG_BOF;


  int idx=0;
  uint8_t **dsa_mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  uint8_t **dsa_dst_mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  for(int i=0; i<num_bufs; i++){
    dsa_mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, 0);
    dsa_dst_mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, 0);
    for(int j=0; j<xfer_size; j++){
      __builtin_prefetch((const void*) dsa_mini_bufs[i] + j);
      __builtin_prefetch((const void*) dsa_dst_mini_bufs[i] + j);
    }
  }
  while(task_node){
    prepare_memcpy_task(task_node->tsk, dsa,mini_bufs[idx], xfer_size,dst_mini_bufs[idx]);
    task_node->tsk->desc->flags |= t_args->flags;
  }

  task_node = dsa->multi_task_node;
  uint64_t start = sampleCoderdtsc();

  while(task_node){
    acctest_desc_submit(dsa, task_node->tsk->desc);
    // while(task_node->tsk->comp->status == 0){
    //   _mm_pause();
    // }
    task_node = task_node->next;
  }
  uint64_t end = sampleCoderdtsc();
  while(task_node){
    // acctest_desc_submit(dsa, task_node->tsk->desc);
    while(task_node->tsk->comp->status == 0){
      _mm_pause();
    }
    task_node = task_node->next;
  }
  uint64_t cycles = end-start;
  acctest_free(dsa);

  return cycles;

exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}