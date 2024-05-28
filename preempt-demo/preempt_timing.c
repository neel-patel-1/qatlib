#include <stdio.h>
#include <ucontext.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>

#include "dsa_funcs.h"

int gDebugParam = 1;

#define NUMCONTEXTS 10              /* how many contexts to make */
#define STACKSIZE 4096
#define NUM_DESCS 128 /* Size of the shared work queue */
#define NUM_COMPS NUM_DESCS
#define NUM_BUFS 128
#define BUF_SIZE 4096

extern int swapcontext_very_fast(ucontext_t *ouctx, ucontext_t *uctx);
ucontext_t *cur_context;            /* a pointer to the current_context */
int curcontext = 0;
ucontext_t contexts[NUMCONTEXTS];   /* store our context info */

char *src_bufs[NUM_BUFS];
char *dst_bufs[NUM_BUFS];
int curbuf = 0;
int buf_size = BUF_SIZE;

struct task_node *sched_tsk_node;
struct task_node *req_tsk_node;

struct acctest_context *dsa = NULL;
const volatile uint8_t *comp;
struct hw_desc *hw;

/* doorbell */
_Atomic bool pendingContext;

void worker_func(){
  PRINT_DBG("Scheduling Pending context\n");
}

void request_func(){
  for(int i=0; i<buf_size; i++){
    src_bufs[curbuf][i] = (char)i;
  }

  uint64_t prePrep = sampleCoderdtsc();

  prepare_memcpy_task(req_tsk_node->tsk, dsa, (src_bufs[0]), buf_size, dst_bufs[0]);
  hw = req_tsk_node->tsk->desc;

  uint64_t postPrep = sampleCoderdtsc();

  if( enqcmd(dsa->wq_reg, hw) ){PRINT_ERR("Failure in enq\n"); exit(-1);};
  comp = (uint8_t *)req_tsk_node->tsk->comp;

  uint64_t postSubmit = sampleCoderdtsc();

  PRINT_DBG("Request Context Yielding\n");
  setcontext(&contexts[1]);

  // swapcontext_very_fast()

  uint64_t postYield = sampleCoderdtsc();



}


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



int main(){

  /* The requests all have access to the same accel */
  int tflags = TEST_FLAGS_BOF;
  int numDescs = NUM_DESCS;

  struct hw_desc *hw = NULL;
  struct task_node *tsk_node = NULL;

  allocDsa(&dsa);
  allocTasks(dsa, &tsk_node, DSA_OPCODE_MEMMOVE, buf_size, tflags, numDescs);

  req_tsk_node = tsk_node;
  sched_tsk_node = tsk_node;

  /* Allocate the buffers we will use */
  for(int i=0; i<NUM_BUFS; i++){
    src_bufs[i] = malloc(BUF_SIZE);
    dst_bufs[i] = malloc(BUF_SIZE);
  }

  mkcontext(&contexts[0], request_func);
  mkcontext(&contexts[1], worker_func );
  setcontext(&contexts[0]);

  comp = (uint8_t *)sched_tsk_node->tsk->comp;
  while(*comp == 0){}
  if(memcmp(src_bufs[0], dst_bufs[0], buf_size) != 0){
    PRINT_ERR("Failed copy\n");
  }

  // cur_context = &(contexts[1]);
  // setcontext(&contexts[1]);

exit:

  return 0;
}