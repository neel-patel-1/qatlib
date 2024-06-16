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

#include <math.h>


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
#include "fcontext.h"

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

/* Offload Component Location . h*/
typedef struct _alloc_td_args{
  int num_bufs;
  int xfer_size;
  int src_buf_node;
  int dst_buf_node;
  int flush_bufs;
  int prefault_bufs;
  pthread_barrier_t *alloc_sync;
} alloc_td_args;

void * buf_alloc_td(void *arg){
  alloc_td_args *args = (alloc_td_args *) arg;
  int num_bufs = args->num_bufs;
  int xfer_size = args->xfer_size;
  int src_buf_node = args->src_buf_node;
  int dst_buf_node = args->dst_buf_node;
  pthread_barrier_t *alloc_sync = args->alloc_sync;
  int flush_bufs = args->flush_bufs;
  int prefault_bufs = args->prefault_bufs;

  mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  dst_mini_bufs = malloc(sizeof(uint8_t *) * num_bufs);
  for(int i=0; i<num_bufs; i++){
    mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, src_buf_node);
    dst_mini_bufs[i] = (uint8_t *)numa_alloc_onnode(xfer_size, dst_buf_node);
    for(int j=0; j<xfer_size; j++){
      __builtin_prefetch((const void*) mini_bufs[i] + j);
      __builtin_prefetch((const void*) dst_mini_bufs[i] + j);
    }
    if(prefault_bufs){
      for(int j=0; j<xfer_size; j++){
        mini_bufs[i][j] = 0;
        dst_mini_bufs[i][j] = 0;
      }
    }
    if(flush_bufs){
      for(int j=0; j<xfer_size; j++){
        _mm_clflush(mini_bufs[i] + j);
        _mm_clflush(dst_mini_bufs[i] + j);
      }
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
  int num_bufs;
  int xfer_size;
  int wq_type;
  int prefault_crs;
  uint64_t *start_times;
  uint64_t *end_times;
  int idx;
  pthread_barrier_t *alloc_sync;
} mbuf_targs;

void *submit_thread(void *arg){
  mbuf_targs *t_args = (mbuf_targs *) arg;
  int desc_node = t_args->desc_node;
  int cr_node = t_args->cr_node;
  bool flush_desc = t_args->flush_desc;
  int prefault_crs = t_args->prefault_crs;

  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int dev_id = t_args->dev_id;
  int wq_id = 0;
  int opcode = 16;
  int wq_type = t_args->wq_type;
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
    prepare_memcpy_task(task_node->tsk, dsa, mini_bufs[idx], xfer_size, dst_mini_bufs[idx]);
    task_node->tsk->desc->flags |= t_args->flags;

    /* Flush task and src/dst */
    if(flush_desc){
      _mm_clflush(task_node->tsk->desc);
      _mm_clflush(task_node->tsk->comp);
    }
    if(prefault_crs){
      task_node->tsk->comp->status = 0;
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
  t_args->start_times[t_args->idx] = start;
  t_args->end_times[t_args->idx] = end;

  /* validate all tasks */
  task_node = dsa->multi_task_node;
  while(task_node){
    if(task_node->tsk->comp->status != DSA_COMP_SUCCESS){
      err("Task failed: 0x%x\n", task_node->tsk->comp->status);
    }
    task_node = task_node->next;
  }


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
    mprotect(tsk->src1, PAGE_SIZE, PROT_READ | PROT_WRITE);
    numa_free(tsk->src1, xfer_size);
    free(tsk->src2);
    numa_free(tsk->dst1, xfer_size);
    free(tsk->dst2);
    free(tsk->input);
    free(tsk->output);



    tsk_node->tsk = NULL;
    free(tsk_node);
    tsk_node = tmp_node;
  }
  dsa->multi_task_node = NULL;

  free(dsa);

}

CpaStatus offloadComponentLocationTest(){ // MAIN TEST Function
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
    targs.flush_desc = false;

    alloc_td_args args;
    args.num_bufs = 1024 * 10;
    args.xfer_size = xfer_size;
    args.alloc_sync = &alloc_sync;
    args.flush_bufs = false;


    /* DESCRIPTOR LOCATION TEST */
    pthread_barrier_init(&alloc_sync, NULL, 2);
    /* descriptors and completion records (offloader) is on local  */
    targs.flags = IDXD_OP_FLAG_CC;
    targs.desc_node = dsa_node;
    targs.cr_node = dsa_node;
    targs.wq_type = ACCFG_WQ_SHARED;

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

/* Thread Safety .h*/

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

/* stats .h */
static inline void gen_diff_array(uint64_t *dst_array, uint64_t* array1,  uint64_t* array2, int size)
{
  for(int i=0; i<size; i++){ dst_array[i] = array2[i] - array1[i]; }
}

#define do_sum_array(accum,array,iter) accum = 0; \
 for (int i=1; i<iter; i++){ accum+=array[i]; } \
 accum /= iter
#define do_avg(sum, itr) (sum/itr)

#define avg_samples_from_arrays(yield_to_submit, avg, before_yield, before_submit, num_samples) \
  gen_diff_array(yield_to_submit, before_submit, before_yield, num_samples); \
  do_sum_array(avg, yield_to_submit, num_samples); \
  do_avg(avg, num_samples);


/* Waiting Styles .h */

static __always_inline void umonitor(const volatile void *addr)
{
	asm volatile(".byte 0xf3, 0x48, 0x0f, 0xae, 0xf0" : : "a"(addr));
}

static __always_inline int umwait(unsigned long timeout, unsigned int state)
{
	uint8_t r;
	uint32_t timeout_low = (uint32_t)timeout;
	uint32_t timeout_high = (uint32_t)(timeout >> 32);

	asm volatile(".byte 0xf2, 0x48, 0x0f, 0xae, 0xf1\t\n"
		"setc %0\t\n"
		: "=r"(r)
		: "c"(state), "a"(timeout_low), "d"(timeout_high));
	return r;
}

static __always_inline int wait_umwait(struct completion_record *comp)
{
	int r = 1;
  while(comp->status == 0){
    umonitor((uint8_t *)comp);
    if (comp->status != 0)
				break;
    r = umwait(0, 0);
  }
}

static __always_inline int wait_pause(struct completion_record *comp)
{
	int r = 1;
  while(comp->status == 0){
    _mm_pause();
  }
}

static __always_inline int wait_spin(struct completion_record *comp)
{
	int r = 1;
  while(comp->status == 0){  }
}

struct completion_record *comp;
uint64_t *start;
uint64_t *end;
pthread_barrier_t *tdSync;
enum wait_style {
  UMWAIT,
  PAUSE,
  SPIN
};
char *wait_style_str[] = {"UMWAIT", "PAUSE", "SPIN"};
enum wait_style *style;

void *wakeup_thread(void *arg){
  uint64_t startStamp;
  pthread_barrier_wait(tdSync);
  startStamp = sampleCoderdtsc();
  comp->status = 1;

  *start = startStamp;
}

void *waiter_thread(void *arg){
  uint64_t endStamp;
  pthread_barrier_wait(tdSync);
  switch(*style){
    case UMWAIT:
      wait_umwait(comp);
      break;
    case PAUSE:
      wait_pause(comp);
      break;
    case SPIN:
      wait_spin(comp);
      break;
  }
  endStamp = sampleCoderdtsc();
  *end = endStamp;
}

int compare_wait_styles(){
  int num_samples = 1000;
  uint64_t cycleCtrs[num_samples];
  uint64_t startTimes[num_samples];
  uint64_t endTimes[num_samples];
  uint64_t avg =0;

  pthread_t wakeupThread, waiterThread;
  tdSync = (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));

  start = malloc(sizeof(uint64_t));
  end = malloc(sizeof(uint64_t));
  comp = malloc(sizeof(struct completion_record));
  style = malloc(sizeof(enum wait_style));
  comp->status = 0;
  for(enum wait_style st=UMWAIT; st<=SPIN; st++){
    *style = st;
    for(int itr=0; itr<num_samples; itr++){
      pthread_barrier_init(tdSync, NULL, 2);
      createThreadPinned(&waiterThread, waiter_thread, NULL, 30);
      createThreadPinned(&wakeupThread, wakeup_thread, NULL, 10);
      pthread_join(wakeupThread, NULL);
      pthread_join(waiterThread, NULL);
      startTimes[itr] = *start;
      endTimes[itr] = *end;
    }
    avg_samples_from_arrays(cycleCtrs, avg, endTimes, startTimes, num_samples);
    printf("%s: %ld\n", wait_style_str[st], avg);
  }

}

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

int componentLocationSocket1(){
  int num_samples = 1000;
  uint64_t cycleCtrs[num_samples];
  uint64_t start_times[num_samples];
  uint64_t end_times[num_samples];
  uint64_t run_times[num_samples];
  uint64_t avg =0;
  /* DRAM, LLC */
  int dev_id = 2;
  int dsa_node = 1;
  int remote_node = 0;

  pthread_t submitThread, allocThread;
  pthread_barrier_t alloc_sync;
  int xfer_size=256;

  mbuf_targs targs;
  targs.dev_id = dev_id;
  targs.xfer_size = xfer_size;
  targs.num_bufs = 128;
  targs.alloc_sync = &alloc_sync;
  targs.wq_type = ACCFG_WQ_DEDICATED;
  targs.flags = IDXD_OP_FLAG_CC;

  alloc_td_args args;
  args.num_bufs = 128;
  args.xfer_size = xfer_size;
  args.alloc_sync = &alloc_sync;
  args.src_buf_node = dsa_node;
  args.dst_buf_node = dsa_node;

  for(int i=0; i<num_samples; i++){
    args.flush_bufs = false;
    targs.desc_node = dsa_node;
    targs.cr_node = dsa_node;
    targs.flush_desc = false;
    targs.idx = i;
    targs.start_times = start_times;
    targs.end_times = end_times;

    pthread_barrier_init(&alloc_sync, NULL, 2);
    createThreadPinned(&allocThread,buf_alloc_td,&args,20);
    createThreadPinned(&submitThread,submit_thread,&targs,20);
    pthread_join(submitThread,NULL);
  }
  avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
  PRINT("All-Cached: %ld\n", avg);

  for(int i=0; i<num_samples; i++){
    args.flush_bufs = true;
    targs.desc_node = dsa_node;
    targs.cr_node = dsa_node;
    targs.flush_desc = false;
    targs.idx = i;
    targs.start_times = start_times;
    targs.end_times = end_times;

    pthread_barrier_init(&alloc_sync, NULL, 2);
    createThreadPinned(&allocThread,buf_alloc_td,&args,20);
    createThreadPinned(&submitThread,submit_thread,&targs,20);
    pthread_join(submitThread,NULL);
  }
  avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
  PRINT("Payload-DRAM: %ld\n", avg);

  for(int i=0; i<num_samples; i++){
    args.flush_bufs = false;
    targs.desc_node = dsa_node;
    targs.cr_node = dsa_node;
    targs.flush_desc = true;
    targs.idx = i;
    targs.start_times = start_times;
    targs.end_times = end_times;

    pthread_barrier_init(&alloc_sync, NULL, 2);
    createThreadPinned(&allocThread,buf_alloc_td,&args,20);
    createThreadPinned(&submitThread,submit_thread,&targs,20);
    pthread_join(submitThread,NULL);
  }
  avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
  PRINT("Descriptor-DRAM: %ld\n", avg);

  for(int i=0; i<num_samples; i++){
    args.flush_bufs = true;
    targs.desc_node = dsa_node;
    targs.cr_node = dsa_node;
    targs.flush_desc = true;
    targs.idx = i;
    targs.start_times = start_times;
    targs.end_times = end_times;

    pthread_barrier_init(&alloc_sync, NULL, 2);
    createThreadPinned(&allocThread,buf_alloc_td,&args,20);
    createThreadPinned(&submitThread,submit_thread,&targs,20);
    pthread_join(submitThread,NULL);
  }
  avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
  PRINT("All-DRAM: %ld\n", avg);
}

/* bof.h */

int pageFaultImpact(){
  int num_samples = 1000;
  uint64_t cycleCtrs[num_samples];
  uint64_t start_times[num_samples];
  uint64_t end_times[num_samples];
  uint64_t run_times[num_samples];
  uint64_t avg =0;



  /* DRAM, LLC */
  int dev_id = 2;
  int dsa_node = 1;
  int remote_node = 0;

  pthread_t submitThread, allocThread;
  pthread_barrier_t alloc_sync;
  int xfer_size=256;

  mbuf_targs targs;
  targs.dev_id = dev_id;
  targs.xfer_size = xfer_size;
  targs.num_bufs = 128;
  targs.alloc_sync = &alloc_sync;
  targs.wq_type = ACCFG_WQ_DEDICATED;
  targs.flags = IDXD_OP_FLAG_CC;

  alloc_td_args args;
  args.num_bufs = 128;
  args.xfer_size = xfer_size;
  args.alloc_sync = &alloc_sync;
  args.src_buf_node = dsa_node;
  args.dst_buf_node = dsa_node;

  for(int i=0; i<num_samples; i++){
    args.flush_bufs = true;
    args.prefault_bufs = true;
    targs.desc_node = dsa_node;
    targs.cr_node = dsa_node;
    targs.flush_desc = true;
    targs.idx = i;
    targs.start_times = start_times;
    targs.end_times = end_times;

    pthread_barrier_init(&alloc_sync, NULL, 2);
    createThreadPinned(&allocThread,buf_alloc_td,&args,20);
    createThreadPinned(&submitThread,submit_thread,&targs,20);
    pthread_join(submitThread,NULL);
  }
  avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
  PRINT("Prefault: %ld\n", avg);

  for(int i=0; i<num_samples; i++){
    args.flush_bufs = true;
    args.prefault_bufs = false;
    targs.desc_node = dsa_node;
    targs.cr_node = dsa_node;
    targs.flush_desc = true;
    targs.idx = i;
    targs.start_times = start_times;
    targs.end_times = end_times;

    pthread_barrier_init(&alloc_sync, NULL, 2);
    createThreadPinned(&allocThread,buf_alloc_td,&args,20);
    createThreadPinned(&submitThread,submit_thread,&targs,20);
    pthread_join(submitThread,NULL);
  }
  avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
  PRINT("No Prefault: %ld\n", avg);

}

/* offload component.h */

int offloadComponentLocSync(){
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];

  int num_samples = 1000;
  uint64_t cycleCtrs[num_samples];
  uint64_t start_times[num_samples];
  uint64_t end_times[num_samples];
  uint64_t run_times[num_samples];
  uint64_t avg =0;

  /* DRAM, LLC */
  int dev_id = 2;
  int dsa_node = 1;
  int remote_node = 0;

  pthread_t submitThread, allocThread;
  pthread_barrier_t alloc_sync;
  for( int xfer_size=256; xfer_size <= 1024; xfer_size*=2){
    PRINT("Xfer_size: %d\n", xfer_size);
    mbuf_targs targs;
    targs.dev_id = dev_id;
    targs.xfer_size = xfer_size;
    targs.num_bufs = 128;
    targs.alloc_sync = &alloc_sync;
    targs.wq_type = ACCFG_WQ_DEDICATED;
    targs.flags = IDXD_OP_FLAG_CC;

    alloc_td_args args;
    args.num_bufs = 128;
    args.xfer_size = xfer_size;
    args.alloc_sync = &alloc_sync;
    args.src_buf_node = dsa_node;
    args.dst_buf_node = dsa_node;


    for(int i=0; i<num_samples; i++){
      args.flush_bufs = true;
      args.prefault_bufs = true;
      targs.desc_node = dsa_node;
      targs.cr_node = dsa_node;
      targs.flush_desc = true;
      targs.idx = i;
      targs.start_times = start_times;
      targs.end_times = end_times;

      pthread_barrier_init(&alloc_sync, NULL, 2);
      createThreadPinned(&allocThread,buf_alloc_td,&args,20);
      createThreadPinned(&submitThread,submit_thread,&targs,20);
      pthread_join(submitThread,NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("FlushBuf-FlushDesc: %ld\n", avg);

    for(int i=0; i<num_samples; i++){
      args.flush_bufs = true;
      args.prefault_bufs = true;
      targs.desc_node = dsa_node;
      targs.cr_node = dsa_node;
      targs.flush_desc = false;
      targs.idx = i;
      targs.start_times = start_times;
      targs.end_times = end_times;

      pthread_barrier_init(&alloc_sync, NULL, 2);
      createThreadPinned(&allocThread,buf_alloc_td,&args,20);
      createThreadPinned(&submitThread,submit_thread,&targs,20);
      pthread_join(submitThread,NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("FlushBuf: %ld\n", avg);

    for(int i=0; i<num_samples; i++){
      args.flush_bufs = false;
      args.prefault_bufs = true;
      targs.desc_node = dsa_node;
      targs.cr_node = dsa_node;
      targs.flush_desc = true;
      targs.idx = i;
      targs.start_times = start_times;
      targs.end_times = end_times;

      pthread_barrier_init(&alloc_sync, NULL, 2);
      createThreadPinned(&allocThread,buf_alloc_td,&args,20);
      createThreadPinned(&submitThread,submit_thread,&targs,20);
      pthread_join(submitThread,NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("FlushDesc: %ld\n", avg);

    for(int i=0; i<num_samples; i++){
      args.flush_bufs = false;
      args.prefault_bufs = true;
      targs.desc_node = dsa_node;
      targs.cr_node = dsa_node;
      targs.flush_desc = true;
      targs.idx = i;
      targs.start_times = start_times;
      targs.end_times = end_times;

      pthread_barrier_init(&alloc_sync, NULL, 2);
      createThreadPinned(&allocThread,buf_alloc_td,&args,20);
      createThreadPinned(&submitThread,submit_thread,&targs,20);
      pthread_join(submitThread,NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("FlushNone: %ld\n", avg);
  }
}

/* enqcmd_qpi.h */
void *enqcmd_submission_latency(void *arg){
  int num_samples = 1000;
  uint64_t cycleCtrs[num_samples];
  uint64_t start_times[num_samples];
  uint64_t end_times[num_samples];
  uint64_t run_times[num_samples];
  uint64_t avg =0;

  /* Use a single descriptor -- prefetch it, submit it repeatedly */

  int dev_id = 2;
  int wq_id = 0;
  pthread_t submitThread, allocThread;

  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int wq_type = ACCFG_WQ_SHARED;
  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;
  if (!dsa)
    return -ENOMEM;

  int rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
  if (rc < 0)
    return -ENOMEM;

  struct task_node * task_node;

  int num_bufs = 128;
  int xfer_size = 256;

  acctest_alloc_multiple_tasks(dsa, num_bufs);

  /* buffers */
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

  /* descs */
  task_node = dsa->multi_task_node;
  while(task_node){
    prepare_memcpy_task(task_node->tsk, dsa,dsa_mini_bufs[idx], xfer_size,dsa_dst_mini_bufs[idx]);
    task_node->tsk->desc->flags |= IDXD_OP_FLAG_CC;
    idx++;
    task_node = task_node->next;
  }

  for(int idx=0; idx<num_samples; idx++){
    task_node = dsa->multi_task_node;
    int retry = 0;
    uint64_t start = sampleCoderdtsc();
    while(task_node){
      retry = enqcmd(dsa->wq_reg, task_node->tsk->desc);
      if(retry){
        err("Failed to enq\n");
        exit(-1);
      }
      task_node = task_node->next;
    }
    uint64_t end = sampleCoderdtsc();
    start_times[idx] = start;
    end_times[idx] = end;

    uint64_t cycles = end-start;
    cycleCtrs[idx] = cycles;


    task_node = dsa->multi_task_node;
    while(task_node){
      while(task_node->tsk->comp->status == 0){
        _mm_pause();
      }
      task_node = task_node->next;
    }

    /* validate all tasks */
    task_node = dsa->multi_task_node;
    while(task_node){
      if(task_node->tsk->comp->status != DSA_COMP_SUCCESS){
        err("Task failed: 0x%x\n", task_node->tsk->comp->status);
      }
      task_node = task_node->next;
    }
  }
  avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
  PRINT("EnqCmdSubmit: %ld\n", avg);

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
    mprotect(tsk->src1, PAGE_SIZE, PROT_READ | PROT_WRITE);
    numa_free(tsk->src1, xfer_size);
    free(tsk->src2);
    numa_free(tsk->dst1, xfer_size);
    free(tsk->dst2);
    free(tsk->input);
    free(tsk->output);



    tsk_node->tsk = NULL;
    free(tsk_node);
    tsk_node = tmp_node;
  }
  dsa->multi_task_node = NULL;

  free(dsa);

  /* Later on: Pin to different cores and submit 128 descs to the device */


  /* enqcmds from socket 0 to socket 1 should take longer */

  /* we will taskset to begin with */

  /* check if !retry, we don't want the impact of a failed enq */

  /* wait and check all offloads == 1*/
}

/* movdir64b submission latency.h*/
void *movdir64b_submission_latency(void *arg){
  int num_samples = 1000;
  uint64_t cycleCtrs[num_samples];
  uint64_t start_times[num_samples];
  uint64_t end_times[num_samples];
  uint64_t run_times[num_samples];
  uint64_t avg =0;

  /* Use a single descriptor -- prefetch it, submit it repeatedly */

  int dev_id = 2;
  int wq_id = 0;
  pthread_t submitThread, allocThread;

  struct acctest_context *dsa = NULL;
  int tflags = TEST_FLAGS_BOF;
  int wq_type = ACCFG_WQ_SHARED;
  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;
  if (!dsa)
    return -ENOMEM;

  int rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
  if (rc < 0)
    return -ENOMEM;

  struct task_node * task_node;

  int num_bufs = 128;
  int xfer_size = 256;

  acctest_alloc_multiple_tasks(dsa, num_bufs);

  /* buffers */
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

  /* descs */
  task_node = dsa->multi_task_node;
  while(task_node){
    prepare_memcpy_task(task_node->tsk, dsa,dsa_mini_bufs[idx], xfer_size,dsa_dst_mini_bufs[idx]);
    task_node->tsk->desc->flags |= IDXD_OP_FLAG_CC;
    idx++;
    task_node = task_node->next;
  }

  for(int idx=0; idx<num_samples; idx++){
    task_node = dsa->multi_task_node;
    int retry = 0;
    uint64_t start = sampleCoderdtsc();
    while(task_node){
      movdir64b(dsa->wq_reg, task_node->tsk->desc);

      task_node = task_node->next;
    }
    uint64_t end = sampleCoderdtsc();
    start_times[idx] = start;
    end_times[idx] = end;

    uint64_t cycles = end-start;
    cycleCtrs[idx] = cycles;


    task_node = dsa->multi_task_node;
    while(task_node){
      while(task_node->tsk->comp->status == 0){
        _mm_pause();
      }
      task_node = task_node->next;
    }

    /* validate all tasks */
    task_node = dsa->multi_task_node;
    while(task_node){
      if(task_node->tsk->comp->status != DSA_COMP_SUCCESS){
        err("Task failed: 0x%x\n", task_node->tsk->comp->status);
      }
      task_node = task_node->next;
    }
  }
  avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
  PRINT("MovDir64BSubmit: %ld\n", avg);

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
    mprotect(tsk->src1, PAGE_SIZE, PROT_READ | PROT_WRITE);
    numa_free(tsk->src1, xfer_size);
    free(tsk->src2);
    numa_free(tsk->dst1, xfer_size);
    free(tsk->dst2);
    free(tsk->input);
    free(tsk->output);



    tsk_node->tsk = NULL;
    free(tsk_node);
    tsk_node = tmp_node;
  }
  dsa->multi_task_node = NULL;

  free(dsa);

  /* Later on: Pin to different cores and submit 128 descs to the device */


  /* enqcmds from socket 0 to socket 1 should take longer */

  /* we will taskset to begin with */

  /* check if !retry, we don't want the impact of a failed enq */

  /* wait and check all offloads == 1*/
}

/* device_descriptor_access_test.h */
int init_memcpy_on_node(struct task *tsk, int tflags, int opcode, unsigned long xfer_size, int node)
{
	unsigned long force_align = PAGE_SIZE;

	tsk->opcode = opcode;
	tsk->test_flags = tflags;
	tsk->xfer_size = xfer_size;

	tsk->src1 = numa_alloc_onnode(xfer_size, node);
	if (!tsk->src1)
		return -ENOMEM;

	tsk->dst1 = numa_alloc_onnode(xfer_size, node);
	if (!tsk->dst1)
		return -ENOMEM;

	return ACCTEST_STATUS_OK;
}

int init_batch_memcpy_task_onnode(struct batch_task *btsk, int task_num, int tflags,
		    int opcode, unsigned long xfer_size, unsigned long dflags, int node)
{
	int i, rc;

	btsk->task_num = task_num;
	btsk->test_flags = tflags;

	for (i = 0; i < task_num; i++) {
		btsk->sub_tasks[i].desc = &btsk->sub_descs[i];
		if (btsk->edl)
			btsk->sub_tasks[i].comp = &btsk->sub_comps[(PAGE_SIZE * i) /
				sizeof(struct completion_record)];
		else
			btsk->sub_tasks[i].comp = &btsk->sub_comps[i];
		btsk->sub_tasks[i].dflags = dflags;
		rc = init_memcpy_on_node(&btsk->sub_tasks[i], tflags, opcode, xfer_size, node);
		if (rc != ACCTEST_STATUS_OK) {
			err("batch: init sub-task failed\n");
			return rc;
		}
	}

	return ACCTEST_STATUS_OK;
}

int init_and_prep_noop_btsk(struct batch_task *btsk, int task_num, int tflags, int node){
  int i, rc = ACCTEST_STATUS_OK;
  struct task *sub_task;
	uint32_t dflags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;

	btsk->task_num = task_num;
	btsk->test_flags = tflags;

	for (i = 0; i < task_num; i++) {
      btsk->sub_tasks[i].desc = &btsk->sub_descs[i];
      if (btsk->edl)
        btsk->sub_tasks[i].comp = &btsk->sub_comps[(PAGE_SIZE * i) /
          sizeof(struct completion_record)];
      else
        btsk->sub_tasks[i].comp = &btsk->sub_comps[i];
      btsk->sub_tasks[i].dflags = dflags;
      rc = init_task(&btsk->sub_tasks[i], tflags, 0, 0);
      if (rc != ACCTEST_STATUS_OK) {
        err("batch: init sub-task failed\n");
        return rc;
      }
  }
  for (i = 0; i < btsk->task_num; i++) {
		sub_task = &btsk->sub_tasks[i];
		acctest_prep_desc_common(sub_task->desc, sub_task->opcode,
					 (uint64_t)(sub_task->dst1),
					 (uint64_t)(sub_task->src1),
					 0, dflags);
		sub_task->desc->completion_addr = (uint64_t)(sub_task->comp);
		sub_task->comp->status = 0;
	}
  return rc;
}

int alloc_batch_task_on_node(struct acctest_context *ctx, unsigned int task_num, int num_itr, int node)
{
	struct btask_node *btsk_node;
	struct batch_task *btsk;
	int cnt = 0;
	int prot = PROT_READ | PROT_WRITE;
	int mmap_flags = MAP_ANONYMOUS | MAP_PRIVATE;

	if (!ctx->is_batch) {
		err("%s is valid only if 'is_batch' is enabled", __func__);
		return -EINVAL;
	}

	while (cnt < num_itr) {
		btsk_node = ctx->multi_btask_node;

		ctx->multi_btask_node = (struct btask_node *)
			malloc(sizeof(struct btask_node));
		if (!ctx->multi_btask_node)
			return -ENOMEM;

		ctx->multi_btask_node->btsk = malloc(sizeof(struct batch_task));
		if (!ctx->multi_btask_node->btsk)
			return -ENOMEM;
		memset(ctx->multi_btask_node->btsk, 0, sizeof(struct batch_task));

		btsk = ctx->multi_btask_node->btsk;

		btsk->core_task = acctest_alloc_task(ctx);
		if (!btsk->core_task)
			return -ENOMEM;

		btsk->sub_tasks = malloc(task_num * sizeof(struct task));
		if (!btsk->sub_tasks)
			return -ENOMEM;
		memset(btsk->sub_tasks, 0, task_num * sizeof(struct task));

		if (ctx->is_evl_test) {
			btsk->sub_descs = mmap(NULL, PAGE_ALIGN(PAGE_SIZE +
					       task_num * sizeof(struct hw_desc)),
					       prot, mmap_flags, -1, 0);
			if (!btsk->sub_descs)
				return -ENOMEM;
			memset(btsk->sub_descs, 0, PAGE_ALIGN(PAGE_SIZE +
			       task_num * sizeof(struct hw_desc)));

			btsk->sub_comps = numa_alloc_onnode(task_num * PAGE_SIZE, node);
			if (!btsk->sub_comps)
				return -ENOMEM;
			memset(btsk->sub_comps, 0,
			       task_num * PAGE_SIZE);
		} else	{
			btsk->sub_descs = numa_alloc_onnode(task_num * sizeof(struct hw_desc), node);
			if (!btsk->sub_descs)
				return -ENOMEM;
			memset(btsk->sub_descs, 0, task_num * sizeof(struct hw_desc));

			/* IAX completion record need to be 64-byte aligned */
			btsk->sub_comps =
				numa_alloc_onnode(task_num * sizeof(struct completion_record), node);
			if (!btsk->sub_comps)
				return -ENOMEM;
			memset(btsk->sub_comps, 0,
			       task_num * sizeof(struct completion_record));
		}

		dbg("batch task allocated %#lx, ctask %#lx, sub_tasks %#lx\n",
		    btsk, btsk->core_task, btsk->sub_tasks);
		dbg("sub_descs %#lx, sub_comps %#lx\n",
		    btsk->sub_descs, btsk->sub_comps);
		ctx->multi_btask_node->next = btsk_node;
		cnt++;
	}

	return ACCTEST_STATUS_OK;
}

typedef struct _batch_perf_test_args{
  int devid;
  int wq_id;
  int node;
  int bsize;
  int num_batches;
  int xfer_size;
  uint64_t *start_times;
  uint64_t *end_times;
  int idx;
  int direct_sub;
  int t_opcode;
  int flush_descs;
  int preftch_descs;
  int flush_payload;
} batch_perf_test_args;

void *batch_memcpy(void *arg){
  batch_perf_test_args *args = (batch_perf_test_args *)arg;

  struct acctest_context *dsa;
	int rc = 0;
	unsigned long buf_size = DSA_TEST_SIZE;
	int wq_type = -1;
	int opcode = DSA_OPCODE_BATCH;
	int bopcode = args->t_opcode;
	int tflags = TEST_FLAGS_BOF ;
	int opt;
	unsigned int bsize = args->bsize;
	char dev_type[MAX_DEV_LEN];
	int wq_id = args->wq_id;
	int dev_id = args->devid;
	int dev_wq_id = ACCTEST_DEVICE_ID_NO_INPUT;
	unsigned int num_desc = args->num_batches;
	struct evl_desc_list *edl = NULL;
	char *edl_str = NULL;

  /* test batch*/
  buf_size = args->xfer_size;
  edl = NULL;
  num_desc = args->num_batches; /*num batches*/
  int node = args->node;

  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;
  if (!dsa)
    return -ENOMEM;
  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
  if (rc < 0)
    return -ENOMEM;

  struct acctest_context *ctx;
  struct btask_node *btsk_node;
  int dflags;
  int batch_tsk_idx = 0;
  ctx = dsa;
  ctx->is_batch = 1;
  rc = alloc_batch_task_on_node(ctx, bsize, num_desc, node);
  dflags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR ;
  if (! (tflags & TEST_FLAGS_BOF) && ctx->bof){
    PRINT_ERR("BOF not set\n");
    return -EINVAL;
  }
  btsk_node = ctx->multi_btask_node;
  while(btsk_node){
    struct batch_task *btsk = btsk_node->btsk;
    int task_num = bsize;

    if(bopcode == DSA_OPCODE_MEMMOVE){
      rc = init_batch_memcpy_task_onnode(btsk, bsize, tflags, bopcode, buf_size, dflags, node);

      /* write prefault payloads and crs */
      for(int j=0; j<task_num; j++){
        for(int i=0; i<buf_size; i++){
          ((char *)(btsk->sub_tasks[j].src1))[i] = 0x1;
          ((char *)(btsk->sub_tasks[j].dst1))[i] = 0x2;
        }
        btsk->core_task->comp->status = 0;
      }

      dsa_prep_batch_memcpy(btsk);
    } else if(bopcode == DSA_OPCODE_NOOP){
      rc = init_and_prep_noop_btsk(btsk,task_num,tflags,node);
    }

    if(args->flush_payload == 1){
      /* flush all payloads, descs and crs */
      for(int i=0; i<task_num; i++){
        for(int j=0; j<buf_size; j++){
          _mm_clflush((const void *)btsk->sub_tasks[i].src1 + j);
          _mm_clflush((const void *)btsk->sub_tasks[i].dst1 + j);
        }
      }
    }
    if(args->flush_descs == 1){
          /* flush all descs and crs */
      for(int i=0; i<task_num; i++){
        _mm_clflush((const void *)btsk->sub_tasks[i].comp);
        _mm_clflush((const void *)btsk->sub_tasks[i].desc);
      }
    }
    if(args->preftch_descs == 1){
      /* prefetch all descs and crs */
      for(int i=0; i<task_num; i++){
        __builtin_prefetch((const void *)btsk->sub_tasks[i].comp);
        __builtin_prefetch((const void *)btsk->sub_tasks[i].desc);
      }
    }
    btsk_node = btsk_node->next;
  }

  btsk_node = ctx->multi_btask_node;
  while (btsk_node) {
    dsa_prep_batch(btsk_node->btsk, dflags);
    dump_sub_desc(btsk_node->btsk);
    btsk_node = btsk_node->next;
  }

  /* Submit and check */
  btsk_node = ctx->multi_btask_node;
  uint64_t start, end;
  if(args->direct_sub == 0){
    start = sampleCoderdtsc();
    while (btsk_node) {
      acctest_desc_submit(ctx, btsk_node->btsk->core_task->desc);
      while(btsk_node->btsk->core_task->comp->status == 0){
        _mm_pause();
      }

      btsk_node = btsk_node->next;
    }
    end = sampleCoderdtsc();
  } else {
    /* (1) Is iterating through the b-task nodes and just submitting the sub-tasks easier/faster ?*/
    start = sampleCoderdtsc();
    while (btsk_node) {
      for(int j=0; j<bsize; j++){
        acctest_desc_submit(ctx, btsk_node->btsk->sub_tasks[j].desc);

      }
      while(btsk_node->btsk->sub_tasks[bsize-1].comp->status == 0){
        _mm_pause();
      }
      btsk_node = btsk_node->next;
    }
    end = sampleCoderdtsc();
  }

  args->start_times[args->idx] = start;
  args->end_times[args->idx] = end;


  /* validate the payloads and completions */
  btsk_node = ctx->multi_btask_node;
  while (btsk_node) {
    for(int j=0; j<bsize; j++){
      if(args->direct_sub){
        rc = btsk_node->btsk->sub_tasks[j].comp->status;
        if(rc != DSA_COMP_SUCCESS){
          PRINT_ERR("Task failed: 0x%x\n", rc);
        }
      } else {
        rc = btsk_node->btsk->sub_tasks[j].comp->status;
        if(rc != DSA_COMP_SUCCESS){
          PRINT_ERR("Task failed: 0x%x\n", rc);
        }
      }
    }
    btsk_node = btsk_node->next;
  }
  if(bopcode == DSA_OPCODE_MEMMOVE){
    btsk_node = ctx->multi_btask_node;
    while (btsk_node) {
      for(int j=0; j<bsize; j++){
        for(int i=0; i<buf_size; i++){
          char src = ((char *)(btsk_node->btsk->sub_tasks[j].src1))[i];
          char dst = ((char *)(btsk_node->btsk->sub_tasks[j].dst1))[i];
          if(src != 0x1 || dst != 0x1){
            PRINT_ERR("Payload mismatch: 0x%x 0x%x\n", src, dst);
            // return -EINVAL;
          }
        }
      }
      btsk_node = btsk_node->next;
    }
  }

  /* free numa batch task*/
  struct btask_node *tsk_node = NULL, *tmp_node = NULL;
  tsk_node = ctx->multi_btask_node;
  while (tsk_node) {
    tmp_node = tsk_node->next;

    /* free descriptors */
    numa_free(tsk_node->btsk->sub_descs, bsize * sizeof(struct hw_desc));

    /* free comps */
    numa_free(tsk_node->btsk->sub_comps, bsize * sizeof(struct completion_record));

    /* free payloads */
    for(int i=0; i<bsize; i++){
      numa_free(tsk_node->btsk->sub_tasks[i].src1, buf_size);
      numa_free(tsk_node->btsk->sub_tasks[i].dst1, buf_size);
    }

    /* free sub tasks */
    free(tsk_node->btsk->sub_tasks);

    /* free batch task*/
    free(tsk_node->btsk);

    tsk_node->btsk = NULL;
    free(tsk_node);
    tsk_node = tmp_node;
  }
  // free(ctx->multi_btask_node);
  ctx->multi_task_node = NULL;

  free(dsa);
  // acctest_free(dsa);
}

/* direct_vs_indirect .h */
int direct_vs_indirect(){
  int num_samples = 1000;
  uint64_t start_times[num_samples];
  uint64_t end_times[num_samples];
  uint64_t run_times[num_samples];
  uint64_t avg = 0;


  pthread_t batchThread;
  batch_perf_test_args args;
  args.num_batches = 1;
  args.xfer_size = 256;
  args.start_times = start_times;
  args.end_times = end_times;
  args.devid = 2;
  args.wq_id = 0;

  args.t_opcode = DSA_OPCODE_NOOP;
  args.flush_payload = 0;

  for(int batch_size=2; batch_size<=32; batch_size*=2){
    PRINT("Batch_size:%d\n", batch_size);
    args.bsize = batch_size;

    args.node = 1;
    args.direct_sub = 0;
    args.preftch_descs = 1;
    args.flush_descs = 0;
    for(int i=0; i<num_samples; i++){
      args.idx = i;
      createThreadPinned(&batchThread, batch_memcpy, &args, 30);
      pthread_join(batchThread, NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("Batch-Local-LLC: %ld\n", avg);

    args.node = 1;
    args.direct_sub = 0;
    args.preftch_descs = 0;
    args.flush_descs = 1;
    for(int i=0; i<num_samples; i++){
      args.idx = i;
      createThreadPinned(&batchThread, batch_memcpy, &args, 30);
      pthread_join(batchThread, NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("Batch-Local-DRAM: %ld\n", avg);

    args.node = 0;
    args.direct_sub = 0;
    args.preftch_descs = 1;
    args.flush_descs = 0;
    args.node = 0;
    for(int i=0; i<num_samples; i++){
      args.idx = i;
      createThreadPinned(&batchThread, batch_memcpy, &args, 10);
      pthread_join(batchThread, NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("Batch-Far-LLC: %ld\n", avg);

    args.node = 0;
    args.direct_sub = 0;
    args.preftch_descs = 0;
    args.flush_descs = 1;
    args.node = 0;
    for(int i=0; i<num_samples; i++){
      args.idx = i;
      createThreadPinned(&batchThread, batch_memcpy, &args, 10);
      pthread_join(batchThread, NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("Batch-Far-DRAM: %ld\n", avg);


    args.node = 1;
    args.direct_sub = 1;
    args.preftch_descs = 1;
    args.flush_descs = 0;
    for(int i=0; i<num_samples; i++){
      args.idx = i;
      createThreadPinned(&batchThread, batch_memcpy, &args, 30);
      pthread_join(batchThread, NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("DirectSub-Local-LLC: %ld\n", avg);

    args.node = 1;
    args.direct_sub = 1;
    args.preftch_descs = 0;
    args.flush_descs = 1;
    for(int i=0; i<num_samples; i++){
      args.idx = i;
      createThreadPinned(&batchThread, batch_memcpy, &args, 30);
      pthread_join(batchThread, NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("DirectSub-Local-DRAM: %ld\n", avg);

    args.node = 0;
    args.direct_sub = 1;
    args.preftch_descs = 1;
    args.flush_descs = 0;
    for(int i=0; i<num_samples; i++){
      args.idx = i;
      createThreadPinned(&batchThread, batch_memcpy, &args, 10);
      pthread_join(batchThread, NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("DirectSub-Far-LLC: %ld\n", avg);

    args.node = 0;
    args.direct_sub = 1;
    args.preftch_descs = 0;
    args.flush_descs = 1;
    for(int i=0; i<num_samples; i++){
      args.idx = i;
      createThreadPinned(&batchThread, batch_memcpy, &args, 10);
      pthread_join(batchThread, NULL);
    }
    avg_samples_from_arrays(run_times,avg, end_times, start_times, num_samples);
    PRINT("DirectSub-Far-DRAM: %ld\n", avg);
  }
}

/* fcontext_switch.h */
int num_requests =  1;
int ret_val;
struct acctest_context *dsa = NULL;

typedef struct _request_args{
  int idx;
  uint64_t *start_times;
  uint64_t *end_times;
  struct completion_record *comp;
} request_args;

void yield_offload_request(fcontext_transfer_t arg) {
    fcontext_t parent = arg.prev_context;
    /* Offload, yield, post process*/
    uint8_t *src = (uint8_t *)malloc(16*1024);
    uint8_t *dst = (uint8_t *)malloc(16*1024);
    request_args *r_arg = (request_args *)(arg.data);

    /* populate the bufs */
    for(int i=0; i<16*1024; i++){
      src[i] = 0x1;
      dst[i] = 0x2;
    }
    struct task *tsk = acctest_alloc_task(dsa);
    prepare_memcpy_task(tsk, dsa, src, 16*1024, dst);
    r_arg->comp = tsk->comp;

    acctest_desc_submit(dsa, tsk->desc);
    fcontext_swap(parent, NULL);

    /* validate the bufs */
    for(int i=0; i<16*1024; i++){
      if(src[i] != 0x1 || dst[i] != 0x1){
        PRINT_ERR("Payload mismatch: 0x%x 0x%x\n", src[i], dst[i]);
        // return -EINVAL;
      }
    }
    fcontext_swap(parent, NULL);

}

void block_offload_request(fcontext_transfer_t arg) {
    fcontext_t parent = arg.prev_context;
    /* Offload, yield, post process*/
    uint8_t *src = (uint8_t *)malloc(16*1024);
    uint8_t *dst = (uint8_t *)malloc(16*1024);
    /* populate the bufs */
    for(int i=0; i<16*1024; i++){
      src[i] = 0x1;
      dst[i] = 0x2;
    }
    struct task *tsk = acctest_alloc_task(dsa);
    prepare_memcpy_task(tsk, dsa, src, 16*1024, dst);

    arg.data = tsk;
    ret_val = 0;

    acctest_desc_submit(dsa, tsk->desc);
    while(tsk->comp->status == 0){
      _mm_pause();
    }
    fcontext_swap(parent, NULL);

    /* validate the bufs */
    for(int i=0; i<16*1024; i++){
      if(src[i] != 0x1 || dst[i] != 0x1){
        PRINT_ERR("Payload mismatch: 0x%x 0x%x\n", src[i], dst[i]);
        // return -EINVAL;
      }
    }
}

void time_the_yield(fcontext_transfer_t arg) {
    fcontext_t parent = arg.prev_context;
    /* Offload, yield, post process*/
    uint8_t *src = (uint8_t *)malloc(16*1024);
    uint8_t *dst = (uint8_t *)malloc(16*1024);
    struct task *tsk = acctest_alloc_task(dsa);
    prepare_memcpy_task(tsk, dsa, src, 16*1024, dst);

    request_args *r_arg = (request_args *)(arg.data);
    int idx = r_arg->idx;
    uint64_t *start_times = r_arg->start_times;
    uint64_t *end_times = r_arg->end_times;
    ret_val = 0;

    start_times[idx] = sampleCoderdtsc();
    fcontext_swap(parent, NULL);

}

/* mem_acc_pattern.h */
uint64_t uniform_distribution(uint64_t rangeLow, uint64_t rangeHigh) {
  double randval = rand()/(1.0 + RAND_MAX);
  uint64_t randVal = rangeLow + (rangeHigh - rangeLow) * randval;
  if(randVal < rangeLow || randVal >= rangeHigh){
    PRINT_ERR("randVal: %ld rangeLow:%ld rangeHigh:%ld \n", randVal, rangeLow, rangeHigh);
    exit(-1);
  }
  return randVal;
}
volatile void *chase_pointers_global;
void **create_linear_chain(int size){
  uint64_t len = size / sizeof(void *);

  void ** memory = (void *)malloc(sizeof(void *) *len);
  uint64_t  *indices = malloc(sizeof(uint64_t) * len);
  for (int i = 0; i < len; i++) {
    indices[i] = i;
  }
  for (int i = 0; i < len-1; ++i) {
    uint64_t j = (i + 1) % len;
    if( i == j) continue;
    uint64_t tmp = indices[i];
    indices[i] = indices[j];
    indices[j] = tmp;
  }

  for (int i = 1; i < len; ++i) {
    memory[indices[i-1]] = (void *) &memory[indices[i]];
  }
  memory[indices[len - 1]] = (void *) &memory[indices[0]];
  return memory;

}

void random_permutation(uint64_t *array, int size){
  srand(time(NULL));

  for (int i = size - 1; i > 0; i--) {
      // Get a random index from 0 to i
      int j = rand() % (i + 1);

      // Swap array[i] with the element at random index
      uint64_t temp = array[i];
      array[i] = array[j];
      array[j] = temp;
  }
}
void **create_random_chain(int size){
  uint64_t len = size / 64;

  void ** memory = (void *)malloc(size);
  uint64_t  *indices = malloc(sizeof(uint64_t) * len);
  for (int i = 0; i < len; i++) {
    indices[i] = i;
  }
  random_permutation(indices, len);

  /* the memaddr is 8 bytes -- only read each cache line once */
  for (int i = 1; i < len; ++i) {
    memory[indices[i-1] * 8] = (void *) &memory[indices[i] * 8];
  }
  memory[indices[len - 1] * 8] = (void *) &memory[indices[0] * 8 ];
  return memory;


}
void **create_random_chain_starting_at(int size, void **st_addr){ /* only touches each cacheline*/
  uint64_t len = size / 64;
  void ** memory = (void *)malloc(size);
  uint64_t  *indices = malloc(sizeof(uint64_t) * len);
  for (int i = 0; i < len; i++) {
    indices[i] = i;
  }
  random_permutation(indices, len); /* have a random permutation of cache lines to pick*/

  for (int i = 1; i < len; ++i) {
    memory[indices[i-1] * 8] = (void *) &st_addr[indices[i] * 8];
  }
  memory[indices[len - 1] * 8] = (void *) &st_addr[indices[0] * 8 ];
  return memory; /* Why x8 ?*/

}
bool chase_pointers(void **memory, int count){
  void ** p = (void **)memory;
  while (count -- > 0) {
    p = (void **) *p;
  }
  chase_pointers_global = *p;
  if(count > 0){
    PRINT_ERR("count: %d\n", count);
    return false;
  }
}

void debug_chain(void **memory){
  void ** p = memory;
  size_t count = 0;
  PRINT_DBG("chain at %p:\n", memory);
  do {
    PRINT_DBG("  %p -> %p\n", p, *p);
    p = (void **) *p;
    count++;
  } while (p != memory);
  PRINT_DBG("# of pointers in chain: %lu\n", count);


}

uint64_t *create_gather_array(int size){
  size = size / 64;
  uint64_t * memory = (uint64_t *)malloc(sizeof(uint64_t) * size);
  for (uint64_t i = 0; i < size; i++) {
    memory[i] = i;
  } /* 0, ... size -1 */
  /* swap elements of gather array */
  random_permutation(memory, size);
  for(int i=0; i<size; i++){
    memory[i] = memory[i] * 64;
  }
  /* randomized */
  return memory;

}

typedef struct _filler_thread_args{
  struct completion_record *signal;
  uint64_t filler_cycles;
} filler_thread_args;


void filler_request(fcontext_transfer_t arg) {
  uint64_t start = sampleCoderdtsc();
    fcontext_t parent = arg.prev_context;
    uint64_t ops = 0;
    filler_thread_args *f_arg = (filler_thread_args *)(arg.data);
    struct completion_record *signal = f_arg->signal;
    uint64_t filler_cycles = f_arg->filler_cycles;

    /* check for preemption signal */
    while(signal->status == 0){
      _mm_pause();
    }
    uint64_t end = sampleCoderdtsc();
    f_arg->filler_cycles += end-start;
    fcontext_swap(parent, NULL);
}

int blocking_request_rps(){
  uint64_t start, end;

  start = sampleCoderdtsc();
  for(int i=0; i<num_requests; i++){
    fcontext_state_t *self = fcontext_create_proxy();
    fcontext_state_t *child = fcontext_create(block_offload_request);
    fcontext_swap(child->context, NULL);
    fcontext_destroy(child);
    fcontext_destroy_proxy(self);
  }
  end = sampleCoderdtsc();
  PRINT("Block-Offload-Cycles: %ld\n", (end-start));
}

int filler_thread_cycle_estimate(){
  uint64_t start, end;

  filler_thread_args f_args;
  f_args.signal = 0;
  f_args.filler_cycles = 0;

  request_args r_args;
  r_args.comp = NULL;

  /* overlap ax offloads */
  start = sampleCoderdtsc();
  for(int i=0; i<num_requests; i++){
    fcontext_state_t *self = fcontext_create_proxy();
    fcontext_state_t *child = fcontext_create(yield_offload_request);
    fcontext_state_t *filler = fcontext_create(filler_request);

    fcontext_transfer_t off_req_xfer = fcontext_swap(child->context, &r_args);
    fcontext_t off_req_ctx = off_req_xfer.prev_context;
    f_args.signal = (struct completion_record *) r_args.comp;
    fcontext_swap(filler->context, &f_args);
    fcontext_swap(off_req_ctx, &r_args);

    fcontext_destroy(filler);
    fcontext_destroy(child);
    fcontext_destroy_proxy(self);
  }
  end = sampleCoderdtsc();
  PRINT("Yield-RPS-Cycles-Total: %ld\n",  (end-start));
  PRINT("Filler-Cycles: %ld\n", f_args.filler_cycles);
}

int yield_time(){


  /* time_yield() */
  request_args args;
  args.start_times = malloc(sizeof(uint64_t) * num_requests);
  args.end_times = malloc(sizeof(uint64_t) * num_requests);


  for(int i=0; i<num_requests; i++){
    fcontext_state_t *self = fcontext_create_proxy();
    fcontext_state_t *child = fcontext_create(time_the_yield);
    args.idx = i;
    fcontext_swap(child->context, &args);
    args.end_times[i] = sampleCoderdtsc();
    fcontext_destroy(child);
    fcontext_destroy_proxy(self);
  }
  uint64_t avg = 0;
  uint64_t run_times[num_requests];
  avg_samples_from_arrays(run_times,avg, args.end_times, args.start_times, num_requests);
  PRINT("Yield Time: %ld\n", avg);
}
enum acc_pattern {
  LINEAR,
  RANDOM,
  GATHER
};
char *pattern_str(enum acc_pattern pat){
  switch(pat){
    case LINEAR:
      return "LINEAR";
    case RANDOM:
      return "RANDOM";
    case GATHER:
      return "GATHER";
  }
}
/* TimeTheUspacePreempt.h*/

typedef struct _time_preempt_args_t {
  uint64_t *ts0;
  uint64_t *ts1;
  uint64_t *ts2;
  uint64_t *ts3;
  uint64_t *ts4;
  uint64_t *ts5;
  uint64_t *ts6;
  uint64_t *ts7;
  uint64_t *ts8;
  uint64_t *ts9;
  uint64_t *ts10;
  uint64_t *ts11;
  uint64_t *ts12;
  uint64_t *ts13;
  uint64_t *ts14;
  uint64_t *ts15;
  uint64_t *ts16;
  struct completion_record *signal;
  struct task *tsk;
  int idx;
  int src_size;
  int scheduler_l1_prefetch;
  int flush_ax_out;
  int test_flags;
  int pollute_llc_way;
  enum acc_pattern pat;
  void **pChase;
  uint64_t pChaseSize;
  uint8_t *dst;
  int cLevel;
  bool specClevel;
  enum acc_pattern filler_access_pattern;
  bool pollute_concurrent;
} time_preempt_args_t;
void filler_request_ts(fcontext_transfer_t arg) {
    /* made it to the filler context */

    /* filler CPU work*/
    time_preempt_args_t *f_arg = (time_preempt_args_t *)(arg.data);
    int idx = f_arg->idx;
    uint64_t *ts8 = f_arg->ts8;
    uint64_t *ts9 = f_arg->ts9;
    uint64_t *ts10 = f_arg->ts10;
    ts8[idx] = sampleCoderdtsc();

    struct task *tsk = f_arg->tsk;
    void *dst = (void *)tsk->desc->dst_addr;
    int size = tsk->desc->xfer_size;

    fcontext_t parent = arg.prev_context;
    uint64_t ops = 0;
    enum acc_pattern filler_access_pattern = f_arg->filler_access_pattern;
    struct completion_record *signal = f_arg->signal;
    void **pChase = f_arg->pChase;
    uint64_t pChaseSize = f_arg->pChaseSize;

    bool pollute_concurrent = f_arg->pollute_concurrent;

    if(pollute_concurrent){
      switch(filler_access_pattern){
        case LINEAR:
          for(int i=0; i<pChaseSize; i+=64){
            pChase[i] = 0;
          }
          chase_pointers_global = ((uint8_t *)pChase)[(int)(pChaseSize-1)];
          break;
        case RANDOM:
          chase_pointers(pChase, pChaseSize / 64);
          break;
        default:
          break;
      }
    }

    ts9[idx] = sampleCoderdtsc();


    while(signal->status == 0){
      _mm_pause();
    }

    if(!pollute_concurrent){
      switch(filler_access_pattern){
        case LINEAR:
          for(int i=0; i<pChaseSize; i+=64){
            pChase[i] = 0;
          }
          chase_pointers_global = ((uint8_t *)pChase)[(int)(pChaseSize-1)];
          break;
        case RANDOM:
          chase_pointers(pChase, pChaseSize / 64);
          break;
        default:
          break;
      }
    }

    /* Received the signal */
    ts10[idx] = sampleCoderdtsc();
    fcontext_swap(parent, NULL);
}

void yield_offload_request_ts (fcontext_transfer_t arg) {
    time_preempt_args_t *r_arg = (time_preempt_args_t *)(arg.data);
    int idx = r_arg->idx;
    uint64_t *ts2 = r_arg->ts2;
    uint64_t *ts3 = r_arg->ts3;
    uint64_t *ts4 = r_arg->ts4;
    uint64_t *ts5 = r_arg->ts5;
    uint64_t *ts6 = r_arg->ts6;
    uint64_t *ts12 = r_arg->ts12;
    uint64_t *ts13 = r_arg->ts13;
    uint64_t *ts14 = r_arg->ts14;
    int memSize = r_arg->src_size ;
    int numAccesses = memSize / 64;
    int flags = r_arg->test_flags;
    int chase_on_dst = r_arg->pollute_llc_way;
    int cLevel = r_arg->cLevel;
    bool specClevel = r_arg->specClevel;
    bool doFlush = r_arg->flush_ax_out;
    enum acc_pattern pat = r_arg->pat;


  /* made it to the offload context */


    fcontext_t parent = arg.prev_context;
    ts3[idx] = sampleCoderdtsc(); /* second time to reduce ctx overhead*/
    void **src;
    void **dst = (void **)malloc(memSize);
    void **ifArray = malloc(memSize);
    /* prefault the pages */
    for(int i=0; i<memSize; i++){ /* we write to dst, but ax will overwrite, src is prefaulted from chain func*/
      ((uint8_t*)(dst))[i] = 0;
    }
    src = create_random_chain_starting_at(memSize, ifArray);

    struct task *tsk = acctest_alloc_task(dsa);

    /*finished all app work */
    ts4[idx] = sampleCoderdtsc();

    if(chase_on_dst){
      prepare_memcpy_task_flags(tsk, dsa, (uint8_t *)src, memSize, (uint8_t *)ifArray, IDXD_OP_FLAG_BOF | flags);
    } else {
      prepare_memcpy_task_flags(tsk, dsa, (uint8_t *)src, memSize, (uint8_t *)dst, IDXD_OP_FLAG_BOF | flags);
      if(!chase_on_dst){
        memcpy((uint8_t *)ifArray, (uint8_t *)src, memSize);
      }
    }
    r_arg->dst = (uint8_t *)ifArray;

    r_arg->signal = tsk->comp;
    r_arg->tsk = tsk;



    /* about to submit */
    ts5[idx] = sampleCoderdtsc();

    acctest_desc_submit(dsa, tsk->desc);
    ts6[idx] = sampleCoderdtsc();
    fcontext_swap(parent, NULL);
    /* made it back to the offload context to perform some post processing */

    /* Preform buffer movement as dictated */
    if(specClevel != false){
      if(doFlush == true ){
        for(int i=0; i<memSize; i+=64){
          _mm_clflush((const void *)ifArray + i);
        }
        cLevel = cLevel * -1;
      }

      switch(cLevel){
        case 0:
          for(int i=0; i<memSize; i+=64){
            _mm_prefetch((const void *)ifArray + i, _MM_HINT_T0);
          }
          break;
        case 1:
          for(int i=0; i<memSize; i+=64){
            _mm_prefetch((const void *)ifArray + i, _MM_HINT_T1);
          }
          break;
        case 2:
          for(int i=0; i<memSize; i+=64){
            _mm_prefetch((const void *)ifArray + i, _MM_HINT_T2);
          }
          break;
        default:
          break;
      }
    }

    ts12[idx] = sampleCoderdtsc();

    /* perform post-processing */
    switch(pat){
      case LINEAR:
        uint8_t *ifArrPtr = (uint8_t *)ifArray;
        for(int i=0; i<memSize; i+=64){
          ifArrPtr[i] = 0;
        }
        break;
      case RANDOM:
        chase_pointers(ifArray, numAccesses);
        break;
      default:
        break;

    }
    ts13[idx] = sampleCoderdtsc();
    free(ifArray);
    free(dst);
    free(src);
    /* returning control to the scheduler */
    ts14[idx] = sampleCoderdtsc();
    fcontext_swap(parent, NULL);

}

void block_offload_request_ts (fcontext_transfer_t arg) {
    time_preempt_args_t *r_arg = (time_preempt_args_t *)(arg.data);
    int idx = r_arg->idx;
    uint64_t *ts2 = r_arg->ts2;
    uint64_t *ts3 = r_arg->ts3;
    uint64_t *ts4 = r_arg->ts4;
    uint64_t *ts5 = r_arg->ts5;
    uint64_t *ts6 = r_arg->ts6;
    uint64_t *ts12 = r_arg->ts12;
    uint64_t *ts13 = r_arg->ts13;
    uint64_t *ts14 = r_arg->ts14;
    int memSize = r_arg->src_size ;
    int numAccesses = memSize / 64;
    int flags = r_arg->test_flags;
    int chase_on_dst = r_arg->pollute_llc_way;
    int cLevel = r_arg->cLevel;
    bool specClevel = r_arg->specClevel;
    bool doFlush = r_arg->flush_ax_out;
    enum acc_pattern pat = r_arg->pat;


  /* made it to the offload context */


    fcontext_t parent = arg.prev_context;
    ts3[idx] = sampleCoderdtsc(); /* second time to reduce ctx overhead*/
    void **src;
    void **dst = (void **)malloc(memSize);
    void **ifArray = malloc(memSize);
    /* prefault the pages */
    for(int i=0; i<memSize; i++){ /* we write to dst, but ax will overwrite, src is prefaulted from chain func*/
      ((uint8_t*)(dst))[i] = 0;
    }
    src = create_random_chain_starting_at(memSize, ifArray);

    struct task *tsk = acctest_alloc_task(dsa);

    /*finished all app work */
    ts4[idx] = sampleCoderdtsc();

    if(chase_on_dst){
      prepare_memcpy_task_flags(tsk, dsa, (uint8_t *)src, memSize, (uint8_t *)ifArray, IDXD_OP_FLAG_BOF | flags);
    } else {
      prepare_memcpy_task_flags(tsk, dsa, (uint8_t *)src, memSize, (uint8_t *)dst, IDXD_OP_FLAG_BOF | flags);
      if(!chase_on_dst){
        memcpy((uint8_t *)ifArray, (uint8_t *)src, memSize);
      }
    }
    r_arg->dst = (uint8_t *)ifArray;

    r_arg->signal = tsk->comp;
    r_arg->tsk = tsk;



    /* about to submit */
    ts5[idx] = sampleCoderdtsc();

    acctest_desc_submit(dsa, tsk->desc);
    while(tsk->comp->status == 0){
      _mm_pause();
    }
    ts6[idx] = sampleCoderdtsc();
    /* made it back to the offload context to perform some post processing */

    /* Preform buffer movement as dictated */
    if(specClevel != false){
      if(doFlush == true ){
        for(int i=0; i<memSize; i+=64){
          _mm_clflush((const void *)ifArray + i);
        }
        cLevel = cLevel * -1;
      }

      switch(cLevel){
        case 0:
          for(int i=0; i<memSize; i+=64){
            _mm_prefetch((const void *)ifArray + i, _MM_HINT_T0);
          }
          break;
        case 1:
          for(int i=0; i<memSize; i+=64){
            _mm_prefetch((const void *)ifArray + i, _MM_HINT_T1);
          }
          break;
        case 2:
          for(int i=0; i<memSize; i+=64){
            _mm_prefetch((const void *)ifArray + i, _MM_HINT_T2);
          }
          break;
        default:
          break;
      }
    }

    ts12[idx] = sampleCoderdtsc();

    /* perform post-processing */
    switch(pat){
      case LINEAR:
        uint8_t *ifArrPtr = (uint8_t *)ifArray;
        for(int i=0; i<memSize; i+=64){
          ifArrPtr[i] = 0;
        }
        break;
      case RANDOM:
        chase_pointers(ifArray, numAccesses);
        break;
      default:
        break;

    }
    ts13[idx] = sampleCoderdtsc();
    free(ifArray);
    free(dst);
    free(src);

    /* returning control to the scheduler */
    ts14[idx] = sampleCoderdtsc();
    fcontext_swap(parent, NULL);

}

int filler_thread_cycle_estimate_ts(){
  int num_requests = 100000;
  time_preempt_args_t t_args;
  t_args.ts0 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts1 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts2 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts3 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts4 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts5 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts6 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts7 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts8 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts9 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts10 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts11 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts12 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts13 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts14 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts15 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts16 = malloc(sizeof(uint64_t) * num_requests);

  uint64_t *ts0 = t_args.ts0;
  uint64_t *ts1 = t_args.ts1;
  uint64_t *ts2 = t_args.ts2;
  uint64_t *ts3 = t_args.ts3;
  uint64_t *ts4 = t_args.ts4;
  uint64_t *ts5 = t_args.ts5;
  uint64_t *ts6 = t_args.ts6;
  uint64_t *ts7 = t_args.ts7;
  uint64_t *ts8 = t_args.ts8;
  uint64_t *ts9 = t_args.ts9;
  uint64_t *ts10 = t_args.ts10;
  uint64_t *ts11 = t_args.ts11;
  uint64_t *ts12 = t_args.ts12;
  uint64_t *ts13 = t_args.ts13;
  uint64_t *ts14 = t_args.ts14;
  uint64_t *ts15 = t_args.ts15;
  uint64_t *ts16 = t_args.ts16;


  fcontext_transfer_t off_req_xfer;
  fcontext_t off_req_ctx;
  t_args.src_size = 16*1024;
  t_args.flush_ax_out = 0;
  t_args.test_flags = IDXD_OP_FLAG_CC;
  t_args.pat = LINEAR;
  t_args.pollute_llc_way = 1;
  int i=0;
  for(i=0; i<num_requests; i++){
    t_args.idx = i;
    fcontext_state_t *self = fcontext_create_proxy();
    ts0[i] = sampleCoderdtsc();
    fcontext_state_t *child = fcontext_create(yield_offload_request_ts);
    ts1[i] = sampleCoderdtsc(); /* how long to create context for a request?*/
    fcontext_state_t *filler = fcontext_create(filler_request_ts);

    ts2[i] = sampleCoderdtsc(); /* about to ctx switch ?*/
    off_req_xfer = fcontext_swap(child->context, &t_args);
    ts7[i] = sampleCoderdtsc(); /* req yielded*/

    fcontext_swap(filler->context, &t_args);
    ts11[i] = sampleCoderdtsc(); /* filler done*/
    off_req_ctx = off_req_xfer.prev_context;
    fcontext_swap(off_req_ctx, &t_args);
    ts15[i] = sampleCoderdtsc(); /* req done*/

    fcontext_destroy(filler);
    ts16[i] = sampleCoderdtsc(); /* filler destroyed*/
    fcontext_destroy(child);
    fcontext_destroy_proxy(self);
  }

  uint64_t avg = 0;
  uint64_t run_times[num_requests];
  avg_samples_from_arrays(run_times,avg, ts1, ts0, num_requests);
  PRINT("Create_a_request_processing_context: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts3, ts2, num_requests);
  PRINT("ContextSwitchIntoRequest: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts4, ts3, num_requests);
  PRINT("RequestCPUWork: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts5, ts4, num_requests);
  PRINT("Prepare_Offload: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts6, ts5, num_requests);
  PRINT("Submit_Offload: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts7, ts6, num_requests);
  PRINT("ContextSwitchIntoScheduler: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts8, ts7, num_requests);
  PRINT("ContextSwitchIntoFiller: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts9, ts8, num_requests);
  PRINT("FillerCPUWork: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts10, ts9, num_requests);
  PRINT("RemainingAxWaitingCycles: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts11, ts10, num_requests);
  PRINT("ContextSwitchIntoScheduler: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts12, ts11, num_requests);
  PRINT("ContextSwitchToResumeRequest: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts14, ts13, num_requests);
  PRINT("RequestOnCPUPostProcessing: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts15, ts14, num_requests);
  PRINT("ContextSwitchIntoScheduler: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts16, ts15, num_requests);
  PRINT("Destroy_a_request_processing_context: %ld\n", avg);

  free(t_args.ts0);
  free(t_args.ts1);
  free(t_args.ts2);
  free(t_args.ts3);
  free(t_args.ts4);
  free(t_args.ts5);
  free(t_args.ts6);
  free(t_args.ts7);
  free(t_args.ts8);
  free(t_args.ts9);
  free(t_args.ts10);
  free(t_args.ts11);
  free(t_args.ts12);
  free(t_args.ts13);
  free(t_args.ts14);
  free(t_args.ts15);
  free(t_args.ts16);

}

int ax_output_pat_interference(
  enum acc_pattern pat,
  int xfer_size,
  int scheduler_prefetch,
  int do_flush,
  int chase_on_dst,
  int tflags,
  uint64_t filler_access_size,
  int cLevel,
  bool specClevel,
  bool pollute_concurrent,
  bool blocking)
{
  int num_requests = 1000;
  time_preempt_args_t t_args;
  t_args.ts0 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts1 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts2 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts3 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts4 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts5 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts6 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts7 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts8 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts9 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts10 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts11 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts12 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts13 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts14 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts15 = malloc(sizeof(uint64_t) * num_requests);
  t_args.ts16 = malloc(sizeof(uint64_t) * num_requests);

  uint64_t *ts0 = t_args.ts0;
  uint64_t *ts1 = t_args.ts1;
  uint64_t *ts2 = t_args.ts2;
  uint64_t *ts3 = t_args.ts3;
  uint64_t *ts4 = t_args.ts4;
  uint64_t *ts5 = t_args.ts5;
  uint64_t *ts6 = t_args.ts6;
  uint64_t *ts7 = t_args.ts7;
  uint64_t *ts8 = t_args.ts8;
  uint64_t *ts9 = t_args.ts9;
  uint64_t *ts10 = t_args.ts10;
  uint64_t *ts11 = t_args.ts11;
  uint64_t *ts12 = t_args.ts12;
  uint64_t *ts13 = t_args.ts13;
  uint64_t *ts14 = t_args.ts14;
  uint64_t *ts15 = t_args.ts15;
  uint64_t *ts16 = t_args.ts16;

  uint64_t *bMnp = malloc(sizeof(uint64_t) * num_requests);
  uint64_t *bMnp2 = malloc(sizeof(uint64_t) * num_requests);


  fcontext_transfer_t off_req_xfer;
  fcontext_t off_req_ctx;

  uint64_t chainSize = filler_access_size;

  switch(pat){
    case LINEAR:
      t_args.pat = LINEAR;
      break;
    case RANDOM:
      t_args.pat = RANDOM;
      break;
    case GATHER:
      t_args.pat = GATHER;
      break;
  }
  t_args.src_size = xfer_size;
  t_args.scheduler_l1_prefetch = scheduler_prefetch;
  t_args.flush_ax_out = do_flush;
  t_args.test_flags = tflags;
  t_args.pollute_llc_way = chase_on_dst;
  t_args.pChase = create_random_chain(chainSize);
  t_args.pChaseSize = chainSize;
  t_args.cLevel = cLevel;
  t_args.specClevel =specClevel;
  t_args.pollute_concurrent = pollute_concurrent;
  t_args.filler_access_pattern = RANDOM;
  for(int i=0; i<num_requests; i++){
    t_args.idx = i;
    fcontext_state_t *self = fcontext_create_proxy();
    ts0[i] = sampleCoderdtsc();
    fcontext_state_t *child;

    if(!blocking){

    child = fcontext_create(yield_offload_request_ts);
    ts1[i] = sampleCoderdtsc(); /* how long to create context for a request?*/
    fcontext_state_t *filler = fcontext_create(filler_request_ts);

    ts2[i] = sampleCoderdtsc(); /* about to ctx switch ?*/
    off_req_xfer = fcontext_swap(child->context, &t_args);
    ts7[i] = sampleCoderdtsc(); /* req yielded*/

    fcontext_swap(filler->context, &t_args);
    ts11[i] = sampleCoderdtsc(); /* filler done*/
    off_req_ctx = off_req_xfer.prev_context;

    /*Optionally prefetch synchronously */
    bMnp[i] = sampleCoderdtsc();

    if(scheduler_prefetch){
      // int num_sw_stride_fetches = xfer_size / 16;
      // int num_sw_stride_fetche_bytes = num_sw_stride_fetches * 64;
      for(int i=0; i<xfer_size; i+=64){
        __builtin_prefetch((const void*) t_args.dst + i);
        // _mm_clflush((const void*) t_args.dst + i);
        // __builtin_prefetch(t_args.dst);
      }
    }
    bMnp2[i] = sampleCoderdtsc();

    fcontext_swap(off_req_ctx, &t_args);
    ts15[i] = sampleCoderdtsc(); /* req done*/

    fcontext_destroy(filler);
    ts16[i] = sampleCoderdtsc(); /* filler destroyed*/
    fcontext_destroy(child);
    } else {
      child = fcontext_create(block_offload_request_ts);
      fcontext_swap(child->context, &t_args);
      fcontext_destroy(child);
    }


    fcontext_destroy_proxy(self);
  }

  uint64_t avg = 0;
  uint64_t run_times[num_requests];
  /* print bytes */
  avg_samples_from_arrays(run_times,avg, ts13, ts12, num_requests);
  PRINT("RequestOnCPUPostProcessing: %ld ", avg);
  avg_samples_from_arrays(run_times,avg, bMnp2, bMnp, num_requests);
  PRINT("AddedPrefetchingTime: %ld ", avg);

  PRINT("FillerBytesAccessed: %d ", chainSize);
  PRINT("RequestorBytesAccessed: %d ", xfer_size);
  // avg_samples_from_arrays(run_times,avg, ts1, ts0, num_requests);
  // PRINT("Create_a_request_processing_context: %ld\n", avg);
  // avg_samples_from_arrays(run_times,avg, ts3, ts2, num_requests);
  // PRINT("ContextSwitchIntoRequest: %ld\n", avg);
  // avg_samples_from_arrays(run_times,avg, ts4, ts3, num_requests);
  // PRINT("RequestCPUWork: %ld\n", avg);
  // avg_samples_from_arrays(run_times,avg, ts5, ts4, num_requests);
  // PRINT("Prepare_Offload: %ld\n", avg);
  // avg_samples_from_arrays(run_times,avg, ts6, ts5, num_requests);
  // PRINT("Submit_Offload: %ld\n", avg);
  // avg_samples_from_arrays(run_times,avg, ts7, ts6, num_requests);
  // PRINT("ContextSwitchIntoScheduler: %ld ", avg);
  // avg_samples_from_arrays(run_times,avg, ts8, ts7, num_requests);
  // PRINT("ContextSwitchIntoFiller: %ld\n", avg);
  avg_samples_from_arrays(run_times,avg, ts9, ts8, num_requests);
  // PRINT("FillerCPUWork: %ld\n", avg);
  // avg_samples_from_arrays(run_times,avg, ts10, ts9, num_requests);
  // PRINT("RemainingAxWaitingCycles: %ld\n", avg);
  // avg_samples_from_arrays(run_times,avg, ts11, ts10, num_requests);
  // PRINT("ContextSwitchIntoScheduler: %ld\n", avg);
  // avg_samples_from_arrays(run_times,avg, ts12, ts11, num_requests);
  // PRINT("ContextSwitchToResumeRequest: %ld\n", avg);

  // avg_samples_from_arrays(run_times,avg, ts15, ts14, num_requests);
  // PRINT("ContextSwitchIntoScheduler: %ld\n", avg);
  // avg_samples_from_arrays(run_times,avg, ts16, ts15, num_requests);
  // PRINT("Destroy_a_request_processing_context: %ld\n", avg);
  PRINT("\n");
  free(t_args.ts0);
  free(t_args.ts1);
  free(t_args.ts2);
  free(t_args.ts3);
  free(t_args.ts4);
  free(t_args.ts5);
  free(t_args.ts6);
  free(t_args.ts7);
  free(t_args.ts8);
  free(t_args.ts9);
  free(t_args.ts10);
  free(t_args.ts11);
  free(t_args.ts12);
  free(t_args.ts13);
  free(t_args.ts14);
  free(t_args.ts15);
  free(t_args.ts16);
  free(t_args.pChase);

}

int access_location_pattern(){
  int tflags = TEST_FLAGS_BOF;
	int wq_id = 0;
	int dev_id = 2;
  int opcode = DSA_OPCODE_MEMMOVE;
  int wq_type = ACCFG_WQ_SHARED;
  int rc;

  int num_offload_requests = 1;
  dsa = acctest_init(tflags);

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
  if (rc < 0)
    return -ENOMEM;

  acctest_alloc_multiple_tasks(dsa, num_offload_requests);
  int xfer_size = 16 * 1024;
  for(int do_flush=0; do_flush<=0; do_flush++){
    for (enum acc_pattern pat = LINEAR; pat <= GATHER; pat++){
      for(int cctrl = 0; cctrl <= 1; cctrl++){
        if(cctrl == 1){
          tflags = IDXD_OP_FLAG_CC | IDXD_OP_FLAG_BOF;
        } else{
          tflags = IDXD_OP_FLAG_BOF;
        }
        int filler_pollute_max;
        if(cctrl == 1){
          filler_pollute_max = 1;
        } else{
          filler_pollute_max = 0;
        }
        for(int chase_on_dst = 0; chase_on_dst <= filler_pollute_max; chase_on_dst++){
          int do_prefetch_max;
          if(chase_on_dst == 0){
            do_prefetch_max = 1;
          } else{
            do_prefetch_max = 0;
          }
          for(int scheduler_prefetch = 0; scheduler_prefetch <= do_prefetch_max; scheduler_prefetch++){
              PRINT("\n");
              PRINT("XferSize: %d ", xfer_size);
              PRINT("Pattern: %s ", pattern_str(pat));
              PRINT("Prefetch: %d ", scheduler_prefetch);
              PRINT("Flush: %d ", do_flush);
              PRINT("FillerPollute: %d ", chase_on_dst);
              PRINT("CacheControl: %d ", cctrl);

              // ax_output_pat_interference(pat, xfer_size, scheduler_prefetch, 0, chase_on_dst, tflags);
          }
        }
      }
    }
  }
  PRINT("\n");


  acctest_free_task(dsa);
  acctest_free(dsa);
}

static inline do_access_pattern(enum acc_pattern pat, void *dst, int size){
  switch(pat){
    case LINEAR:
      for(int i=0; i<size; i+=64){
        ((uint8_t*)dst)[i] = 0;
      }
      break;
    case RANDOM:
      chase_pointers(dst, size / 64);
      break;
    case GATHER:
      break;
    default:
      break;
  }
}

int main(){
  CpaStatus status = CPA_STATUS_SUCCESS, stat;
  stat = qaeMemInit();
  stat = icp_sal_userStartMultiProcess("SSL", CPA_FALSE);
  CpaInstanceHandle dcInstHandles[MAX_INSTANCES];
  CpaDcSessionHandle sessionHandles[MAX_INSTANCES];

  int tflags = TEST_FLAGS_BOF;
	int wq_id = 0;
	int dev_id = 2;
  int opcode = DSA_OPCODE_MEMMOVE;
  int wq_type = ACCFG_WQ_SHARED;
  int rc;

  int num_offload_requests = 1;
  dsa = acctest_init(tflags);

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
  if (rc < 0)
    return -ENOMEM;

  acctest_alloc_multiple_tasks(dsa, num_offload_requests);

  int num_requests = 1000;
  #define CACHE_LINE_SIZE 64
  #define L1SIZE 48 * 1024
  #define L2SIZE 2 * 1024 * 1024
  #define L3WAYSIZE 2621440ULL
  #define L3FULLSIZE 39321600ULL

  // int f_acc_size[1] = {L1SIZE, L2SIZE, L3WAYSIZE, L3FULLSIZE};
  int f_acc_size[5] = {2 * 1024, L1SIZE, L2SIZE, L3FULLSIZE};
  /* but how much damage can the filler even do if we preempt it*/


  // for(int cLevel = 0; cLevel <= 2; cLevel++){
  int cLevel = 0;

  int xfer_size = 1024 * 1024;
  int chase_on_dst = 0; /* yielder reads dst */
  tflags = IDXD_OP_FLAG_BOF | IDXD_OP_FLAG_CC;

  int reuse_distance = L1SIZE;
  int do_flush = 0;
  bool specClevel = false;
  int scheduler_prefetch = false;


  enum acc_pattern pat = RANDOM;
  scheduler_prefetch = true;
  for(enum acc_pattern pat = RANDOM; pat <=RANDOM; pat++){
    for(int i=4*1024; i<=37 * 1024 * 1024 ; i*=2){

      /* Baseline access */
      // PRINT("Baseline: %d Pattern: %s ", f_acc_size[i], pattern_str(pat));
      // uint64_t start_times[num_requests],
      //   end_times[num_requests],
      //   run_times[num_requests],
      //   avg;
      // void ** dst = malloc(f_acc_size[i]);
      // void ** src = create_random_chain_starting_at(f_acc_size[i], dst);
      // memcpy(dst, src, f_acc_size[i]);
      // for(int j=0; j<num_requests; j++){
      //   uint64_t start, end;
      //   start = sampleCoderdtsc();
      //   do_access_pattern(pat, dst, f_acc_size[i]);
      //   end = sampleCoderdtsc();
      //   start_times[j] = start;
      //   end_times[j] = end;
      // }
      // avg_samples_from_arrays(run_times, avg, end_times, start_times, num_requests);
      // PRINT("RequestOnCPUPostProcessing: %ld\n", avg);
      // free(dst);
      // free(src);

      /* prefetched */
      PRINT("AxOutput-Prefetch: %d Pattern: %s ", i, pattern_str(pat));
      ax_output_pat_interference(pat, i, true, do_flush,
      chase_on_dst, tflags, f_acc_size[0], cLevel, specClevel, true, false);
      PRINT("AxOutput-LLC: %d Pattern: %s ", i, pattern_str(pat));
      ax_output_pat_interference(pat, i, false, do_flush,
      chase_on_dst, tflags, f_acc_size[0], cLevel, specClevel, true, false);
    }
  }
    // PRINT("ReuseDistance: %d ", reuse_distance);
    // ax_output_pat_interference(pat, xfer_size, scheduler_prefetch, do_flush, chase_on_dst, tflags, reuse_distance, cLevel, specClevel);

    // PRINT("Flush_ChaseOnDst L1" );
    // ax_output_pat_interference(pat, xfer_size, scheduler_prefetch, 1, 1, tflags, CACHE_LINE_SIZE, 0, true);
    // PRINT("ChaseOnDst L1" );
    // ax_output_pat_interference(pat, xfer_size, scheduler_prefetch, 0, 1, tflags, CACHE_LINE_SIZE, 0, true);
    // PRINT("Flush_ChaseOnDst L2" );
    // ax_output_pat_interference(pat, xfer_size, scheduler_prefetch, 1, 1, tflags, CACHE_LINE_SIZE, 1, true);
    // PRINT("ChaseOnDst L2" );
    // ax_output_pat_interference(pat, xfer_size, scheduler_prefetch, 0, 1, tflags, CACHE_LINE_SIZE, 1, true);
    // PRINT("Flush_ChaseOnDst L3" );
    // ax_output_pat_interference(pat, xfer_size, scheduler_prefetch, 1, 1, tflags, CACHE_LINE_SIZE, 2, true);
    // PRINT("ChaseOnDst L3" );
    // ax_output_pat_interference(pat, xfer_size, scheduler_prefetch, 0, 1, tflags, CACHE_LINE_SIZE, 2, true);

    // PRINT("\n");


    return;

  acctest_free_task(dsa);
  acctest_free(dsa);

  /* scheduler thread waits for offload completion and switches tasks */
  /* figure of merit is filler ops completed */
  /* Want to measure the request throughput*/


exit:

  icp_sal_userStop();
  qaeMemDestroy();
  return 0;
}