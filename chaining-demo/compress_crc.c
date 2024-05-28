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


#include <ucontext.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>



#define NUMCONTEXTS 10              /* how many contexts to make */
#define STACKSIZE 4096
#define NUM_DESCS 128 /* Size of the shared work queue */
#define NUM_BUFS 128
#define BUF_SIZE 4096

extern int swapcontext_very_fast(ucontext_t *ouctx, ucontext_t *uctx);
ucontext_t *cur_context;            /* a pointer to the current_context */
int curcontext = 0;
ucontext_t contexts[NUMCONTEXTS];   /* store our context info */

struct acctest_context *dsa = NULL;
struct hw_desc *descs = NULL;
struct hw_desc *cur_desc = NULL;
int curdesc = 0;

char *src_bufs[NUM_BUFS];
char *dst_bufs[NUM_BUFS];
int curbuf = 0;

void
mkcontext(ucontext_t *uc,  void *function)
{
    void * stack;

    getcontext(uc);

    stack = malloc(STACKSIZE);
    if (stack == NULL) {
        perror("malloc");
        exit(1);
    }
    /* we need to initialize the ucontext structure, give it a stack,
        flags, and a sigmask */
    uc->uc_stack.ss_sp = stack;
    uc->uc_stack.ss_size = STACKSIZE;
    uc->uc_stack.ss_flags = 0;
    if (sigemptyset(&uc->uc_sigmask) < 0){
      perror("sigemptyset");
      exit(1);
    }

    /* setup the function we're going to, and n-1 arguments. */
    makecontext(uc, function, 1);

    PRINT_DBG("context is %p\n", uc);
}

void
scheduler()
{
    PRINT_DBG("scheduling out thread %d\n", curcontext);

    curcontext = (curcontext + 1) % NUMCONTEXTS; /* round robin */
    cur_context = &contexts[curcontext];

    PRINT_DBG("scheduling in thread %d\n", curcontext);

    setcontext(cur_context); /* go */
}

void worker_fn(){
  /* busy-while loop polls for responses and swaps the context */

}

void request_fn(){
  /* ROCKSDB SCAN */
  for(int i=0; i<BUF_SIZE; i++)
    src_bufs[curbuf][i] = (char)i;


  /* Prepare, Submit request and yield */
  struct hw_desc *mDesc = &(descs[curdesc]);
  mDesc->src_addr = (uint64_t)(src_bufs[curbuf]);
  mDesc->dst_addr = (uint64_t)(dst_bufs[curbuf]);
  mDesc->xfer_size = BUF_SIZE;

  while(enqcmd(dsa->wq_reg, mDesc)){ }

  /* Re-entry point */
  curbuf = (curbuf + 1) % NUM_BUFS;

}

void requestGen(){
  /* Generate pre-processed Requests to feed to the worker */
}


int main(){

  /* The requests all have access to the same accel */

  int tflags = TEST_FLAGS_BOF;
  int rc;
  int wq_type = ACCFG_WQ_SHARED;
  int dev_id = 0;
  int wq_id = 0;
  int opcode = 16;
  uint32_t dflags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;

  dsa = acctest_init(tflags);
  dsa->dev_type = ACCFG_DEVICE_DSA;
  if (!dsa)
		return -ENOMEM;

  rc = acctest_alloc(dsa, wq_type, dev_id, wq_id);
	if (rc < 0)
		return -ENOMEM;


  /* Allocate the request set we will use */
  descs = malloc(sizeof(struct hw_desc) * NUM_DESCS);

  /* Pre-prepare them */
  for(int i=0; i<NUM_DESCS; i++){
    acctest_prep_desc_common(&(descs[i]), opcode, 0x0, 0x0, 0, dflags);
  }

  mkcontext(&contexts[1], request_fn);
  setcontext(&contexts[1]);

exit:

  return 0;
}