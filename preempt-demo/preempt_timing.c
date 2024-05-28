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
  /* application work */
  for(int i=0; i<BUF_SIZE; i++)
    src_bufs[curbuf][i] = (char)i;


  /* Prepare, Submit request and yield */

  __builtin_ia32_sfence();

  /* Re-entry point */
  for(int i=0; i<BUF_SIZE; i++)
    if (src_bufs[curbuf][i] != dst_bufs[curbuf][i]){ PRINT_ERR("bad copy\n"); exit(-1); }

  curbuf = (curbuf + 1) % NUM_BUFS;

}

void requestGen(){
  /* Generate pre-processed Requests to feed to the worker */
}


int main(){

  /* The requests all have access to the same accel */
  struct acctest_context *dsa = NULL;
  allocDsa(&dsa);

  /* Allocate the buffers we will use */
  for(int i=0; i<NUM_BUFS; i++){
    src_bufs[i] = malloc(BUF_SIZE);
    dst_bufs[i] = malloc(BUF_SIZE);
  }

  /* Allocate the completion records */



  mkcontext(&contexts[1], request_fn);

  cur_context = &(contexts[1]);
  setcontext(&contexts[1]);

exit:

  return 0;
}