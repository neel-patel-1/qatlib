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

#include <unistd.h>

#include <math.h>

#include "accel-sim.h"

int gDebugParam = 0;

uint8_t **mini_bufs;
uint8_t **dst_mini_bufs;

#define time_code_region(per_run_setup, code_to_measrure, per_run_cleanup, iterations) \
  uint64_t start, end, avg; \
  uint64_t start_times[iterations], end_times[iterations], times[iterations]; \
  for(int i=0; i<iterations; i++){ \
    per_run_setup; \
    start = sampleCoderdtsc(); \
    code_to_measrure; \
    end = sampleCoderdtsc(); \
    start_times[i] = start; \
    end_times[i] = end; \
    per_run_cleanup; \
  } \
  avg_samples_from_arrays(times, avg, end_times, start_times, iterations);



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
struct acctest_context *iaa = NULL;

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
  enum wait_style block_wait_style;
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
    uint64_t *ts7 = r_arg->ts12;
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
    enum wait_style block_style = r_arg->block_wait_style;



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

    uint8_t touch;
    for(int i=0; i<memSize; i+=64){
      touch = ((volatile uint8_t *)ifArray)[i];
    }

    /* about to submit */
    ts5[idx] = sampleCoderdtsc();

    acctest_desc_submit(dsa, tsk->desc);
    ts6[idx] = sampleCoderdtsc();

    switch(block_style){
      case UMWAIT:
        wait_umwait(tsk->comp);
        break;
      case PAUSE:
        wait_pause(tsk->comp);
        break;
      case SPIN:
        wait_spin(tsk->comp);
        break;
    }
    ts7[idx] = sampleCoderdtsc();
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
  enum acc_pattern fillerPat,
  int xfer_size,
  int scheduler_prefetch,
  int do_flush,
  int chase_on_dst,
  int tflags,
  uint64_t filler_access_size,
  int cLevel,
  bool specClevel,
  bool pollute_concurrent,
  bool blocking,
  enum wait_style wait_style,
  int iterations)
{
  int num_requests = iterations;
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
  memset(bMnp, 0, sizeof(uint64_t) * num_requests);
  memset(bMnp2, 0, sizeof(uint64_t) * num_requests);


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
    default:
      t_args.pat = RANDOM;
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
  switch(fillerPat){
    case LINEAR:
      t_args.filler_access_pattern = LINEAR;
      break;
    case RANDOM:
      t_args.filler_access_pattern = RANDOM;
      break;
    case GATHER:
      t_args.filler_access_pattern = GATHER;
      break;
    default:
      t_args.filler_access_pattern = RANDOM;
      break;
  }
  t_args.block_wait_style = wait_style;
  for(int i=0; i<num_requests; i++){
    t_args.idx = i;
    fcontext_state_t *self = fcontext_create_proxy();
    fcontext_state_t *child;

    if(!blocking){
      ts0[i] = sampleCoderdtsc();
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
      ts0[i] = sampleCoderdtsc();
      child = fcontext_create(block_offload_request_ts);
      fcontext_swap(child->context, &t_args);
      ts1[i] = sampleCoderdtsc(); /* how long to create context for a request?*/
      fcontext_destroy(child);
    }


    fcontext_destroy_proxy(self);
  }

  uint64_t avg = 0;
  uint64_t run_times[num_requests];
  /* print bytes */
  avg_samples_from_arrays(run_times,avg, bMnp2, bMnp, num_requests);
  PRINT("CyclesInSchedulingContext: %ld ", avg);
  avg_samples_from_arrays(run_times,avg, ts12, ts6, num_requests);
  PRINT("PrefetchCycles(IfBlockingEnabled): %ld ", avg);
  avg_samples_from_arrays(run_times,avg, ts13, ts12, num_requests);
  PRINT("RequestOnCPUPostProcessing: %ld ", avg);

  if(blocking){
    avg_samples_from_arrays(run_times,avg, ts3, ts0, num_requests);
    PRINT("SwitchIntoBlockingContext: %ld ", avg);
    avg_samples_from_arrays(run_times,avg, ts4, ts3, num_requests);
    PRINT("AllocatBuffers_PrefaultPages_CreateRandomChainad_AllocateTask: %ld ", avg);
    avg_samples_from_arrays(run_times,avg, ts5, ts4, num_requests);
    PRINT("PrepareDescriptor_TouchHostSrcBuffer: %ld ", avg);
    avg_samples_from_arrays(run_times,avg, ts6, ts5, num_requests);
    PRINT("AxSubmit: %ld ", avg);
    avg_samples_from_arrays(run_times,avg, ts7, ts6, num_requests);
    PRINT("AxComputeTime: %ld ", avg);
    avg_samples_from_arrays(run_times,avg, ts12, ts7, num_requests);
    PRINT("PlacingBufferInHierarchy: %ld ", avg);
    avg_samples_from_arrays(run_times,avg, ts13, ts12, num_requests);
    PRINT("PostProcessing: %ld ", avg);
    avg_samples_from_arrays(run_times,avg, ts1, ts13, num_requests);
    PRINT("SwitchBackIntoScheduler: %ld\n", avg);

  }
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

int accel_output_acc_test(int argc, char **argv){
  #define CACHE_LINE_SIZE 64
  #define L1SIZE 48 * 1024
  #define L2SIZE 2 * 1024 * 1024
  #define L3WAYSIZE 2621440ULL
  #define L3FULLSIZE 39321600ULL

  // int f_acc_size[1] = {L1SIZE, L2SIZE, L3WAYSIZE, L3FULLSIZE};
  int f_acc_size[5] = {2 * 1024, L1SIZE, L2SIZE, L3FULLSIZE};
  /* but how much damage can the filler even do if we preempt it*/

  int cLevel = 0;

  int xfer_size = 1024 * 1024;

  int reuse_distance = L1SIZE;
  int do_flush = 0;
  bool specClevel = false;

  int post_proc_sizes[2] = {L1SIZE, L2SIZE};
  int tflags = IDXD_OP_FLAG_BOF ;

  int post_ = post_proc_sizes[1];
  int filler_ = L2SIZE;
  enum acc_pattern pat = RANDOM;
  enum acc_pattern filler_pat = RANDOM;
  bool block = false;
  bool scheduler_prefetch = false;
  int opt;
  int chase_on_dst = 0; /* yielder reads dst */
  int iterations;

  while ((opt = getopt(argc, argv, "bps:f:t:h:i:ac:j")) != -1) {
		switch (opt) {
		case 'b':
			block = true;
			break;
		case 'p':
			scheduler_prefetch = true;
      break;
    case 's':
      post_ = atoi(optarg);
      break;
    case 'f':
      filler_ = atoi(optarg);
      break;
    case 'a':
      chase_on_dst = 1;
      break;
    case 't':
      int pat_num = atoi(optarg);
      pat = (enum acc_pattern)pat_num;
      break;
    case 'h':
      int fill_num = atoi(optarg);
      filler_pat = (enum acc_pattern)fill_num;
    case 'i':
      iterations = atoi(optarg);
      break;
    case 'l':
      cLevel = atoi(optarg);
      break;
    case 'c':
      specClevel = true;
      break;
    case 'j':
      tflags |= IDXD_OP_FLAG_CC;
      break;
		default:
			break;
		}
	}

  PRINT("b: %d p: %d s: %d f: %d a: %d t: %d h: %d i: %d l: %d c: %d j: %d\n",
    block, scheduler_prefetch, post_, filler_, chase_on_dst, pat, filler_pat,
    iterations, cLevel, specClevel, tflags);

  ax_output_pat_interference(pat, filler_pat, post_, scheduler_prefetch, false,
        chase_on_dst, tflags, filler_, cLevel, specClevel, true, block, SPIN, iterations);



  return 0;
}


/* hash.h*/
/* Seed constant for MurmurHash64A selected by search for optimum diffusion
 * including recursive application.
 */
#define SEED 4193360111ul

/* Maximum number tries for in-range result before just returning 0. */
#define MAX_TRIES 32

/* Gap in bit index per try; limits us to 2^FURC_SHIFT shards.  Making this
 * larger will sacrifice a modest amount of performance.
 */
#define FURC_SHIFT 23

/* Size of cache for hash values; should be > MAXTRIES * (FURCSHIFT + 1) */
#define FURC_CACHE_SIZE 1024

/* MurmurHash64A performance-optimized for hash of uint64_t keys and seed = M0
 */
static uint64_t murmur_rehash_64A(uint64_t k) {
  const uint64_t m = 0xc6a4a7935bd1e995ULL;
  const int r = 47;

  uint64_t h = (uint64_t)SEED ^ (sizeof(uint64_t) * m);

  k *= m;
  k ^= k >> r;
  k *= m;

  h ^= k;
  h *= m;

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;
}
uint64_t
murmur_hash_64A(const void* const key, const size_t len, const uint32_t seed) {
  const uint64_t m = 0xc6a4a7935bd1e995ULL;
  const int r = 47;

  uint64_t h = seed ^ (len * m);

  const uint64_t* data = (const uint64_t*)key;
  const uint64_t* end = data + (len / 8);

  while (data != end) {
    uint64_t k = *data++;

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;
  }
  const uint8_t* data2 = (const uint8_t*)data;

  switch (len & 7) {
    case 7:
      h ^= (uint64_t)data2[6] << 48;
      __attribute__((fallthrough));
    case 6:
      h ^= (uint64_t)data2[5] << 40;
      __attribute__((fallthrough));
    case 5:
      h ^= (uint64_t)data2[4] << 32;
      __attribute__((fallthrough));
    case 4:
      h ^= (uint64_t)data2[3] << 24;
      __attribute__((fallthrough));
    case 3:
      h ^= (uint64_t)data2[2] << 16;
      __attribute__((fallthrough));
    case 2:
      h ^= (uint64_t)data2[1] << 8;
      __attribute__((fallthrough));
    case 1:
      h ^= (uint64_t)data2[0];
      h *= m;
  }

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;
}

uint32_t furc_get_bit(
    const char* const key,
    const size_t len,
    const uint32_t idx,
    uint64_t* hash,
    int32_t* old_ord_p) {
  int32_t ord = (idx >> 6);
  int n;

  if (key == NULL) {
    *old_ord_p = -1;
    return 0;
  }

  if (*old_ord_p < ord) {
    for (n = *old_ord_p + 1; n <= ord; n++) {
      hash[n] =
          ((n == 0) ? murmur_hash_64A(key, len, SEED)
                    : murmur_rehash_64A(hash[n - 1]));
    }
    *old_ord_p = ord;
  }

  return (hash[ord] >> (idx & 0x3f)) & 0x1;
}

uint32_t furc_hash(const char* const key, const size_t len, const uint32_t m) {
  uint32_t try;
  uint32_t d;
  uint32_t num;
  uint32_t i;
  uint32_t a;
  uint64_t hash[FURC_CACHE_SIZE];
  int32_t old_ord;

  // There are (ab)users of this function that pass in larger
  // numbers, and depend on the behavior not changing (ie we can't
  // just clamp to the max). Just let it go for now.

  // assert(m <= furc_maximum_pool_size());

  if (m <= 1) {
    return 0;
  }

  furc_get_bit(NULL, 0, 0, hash, &old_ord);
  for (d = 0; m > (1ul << d); d++)
    ;

  a = d;
  for (try = 0; try < MAX_TRIES; try++) {
    while (!furc_get_bit(key, len, a, hash, &old_ord)) {
      if (--d == 0) {
        return 0;
      }
      a = d;
    }
    a += FURC_SHIFT;
    num = 1;
    for (i = 0; i < d - 1; i++) {
      num = (num << 1) | furc_get_bit(key, len, a, hash, &old_ord);
      a += FURC_SHIFT;
    }
    if (num < m) {
      return num;
    }
  }

  // Give up; return 0, which is a legal value in all cases.
  return 0;
}
static inline int gen_sample_memcached_request(void *buf, int buf_size){
  const char *key = "/region/cluster/foo:key|#|etc";
  memcpy(buf, key, strlen(key));
}

/*
functions/buffers prepared for host access after offload
*/
uint8_t **prepped_dsa_bufs;
uint8_t ** host_memcached_requests;


void prep_dsa_bufs(uint8_t **valuable_src_bufs, int num_bufs, int buf_size){
  prepped_dsa_bufs = (uint8_t **)malloc(sizeof(uint8_t*) * num_bufs);
  for(int i=0; i<num_bufs; i++){
    prepped_dsa_bufs[i] = (uint8_t *)malloc(buf_size);
    dsa_memcpy(valuable_src_bufs[i], buf_size, prepped_dsa_bufs[i], buf_size);
  }
}

/* router */
uint8_t ** prep_ax_desered_mc_reqs(int num_mc_reqs, int mc_req_size){
  uint8_t **memcached_requests =
    (uint8_t **)malloc(sizeof(uint8_t*) * num_mc_reqs);
  for(int i=0; i<num_mc_reqs; i++){
    memcached_requests[i] = (uint8_t *)malloc(mc_req_size);
    gen_sample_memcached_request(memcached_requests[i], mc_req_size);
  }
  prep_dsa_bufs(memcached_requests, num_mc_reqs, mc_req_size);
  return prepped_dsa_bufs;
}

typedef struct _node{
  struct node *next;
  int data;
} node;
/* linked list merge */
uint8_t ** prep_ax_generated_linked_lists(int num_lls, int num_nodes){
  /* allocate contiguous linked lists*/
  int ll_data_size = sizeof(node) * num_nodes;
  node **llists = (node **)malloc(sizeof(node*) * num_lls);

  for(int j=0; j<num_lls; j++){
    llists[j] = (node *)malloc(ll_data_size);
    node *llist = llists[j];

    for(int i=0; i<num_nodes-1; i++){
      llist[i].data = i;
      llist[i].next = &llist[i+1];
    }
    llist[num_nodes-1].data = num_nodes-1;
    llist[num_nodes-1].next = NULL;
  }
  prep_dsa_bufs(llists, num_lls, ll_data_size);
  return prepped_dsa_bufs;
}

uint8_t **  prep_host_deserd_mc_reqs(int num_mc_reqs, int mc_req_size){
  host_memcached_requests =
    (uint8_t **)malloc(sizeof(uint8_t*) * num_mc_reqs);
  for(int i=0; i<num_mc_reqs; i++){
    host_memcached_requests[i] = (uint8_t *)malloc(mc_req_size);
    gen_sample_memcached_request(host_memcached_requests[i], mc_req_size);
  }
}

void free_prepped_dsa_bufs(int num_bufs){
  for(int i=0; i<num_bufs; i++){
    free(prepped_dsa_bufs[i]);
  }
  free(prepped_dsa_bufs);
}

void free_host_memcached_requests(int num_mc_reqs){
  for(int i=0; i<num_mc_reqs; i++){
    free(host_memcached_requests[i]);
  }
  free(host_memcached_requests);
}

/* request_sim.h */
struct completion_record *comps = NULL;
int next_unresumed_task_comp_idx = 0;
int last_preempted_task_idx = 0;
bool exists_waiting_preempted_task = false;

void probe_point(int task_idx, fcontext_t parent, bool yield_to_completed_offloads){
  if(task_idx > next_unresumed_task_comp_idx && yield_to_completed_offloads){
    if(comps[next_unresumed_task_comp_idx].status == DSA_COMP_SUCCESS){
      PRINT_DBG("Task %d Preempted In Favor of Task %d\n", task_idx, next_unresumed_task_comp_idx);
      exists_waiting_preempted_task = true;
      last_preempted_task_idx = task_idx;
      fcontext_swap(parent,NULL);
    }
  }
}


void pre_offload_kernel(int kernel, void *input, int input_len, int task_idx,
  fcontext_t parent, bool yield_to_completed_offloads){
  if(kernel == 0){
    if(next_unresumed_task_comp_idx == 0){
      return;
    }
    struct completion_record *next_unresumed_task_comp = &(comps[next_unresumed_task_comp_idx-1]);
    while(next_unresumed_task_comp->status == 0){
      _mm_pause();
    }
  }
  else if(kernel == 1){
    for(int i=0; i<input_len; i++){
      if(i%200 == 0){
        probe_point(task_idx, parent, yield_to_completed_offloads);
      }
      ((uint8_t *)input)[i] = 0;
    }
  } else if (kernel == 2){
    create_random_chain_starting_at(input_len, &input);
  }
}



void post_offload_kernel(int kernel, void *pre_wrk_set,
  int pre_wrk_set_size, void *offload_data, int offload_data_size,
  int task_idx, fcontext_t parent, bool yield_to_completed_offloads){
  if(kernel == 0){
    struct completion_record *next_unresumed_task_comp = &(comps[next_unresumed_task_comp_idx]);
    while(next_unresumed_task_comp->status == 0){
      _mm_pause();
    }
  }
  else if(kernel == 1){
    int iters_between_probes = 100;
    for(int i=0; i<pre_wrk_set_size; i++){
      if(i%iters_between_probes == 0){
        probe_point(task_idx, parent, yield_to_completed_offloads);
      }
      ((uint8_t *)pre_wrk_set)[i] = 0x1;
    }
    for(int i=0; i<offload_data_size; i++){
      if(i%iters_between_probes == 0){
        probe_point(task_idx, parent, yield_to_completed_offloads);
      }
      ((uint8_t *)offload_data)[i] = 0x1;
    }
  }
  else if(kernel == 2){
    chase_pointers(pre_wrk_set, pre_wrk_set_size/sizeof(void *));
  }
  else if(kernel == 3){
    hash_memcached_request(offload_data);
  }
  else if (kernel == 4){
    int ctr = 0;
    node *curr = offload_data;
    while(curr != NULL){
      curr->data = 0;
      curr = curr->next;
      ctr++;
    }
  }
}

struct task *acctest_alloc_task_with_provided_comp(struct acctest_context *ctx,
  struct completion_record *comp)
{
	struct task *tsk;

	tsk = malloc(sizeof(struct task));
	if (!tsk)
		return NULL;
	memset(tsk, 0, sizeof(struct task));

	tsk->desc = malloc(sizeof(struct hw_desc));
	if (!tsk->desc) {
		free_task(tsk);
		return NULL;
	}
	memset(tsk->desc, 0, sizeof(struct hw_desc));

  tsk->comp = comp;

	return tsk;
}

_Atomic bool offload_pending = false;

uint8_t **pOutBuf= NULL;
uint64_t offloadDur = 0;
struct completion_record *subComp;
int submitter_task_idx = 0;

bool emul_ax_receptive = true;
int emul_ax_total_expected_offloads = 128;

void *emul_ax_func(void *arg){

  uint64_t offCompTimes[emul_ax_total_expected_offloads];
  struct completion_record *reqComps[emul_ax_total_expected_offloads];

  int lastRcvdTskIdx = -1, earlUncompTskIdx = 0;
  bool processing_offload = false, offload_completed = false;

  while(emul_ax_receptive){
    if(processing_offload){
      uint64_t currTime = sampleCoderdtsc();
      offload_completed = currTime >= offCompTimes[earlUncompTskIdx];
      if(offload_completed){
        /*notify offloader*/
        reqComps[earlUncompTskIdx]->status = DSA_COMP_SUCCESS;

        earlUncompTskIdx++;

        if(earlUncompTskIdx > lastRcvdTskIdx){
          processing_offload = false;
        }
      }
    }
    if(offload_pending){ /* always check for pending offloads */
      uint64_t offStTime = sampleCoderdtsc();
      uint64_t stallTime = offloadDur;
      *pOutBuf = prepped_dsa_bufs[submitter_task_idx]; /* just set this immediately */
      reqComps[submitter_task_idx] = subComp; /* need this at cr write time*/

      lastRcvdTskIdx = submitter_task_idx;

      processing_offload = true;
      offload_pending = false;

      offCompTimes[submitter_task_idx] = offStTime + stallTime;
    }
  }
}

void do_offload_offered_load_test(
  int offload_type,
  void *input,
  int input_size,
  void *output,
  int output_size,
  bool do_yield,
  fcontext_t parent,
  int task_id,
  uint8_t **pDstBuf, /*pointer to overwrite after comp*/
  uint64_t cycles_to_stall
  )
{
  if(offload_type == 0){
    uint64_t ts[4];
    /* do some offload */
    ts[0] = sampleCoderdtsc();
    struct task *tsk =
      acctest_alloc_task_with_provided_comp(dsa,
        &comps[task_id]);
    /* use the task id to map to the correct comp */
    ts[1] = sampleCoderdtsc();
    prepare_memcpy_task_flags(tsk,
      dsa,
      (uint8_t *)input,
      input_size,
      (uint8_t *)output,
      IDXD_OP_FLAG_BOF | IDXD_OP_FLAG_CC);
    if(enqcmd(dsa->wq_reg, tsk->desc)){
      PRINT_ERR("Failed to enqueue task\n");
      exit(-1);
    }
    ts[2] = sampleCoderdtsc();
    if(do_yield){
      PRINT_DBG("Request %d Yielding\n", task_id);
      fcontext_swap(parent, NULL);
    } else {
      while(tsk->comp->status == 0){
        _mm_pause();
      }
    }
    if(tsk->comp->status != DSA_COMP_SUCCESS){
      PRINT_ERR("Task failed: 0x%x\n", tsk->comp->status);
    }
    ts[3] = sampleCoderdtsc();
    PRINT_DBG("Offload %d: %ld %ld %ld %ld\n", task_id, ts[0], ts[1], ts[2], ts[3]);
  } else if (offload_type == 1){
    uint64_t ts[3];
    /* submit work to the emul ax*/
    ts[0] = sampleCoderdtsc();
    pOutBuf = pDstBuf;
    offloadDur = cycles_to_stall;
    subComp = &(comps[task_id]);
    submitter_task_idx = task_id;
    struct completion_record *wait_comp = &(comps[task_id]);
    offload_pending = true;
    ts[1] = sampleCoderdtsc();

    if(do_yield){
      while(offload_pending == true){ /* wait for submission to complete */
        _mm_pause();
      }
      fcontext_swap(parent, NULL);
    } else {
      while(subComp->status == 0){
        _mm_pause();
      }
    }
    ts[2] = sampleCoderdtsc();
    PRINT_DBG(" %ld %ld ", task_id, ts[1] - ts[0], ts[2] - ts[1]);
  } else if(offload_type == 2){ /*sychronous deser sim on CPU*/
    uint64_t st, end;
    uint64_t deser_dur = 1123;
    st = sampleCoderdtsc();
    gen_sample_memcached_request(output, output_size);
    for(int i=0; i<output_size; i+=64){
      ((uint8_t *)output)[i] = 0x1;
    }
    end = sampleCoderdtsc();
    while (end - st < deser_dur){
      end = sampleCoderdtsc();
    }
    PRINT_DBG("DataTouched: %d Deserialization Time: %ld\n", output_size, end-st);
  } else if(offload_type == 3){
      /* synchronous CPU linked list traversal updating a variable for each node visited*/
      /* we can create the linked list here, or we can swap the dst pointer same as the emul ax*/
      *pDstBuf = prepped_dsa_bufs[task_id];
      int ctr = 0;
      node *curr = *pDstBuf; /*output data is prepped ll?*/
      while(curr != NULL){
        curr->data = ctr;
        curr = curr->next;
        ctr++;
      }
      PRINT_DBG("Traversed %d length list\n", ctr);
      int buf_size = 2 * 1024 * 1024;
      uint8_t *dummy_buf = (uint8_t *)malloc(buf_size);
      memset(dummy_buf, 0, buf_size);

      for(int i=0; i<48 * 1024; i+=64){
        dummy_buf[i] += i;
      }
      PRINT_DBG("DummyBufTouched: %d\n", buf_size);

  }
}






void hash_memcached_request(void *request){
    /* extract hashable suffix*/
    char *suffix_start, *suffix_end;
    char route_end = '/';
    int hashable_len = 0;

    suffix_start = strrchr(request,route_end );
    suffix_end = strchr(suffix_start, '|');
    if(suffix_start != NULL && suffix_end !=NULL){
      suffix_start++;
      hashable_len = suffix_end - suffix_start;
    } else {
      PRINT_ERR("No suffix found\n");
    }
    // PRINT_DBG("Hashing %d bytes from beginning of this string: %s\n",
    //   hashable_len, suffix_start);
    furc_hash(suffix_start, hashable_len, 16);
}

float distance(float *point_query, float *query_feature, int dim){
  float sum = 0.0;
    for(int i = 0; i < dim; ++i){
        float diff = point_query[i] - query_feature[i];
        sum += diff * diff;
    }
  return sqrtf(sum);
}

void ax_access(int kernel, int offload_size){
  char *dst_buf = malloc(offload_size);
  char *src_buf = malloc(offload_size);
  float *image_point;
  uint64_t start, end;

  int iterations = 100;
  uint64_t start_times[iterations], end_times[iterations], times[iterations];
  uint64_t avg = 0;

  if(kernel == 0){
    int key_size = gen_sample_memcached_request(src_buf, offload_size); /* kernel 0*/
  } else if (kernel == 1){
    float *query_buf = (float *)(src_buf); /* kernel 1 */
    image_point = (float *)malloc(offload_size); /* one of the indexed imgs*/
    /* generate random query and image in the index*/
    for(int i=0; i<offload_size/sizeof(float); i++){
      query_buf[i] = (float)rand()/(float)(RAND_MAX); /* this gets copied to the dst*/
      image_point[i] = (float)rand()/(float)(RAND_MAX);
    }
  }



  for(int i=0; i<iterations; i++){ /*per-iteration setup*/
    int key_size = gen_sample_memcached_request(src_buf, offload_size);
    memcpy((void *)dst_buf,
      (void *)src_buf, offload_size);

    /*host produced*/
    if(kernel == 0){ /*furc*/
      start = sampleCoderdtsc(); /*timing phase*/
      hash_memcached_request(dst_buf);
      end = sampleCoderdtsc();
      end_times[i] = end;
      start_times[i] = start;
    } else if (kernel == 1){ /* get the distance between the random query and the */
      start = sampleCoderdtsc(); /*timing phase*/
      /* extract hashable suffix*/
      distance(dst_buf, image_point, offload_size/sizeof(float));
      end = sampleCoderdtsc();
      end_times[i] = end;
      start_times[i] = start;
    }
  }

  /* average across the runs */
  avg_samples_from_arrays(times, avg, end_times, start_times, iterations);
  PRINT("Host-Access-Cycles: %ld\n", avg);

  for(int i=0; i<iterations; i++){ /*per-iteration setup*/
    dsa_memcpy((void *)src_buf,
      offload_size, (void *)dst_buf, offload_size);

    /*ax produced*/
    if(kernel == 0){ /*furc*/
      start = sampleCoderdtsc(); /*timing phase*/
      /* extract hashable suffix*/
      hash_memcached_request(dst_buf);
      end = sampleCoderdtsc();
      end_times[i] = end;
      start_times[i] = start;
    } else if (kernel == 1){ /* get the distance between the random query and the */
      start = sampleCoderdtsc(); /*timing phase*/
      /* extract hashable suffix*/
      distance(dst_buf, image_point, offload_size/sizeof(float));
      end = sampleCoderdtsc();
      end_times[i] = end;
      start_times[i] = start;
    }
  }
  /* average across the runs */
  avg_samples_from_arrays(times, avg, end_times, start_times, iterations);
  PRINT("AX-Access-Cycles: %ld\n", avg);

  free(dst_buf);
  free(src_buf);
  free(image_point);

}



/* Give a req, switch into req context, it determines what it needs to do*/
/* Does each req need its own args?*/
typedef struct offload_request_args_t {
  bool do_yield;
  int task_id;

  int pre_offload_kernel_type;
  int pre_working_set_size;

  int offload_type;
  int offload_size;
  uint64_t offload_cycles;

  int post_offload_kernel_type;
} offload_request_args;
void offload_request(fcontext_transfer_t arg){
  uint64_t ts[4];
  ts[0] = sampleCoderdtsc();
  offload_request_args *r_arg = (offload_request_args *)(arg.data);
  fcontext_t parent = arg.prev_context;
  bool do_yield = r_arg->do_yield;
  int task_id = r_arg->task_id;

  int pre_offload_kernel_type = r_arg->pre_offload_kernel_type;
  int pre_working_set_size = r_arg->pre_working_set_size;

  int offload_type = r_arg->offload_type;
  int offload_size = r_arg->offload_size;

  uint64_t offload_cycles = r_arg->offload_cycles;

  int post_offload_kernel_type = r_arg->post_offload_kernel_type;

  /*pre offload*/
  void *pre_working_set = malloc(pre_working_set_size);
  pre_offload_kernel(pre_offload_kernel_type,pre_working_set, pre_working_set_size,
    task_id, parent, do_yield);

  ts[1] = sampleCoderdtsc();
  PRINT_DBG("Request %d: %ld ", task_id, ts[1] - ts[0]);
  /*offload*/
  int dst_buf_size = offload_size;
  void *dst_buf = malloc(dst_buf_size);
  do_offload_offered_load_test(
    offload_type,
    pre_working_set,
    pre_working_set_size, dst_buf,
    dst_buf_size,
    do_yield,
    arg.prev_context,
    task_id,
    (uint8_t **)&dst_buf,
    offload_cycles);

  ts[2] = sampleCoderdtsc();

  PRINT_DBG(" %ld ", ts[2] - ts[1]);
  /*post offload*/
  post_offload_kernel(post_offload_kernel_type, pre_working_set,
    pre_working_set_size, dst_buf, dst_buf_size, task_id, parent, do_yield);
  ts[3] = sampleCoderdtsc();
  PRINT_DBG(" %ld\n", task_id, ts[3] - ts[2]);

  fcontext_swap(arg.prev_context, NULL);
}

int service_time_under_exec_model_test(bool do_yield, int total_requests, int iters, int pre_offload_kernel_type,
  int pre_working_set_size, int offload_type,
  int offload_size, int post_offload_kernel_type,
  uint64_t offload_cycles)
{
  double offered_loads[iters];
  double avg_offered_load = 0;
  for(int i=0;i<iters; i++){
    int next_unused_task_comp_idx = 0;
    next_unresumed_task_comp_idx = 0;
    last_preempted_task_idx = 0;
    exists_waiting_preempted_task = false;


    fcontext_state_t *request_states[total_requests];
    fcontext_t *request_ctxs[total_requests];
    fcontext_transfer_t request_xfers[total_requests];

    comps = aligned_alloc(4096, sizeof(struct completion_record) * total_requests);
    memset(comps, 0, sizeof(struct completion_record) * total_requests);

    fcontext_state_t *self = fcontext_create_proxy();
    fcontext_t off_req_ctx;

    offload_request_args *r_args[total_requests];
    struct completion_record *next_unresumed_task_comp;

    bool need_check_for_completed_offload_tasks = do_yield; /* are tasks yielding?*/

    uint64_t total_requests_processed;


    /*setup */
    pthread_t emul_ax;
    if(offload_type == 1){
      createThreadPinned(&emul_ax, emul_ax_func, NULL, 20);
      emul_ax_receptive = true;
    }

    uint64_t start = sampleCoderdtsc();

    while(next_unused_task_comp_idx < total_requests){
      next_unresumed_task_comp = &(comps[next_unresumed_task_comp_idx]);
      if(next_unresumed_task_comp->status == DSA_COMP_SUCCESS && need_check_for_completed_offload_tasks){
        PRINT_DBG("CR Received. Request %d resuming\n", next_unresumed_task_comp_idx);
        request_xfers[next_unresumed_task_comp_idx] = /* no need for state save here for FCFS,(resumed CRs always correspond to highest priority task) but just in case */
          fcontext_swap(request_xfers[next_unresumed_task_comp_idx].prev_context, NULL);
        next_unresumed_task_comp_idx++;
      } else if(exists_waiting_preempted_task){
        PRINT_DBG("Preempted Request %d resuming\n", last_preempted_task_idx);
        exists_waiting_preempted_task = false;
        request_xfers[last_preempted_task_idx] = fcontext_swap(request_xfers[last_preempted_task_idx].prev_context, NULL);
      } else {
        request_states[next_unused_task_comp_idx] = fcontext_create(offload_request);
        r_args[next_unused_task_comp_idx] = malloc(sizeof(offload_request_args));
        r_args[next_unused_task_comp_idx]->do_yield = do_yield;
        r_args[next_unused_task_comp_idx]->task_id = next_unused_task_comp_idx;
        r_args[next_unused_task_comp_idx]->pre_offload_kernel_type = pre_offload_kernel_type;
        r_args[next_unused_task_comp_idx]->pre_working_set_size = pre_working_set_size;
        r_args[next_unused_task_comp_idx]->offload_type = offload_type;
        r_args[next_unused_task_comp_idx]->offload_size = offload_size;
        r_args[next_unused_task_comp_idx]->post_offload_kernel_type = post_offload_kernel_type;
        r_args[next_unused_task_comp_idx]->offload_cycles = offload_cycles;
        request_xfers[next_unused_task_comp_idx] =
          fcontext_swap(request_states[next_unused_task_comp_idx]->context, r_args[next_unused_task_comp_idx]);
        next_unused_task_comp_idx++;
      }
    }
    uint64_t end = sampleCoderdtsc();
    if(do_yield){
      total_requests_processed = next_unresumed_task_comp_idx;
      PRINT_DBG("RequestsProcessed: %d\n", total_requests_processed);
    }


    if(need_check_for_completed_offload_tasks){
      /* Complete all in-flight requests without starting up new ones*/
      while(next_unresumed_task_comp_idx < next_unused_task_comp_idx){
        next_unresumed_task_comp = &(comps[next_unresumed_task_comp_idx]);
        if(next_unresumed_task_comp->status == DSA_COMP_SUCCESS){
          PRINT_DBG("CR Received. Request %d resuming\n", next_unresumed_task_comp_idx);
          request_xfers[next_unresumed_task_comp_idx] = /* no need for state save here for FCFS,(resumed CRs always correspond to highest priority task) but just in case */
            fcontext_swap(request_xfers[next_unresumed_task_comp_idx].prev_context, NULL);
          next_unresumed_task_comp_idx++;
        }
        else if(exists_waiting_preempted_task){
          PRINT_DBG("Preempted Request %d resuming\n", last_preempted_task_idx);
          exists_waiting_preempted_task = false;
          request_xfers[last_preempted_task_idx] =
            fcontext_swap(request_xfers[last_preempted_task_idx].prev_context, NULL);
        }
      }
    }

    uint64_t nanos = (end - start)/(2.1);
    uint64_t micros = nanos / 1000;
    double seconds = (double)nanos / 1000000000;



    if (!do_yield) {
      total_requests_processed = total_requests;
    }


    PRINT_DBG("OfferedLoad(RPS): %f\n", (double)((double)(total_requests_processed)/(double)seconds));
    offered_loads[i] = (double)((double)(total_requests_processed)/(double)seconds);

    /*teardonw*/
    for(int i=0; i<total_requests; i++){
      fcontext_destroy(request_states[i]);
      free(r_args[i]);
    }

    free(comps);
    fcontext_destroy(self);

    if(offload_type == 1){
      emul_ax_receptive = false;
      pthread_join( emul_ax, NULL);
    }
  }
  for(int i=0; i<iters; i++){
    avg_offered_load += offered_loads[i];
  }
  avg_offered_load = avg_offered_load / iters;
  PRINT("AvgOfferedLoad(RPS): %f\n", avg_offered_load);

}

void do_offered_load_test(int argc, char **argv){
  int total_requests = 128;
  int iters = 10;
  bool do_yield = false;

  int pre_offload_kernel_type = 1;
  int pre_working_set_size = 16 * 1024;

  int offload_type = 1;
  int offload_size = 16 * 1024;

  int post_offload_kernel_type = 3;

  uint64_t emul_offload_cycles = 2100 * 100;


  int opt;
  while ((opt = getopt(argc, argv, "yi:r:f:o:k:l:s:dt:")) != -1) {
    switch (opt) {
    case 'y':
      do_yield = true;
      break;
    case 'i':
      iters = atoi(optarg);
      break;
    case 'r':
      total_requests = atoi(optarg);
      break;
    case 'f':
      pre_working_set_size = atoi(optarg);
      break;
    case 'o':
      offload_type = atoi(optarg);
      break;
    case 'k':
      pre_offload_kernel_type = atoi(optarg);
      break;
    case 'l':
      post_offload_kernel_type = atoi(optarg);
      break;
    case 's':
      offload_size = atoi(optarg);
      break;
    case 'd':
      gDebugParam = 1;
      break;
    case 't':
      sscanf( optarg, "%lu", &emul_offload_cycles);
      break;
    default:
      break;
    }
  }
  PRINT("y: %d i: %d r: %d f: %d o: %d k: %d l: %d d: %d t: %d\n",
    do_yield, iters, total_requests, pre_working_set_size,
    offload_size, pre_offload_kernel_type, post_offload_kernel_type,
    gDebugParam, emul_offload_cycles);

  /* prep the dsa bufs depending on the offload kernel type */
  if(post_offload_kernel_type == 3){
    prep_ax_desered_mc_reqs(total_requests, offload_size);
  } else if (post_offload_kernel_type == 4)
  {
    prep_ax_generated_linked_lists(total_requests, offload_size/sizeof(node)); /*node is 16 bytes*/
  }

  service_time_under_exec_model_test(do_yield, total_requests, iters,
    pre_offload_kernel_type, pre_working_set_size,
      offload_type, offload_size, post_offload_kernel_type,
      emul_offload_cycles);

}

void hash_ax_memcached_request(int num_mc_reqs){
  for(int i=0; i<num_mc_reqs; i++){
    hash_memcached_request(prepped_dsa_bufs[i]);
  }
}

void hash_host_memcached_request(int num_mc_reqs){
  for(int i=0; i<num_mc_reqs; i++){
    hash_memcached_request(host_memcached_requests[i]);
  }
}

void pre_alloc_deser_vs_reused_src_dst_ax_access_divergence(){
  int num_mc_reqs = 1; /* maybe there is some kind of prefetching ?*/
  ax_access(0, 256);

  {
    time_code_region(  prep_host_deserd_mc_reqs(num_mc_reqs, 256),
      hash_host_memcached_request(num_mc_reqs),
      free_host_memcached_requests(num_mc_reqs), 1000);
    PRINT("host_hash: %ld\n", avg);
  }

  {
    time_code_region(prep_ax_desered_mc_reqs(num_mc_reqs, 256),
      hash_ax_memcached_request(num_mc_reqs), free_prepped_dsa_bufs(num_mc_reqs), 1000);
    PRINT("ax_hash: %ld\n", avg);
  }
}



/* Use this to measure access overhead for linked list traversal */
void linked_list_overhead(int num_nodes){
  int iterations = 100;
  uint64_t start_times[iterations], end_times[iterations], times[iterations];
  uint64_t start, end, avg = 0;

  int ll_data_size = sizeof(node) * num_nodes;
  /* allocate big mem */
  node *llist = (node *)malloc(ll_data_size);
  node *dst_llist = (node *)malloc(ll_data_size);

  for(int i=0; i<num_nodes-1; i++){
    llist[i].data = i;
    llist[i].next = &llist[i+1];
  }
  llist[num_nodes-1].data = num_nodes-1;
  llist[num_nodes-1].next = NULL;

  dsa_memcpy(llist,
    ll_data_size, dst_llist, ll_data_size);

  for(int i=0; i<iterations; i++){
    dsa_memcpy(llist,
      ll_data_size, dst_llist, ll_data_size);
    start = sampleCoderdtsc();
    node *curr = dst_llist;
    while(curr != NULL){
      curr->data = 0;
      curr = curr->next;
    }
    end = sampleCoderdtsc();
    end_times[i] = end;
    start_times[i] = start;
  }
  avg_samples_from_arrays(times,
    avg, end_times, start_times, iterations);

  PRINT("%ld ", avg);

  for(int i=0; i<iterations; i++){
    memcpy(dst_llist,
      llist, ll_data_size);
    start = sampleCoderdtsc();
    node *curr = dst_llist;
    while(curr != NULL){
      curr->data = 0;
      curr = curr->next;
    }
    end = sampleCoderdtsc();
    end_times[i] = end;
    start_times[i] = start;
  }
  avg_samples_from_arrays(times,
    avg, end_times, start_times, iterations);

  PRINT(" %ld\n", avg);
}



int main(int argc, char **argv){
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

  // for(int i=10; i<=150; i+=10){
  //   linked_list_overhead(i);
  // }


  // linked_list_overhead();
  do_offered_load_test(argc, argv);

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